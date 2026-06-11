#!/usr/bin/env node
/* apps/showcase/tools/capture.mjs — capture-once harness (ENC-549 / T6.1)
 *
 *   node apps/showcase/tools/capture.mjs <view-id> [--duration 20000] [--gma-port N] [--data-port N]
 *
 * Runs the REAL faithful pipeline once for a single view and freezes its
 * dataplane output to `apps/showcase/views/<view-id>/records.json`:
 *
 *   cmd/mock-gma  ──ws──▶  forum-less cmd/embassy  ──dataplane /data ws──▶  this client
 *
 * Both binaries are built from the embassy MAIN checkout (see EMBASSY_REPO
 * below); we never modify embassy. The harness:
 *   1. builds mock-gma + embassy (go build, cached) if the bins are missing/stale,
 *   2. spawns mock-gma, then forum-less embassy with the view's instruction.json,
 *   3. waits for embassy's `orchestrator.apply.ok` + `embassy.gma.subscribe.sent`,
 *   4. connects a WS client to the dataplane (/data) and records every BINARY
 *      frame with a relative timestamp for ~duration ms (text frames — embassy's
 *      own scene-init — are ignored, mirroring useReplay/useAgentStream),
 *   5. writes records.json: { meta:{viewId,durationMs,frameCount,cadenceMs}, frames:[{t,b64}] },
 *   6. tears the two processes down by PID (SIGTERM → SIGKILL), never a broad pkill.
 *
 * Node 22's built-in global `WebSocket` is the dataplane client — no `ws` npm
 * dependency. Run from the showcase app dir or anywhere (paths are resolved
 * relative to this file).
 */

import { spawn, spawnSync } from 'node:child_process';
import { mkdirSync, existsSync, statSync, writeFileSync, readFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
// apps/showcase/tools -> apps/showcase
const SHOWCASE_DIR = resolve(__dirname, '..');
const VIEWS_DIR = join(SHOWCASE_DIR, 'views');

// The embassy MAIN checkout (mock-gma + embassy build from here; never modified).
const EMBASSY_REPO = process.env.EMBASSY_REPO || '/home/ndrandal/workspace/Github/embassy';
const BIN_DIR = process.env.KEYSTONE_BIN_DIR || '/tmp/keystone-bin';

// ---- args ----
const args = process.argv.slice(2);
const viewId = args.find((a) => !a.startsWith('--'));
function flag(name, def) {
  const i = args.indexOf(`--${name}`);
  return i >= 0 && args[i + 1] ? args[i + 1] : def;
}
if (!viewId) {
  console.error('usage: node capture.mjs <view-id> [--duration ms] [--gma-port N] [--data-port N] [--cadence ms]');
  process.exit(2);
}

const DURATION_MS = Number(flag('duration', '20000'));
const CADENCE_MS = Number(flag('cadence', '75'));
const GMA_PORT = Number(flag('gma-port', '41010'));
const DATA_PORT = Number(flag('data-port', '41011'));
const ADMIN_PORT = Number(flag('admin-port', '41012'));
// Stream a little longer than we record so the feed never dries up mid-capture.
const FEED_DURATION_MS = DURATION_MS + 10000;

const viewDir = join(VIEWS_DIR, viewId);
const instructionPath = join(viewDir, 'instruction.json');
const recordsPath = join(viewDir, 'records.json');

if (!existsSync(instructionPath)) {
  console.error(`[capture] no instruction.json for view "${viewId}" at ${instructionPath}`);
  process.exit(2);
}

const log = (...m) => console.error('[capture]', ...m);

// ---- build the binaries (cached) ----
function goBin() {
  // Prefer the user-local Go install on PATH; fall back to ~/.local/go/bin.
  return existsSync(`${process.env.HOME}/.local/go/bin/go`)
    ? `${process.env.HOME}/.local/go/bin/go`
    : 'go';
}

function buildIfStale(cmdPkg, outName) {
  const out = join(BIN_DIR, outName);
  mkdirSync(BIN_DIR, { recursive: true });
  // Rebuild if the binary is missing — cheap incremental otherwise. We always
  // run `go build` (it is a no-op when nothing changed) to pick up embassy main
  // updates, but only the first run pays the compile cost.
  log(`building ${outName} from ${EMBASSY_REPO}/${cmdPkg}…`);
  const r = spawnSync(goBin(), ['build', '-o', out, `./${cmdPkg}`], {
    cwd: EMBASSY_REPO,
    stdio: ['ignore', 'inherit', 'inherit'],
    env: process.env,
  });
  if (r.status !== 0) {
    console.error(`[capture] go build ./${cmdPkg} failed (status ${r.status})`);
    process.exit(1);
  }
  if (!existsSync(out)) {
    console.error(`[capture] expected binary missing after build: ${out}`);
    process.exit(1);
  }
  return out;
}

const mockBin = buildIfStale('cmd/mock-gma', 'mock-gma');
const embassyBin = buildIfStale('cmd/embassy', 'embassy');

// ---- process bookkeeping (teardown by PID, never pkill) ----
/** @type {import('node:child_process').ChildProcess[]} */
const children = [];
let tornDown = false;

function teardown() {
  if (tornDown) return;
  tornDown = true;
  for (const c of children) {
    if (c.exitCode === null && c.signalCode === null) {
      try { c.kill('SIGTERM'); } catch { /* already gone */ }
    }
  }
  // Hard-kill anything still alive shortly after.
  setTimeout(() => {
    for (const c of children) {
      if (c.exitCode === null && c.signalCode === null) {
        try { c.kill('SIGKILL'); } catch { /* already gone */ }
      }
    }
  }, 1500).unref();
}

process.on('exit', teardown);
process.on('SIGINT', () => { teardown(); process.exit(130); });
process.on('SIGTERM', () => { teardown(); process.exit(143); });

function spawnLogged(name, bin, argv, env) {
  const child = spawn(bin, argv, { env, stdio: ['ignore', 'pipe', 'pipe'] });
  children.push(child);
  const onData = (buf) => {
    for (const line of buf.toString().split('\n')) {
      if (line.trim()) emit(name, line);
    }
  };
  child.stdout.on('data', onData);
  child.stderr.on('data', onData);
  child.on('exit', (code, sig) => log(`${name} exited (code=${code} sig=${sig})`));
  return child;
}

// Per-process log line fan-out so waiters can match markers.
const logWaiters = [];
function emit(name, line) {
  // Surface embassy's important control-path markers; keep the rest quiet.
  if (/orchestrator\.apply\.ok|subscribe\.sent|subscribe\.failed|dataplane up|gma client up|error/i.test(line)) {
    log(`${name}: ${line}`);
  }
  for (const w of logWaiters) w(name, line);
}

function waitForMarkers(markers, timeoutMs) {
  const remaining = new Set(markers);
  return new Promise((res, rej) => {
    const to = setTimeout(() => {
      logWaiters.splice(logWaiters.indexOf(waiter), 1);
      rej(new Error(`timed out waiting for embassy markers: ${[...remaining].join(', ')}`));
    }, timeoutMs);
    const waiter = (_name, line) => {
      for (const m of [...remaining]) if (line.includes(m)) remaining.delete(m);
      if (remaining.size === 0) {
        clearTimeout(to);
        logWaiters.splice(logWaiters.indexOf(waiter), 1);
        res();
      }
    };
    logWaiters.push(waiter);
  });
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function main() {
  // 1. mock-gma
  log(`starting mock-gma on :${GMA_PORT} (duration=${FEED_DURATION_MS}ms cadence=${CADENCE_MS}ms)`);
  spawnLogged('mock-gma', mockBin, [
    '--addr', `:${GMA_PORT}`,
    '--duration', `${FEED_DURATION_MS}ms`,
    '--cadence', `${CADENCE_MS}ms`,
    '--seed', '1',
  ], process.env);
  await sleep(800); // let the listener bind

  // 2. forum-less embassy with the view's instruction file
  log(`starting embassy: GMA=ws://localhost:${GMA_PORT} DATA=:${DATA_PORT} instruction=${instructionPath}`);
  const embEnv = {
    ...process.env,
    EMBASSY_GMA_WS_URL: `ws://localhost:${GMA_PORT}`,
    EMBASSY_FORUM_URL: '',
    EMBASSY_INSTRUCTION_FILE: instructionPath,
    EMBASSY_DATA_ADDR: `:${DATA_PORT}`,
    EMBASSY_ADMIN_ADDR: `127.0.0.1:${ADMIN_PORT}`,
  };
  spawnLogged('embassy', embassyBin, [], embEnv);

  // 3. wait for the control path to be wired + subscribed
  log('waiting for orchestrator.apply.ok + embassy.gma.subscribe.sent…');
  await waitForMarkers(['orchestrator.apply.ok', 'embassy.gma.subscribe.sent'], 15000);
  log('embassy ready — subscriptions sent. connecting dataplane client.');
  await sleep(300);

  // 4. connect the dataplane WS client and record binary frames
  const dataURL = `ws://localhost:${DATA_PORT}/data`;
  /** @type {{t:number,b64:string}[]} */
  const frames = [];
  let t0 = 0;

  await new Promise((res, rej) => {
    const ws = new WebSocket(dataURL);
    ws.binaryType = 'arraybuffer';
    let recordTimer = null;
    let firstBinary = true;

    const finish = () => {
      try { ws.close(); } catch { /* noop */ }
      res();
    };

    ws.addEventListener('open', () => {
      log(`dataplane connected: ${dataURL} — recording ${DURATION_MS}ms of binary frames`);
    });

    ws.addEventListener('message', (ev) => {
      // Text frames are embassy's own scene-init / control — IGNORE them
      // (the showcase applies its own manifest; replay only needs binary).
      if (typeof ev.data === 'string') return;
      if (!(ev.data instanceof ArrayBuffer)) return;
      const now = performance.now();
      if (firstBinary) {
        firstBinary = false;
        t0 = now;
        // Start the record window from the FIRST binary frame so the timeline
        // is relative and tight (no leading dead time before data lands).
        recordTimer = setTimeout(finish, DURATION_MS);
        recordTimer.unref?.();
      }
      const b64 = Buffer.from(ev.data).toString('base64');
      frames.push({ t: Math.round(now - t0), b64 });
    });

    ws.addEventListener('error', (e) => {
      rej(new Error(`dataplane WS error: ${e?.message ?? e}`));
    });

    ws.addEventListener('close', () => {
      if (recordTimer) clearTimeout(recordTimer);
      // If the socket closed before we hit the timer, finish with what we have.
      res();
    });

    // Safety cap: never hang past duration + slack even if no frames arrive.
    setTimeout(() => finish(), DURATION_MS + 8000).unref?.();
  });

  if (frames.length === 0) {
    console.error('[capture] NO binary frames recorded — pipeline produced no dataplane output.');
    teardown();
    process.exit(1);
  }

  const durationMs = frames.length ? frames[frames.length - 1].t : 0;
  const out = {
    meta: {
      viewId,
      durationMs,
      frameCount: frames.length,
      cadenceMs: CADENCE_MS,
    },
    frames,
  };
  writeFileSync(recordsPath, JSON.stringify(out));
  const sizeKb = Math.round(statSync(recordsPath).size / 1024);
  log(`wrote ${recordsPath} — frameCount=${frames.length} durationMs=${durationMs} size=${sizeKb}KB`);

  teardown();
  // Give SIGTERM a beat to land, then exit.
  await sleep(400);
  process.exit(0);
}

main().catch((err) => {
  console.error('[capture] FAILED:', err?.stack ?? err);
  teardown();
  process.exit(1);
});
