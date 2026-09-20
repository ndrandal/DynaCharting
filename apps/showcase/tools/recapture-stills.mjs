#!/usr/bin/env node
/* apps/showcase/tools/recapture-stills.mjs — recapture the showcase gallery (ENC-1288)
 *
 *   node apps/showcase/tools/recapture-stills.mjs
 *     [--url http://localhost:5650/] [--vite-port 5650] [--port 9530]
 *     [--views "a b c"] [--outdir <showcase>/stills] [--contact-sheet-only]
 *
 * WHY THIS FILE EXISTS, AND WHY IT IS NOT `snap-stills.mjs`.
 *
 * Every PNG in `apps/showcase/stills/` was captured in one commit — 537c995,
 * 2026-06-11 — and the EngineHost blit fix that put Y the right way up landed at
 * d6b5acd, 2026-06-21, ten days later. Every one was therefore a vertically
 * MIRRORED picture of a render that was itself correct, and nothing in the repo
 * said so for three months (LIMITATIONS.md DC-L15). SPEC §1.1 read
 * `price-line-area.png` as "the area is filled on the wrong side of the line"
 * and scored the view a tier-0 FAIL off it; it took ENC-1250 and ENC-1276 to
 * unwind. A mirrored random walk still looks like a random walk, so orientation
 * was never going to be caught by eye.
 *
 * `snap-stills.mjs` is the tool that produced them, and it has two properties
 * this recapture may not inherit:
 *
 *   1. it screenshots `.single-canvas-region` — the engine canvas PLUS the
 *      DOM/SVG chrome overlay composited over it. A DOM overlay in a still can
 *      pass a tier-1 check on the engine's behalf, which is exactly what
 *      concealed the missing engine axis for six months (SPEC §1.3, D10);
 *   2. it never names the adapter, so a SwiftShader frame and an NVIDIA frame
 *      are indistinguishable after the fact (SPEC D8).
 *
 * So the capture moves to the chart-quality harness's `shoot-live.mjs`, which
 * already does both correctly and has had two independent bugs beaten out of it:
 *
 *   - canvas-only by default: `canvas.toDataURL('image/png')` on a named
 *     selector, stamped with `captureMode`/`captureDetail` tEXt chunks INSIDE
 *     the PNG plus a sidecar `<out>.capture.json` (ENC-1264);
 *   - the adapter is probed once, printed, RECORDED, and ENFORCED — and it reads
 *     `adapter.info.isFallbackAdapter`, because `GPUAdapter.isFallbackAdapter`
 *     does not exist on Chromium 1228 and `!!adapter.isFallbackAdapter` is
 *     therefore false on a software adapter too (ENC-1263). A software adapter
 *     aborts the shot rather than producing a still that looks hardware-rendered.
 *
 * Re-implementing either of those here would mean re-earning both bugs. The
 * other candidate, `capture.mjs`, is not a still harness at all: it captures
 * `records.json` (the embassy dataplane tape) and needs an embassy checkout at a
 * path that does not exist on this machine. The tapes are already committed, so
 * nothing about a recapture needs it.
 *
 * ── THREE THINGS THIS SCRIPT IS CAREFUL ABOUT ───────────────────────────────
 *
 * 1. EVERY VIEW IS ENTERED THROUGH THE HERO ROUTE `#/`, NEVER DEEP-LINKED.
 *    A cold deep link to `#/view/<id>` takes the whole app down for 11 of the 22
 *    views — `PlotBoxError: horizontal insets (64+16) leave no plot box in 1px`,
 *    thrown out of a passive effect with no boundary above it (measured by
 *    ENC-1262; ENC-1313 narrowed but did not remove it). Entering from `#/`
 *    mounts the canvas at its real size first.
 *
 * 2. THE FRAME IS TAKEN AT A KNOWN POINT IN THE REPLAY, NOT AFTER A FIXED WAIT.
 *    `snap-stills.mjs` waited 10s of a ~20s loop and hoped. Measured 2026-09-20,
 *    a 40s fixed dwell landed `price-line-area` 30 pixel-columns into a fresh
 *    loop — 30 of 267 records drawn, and `still-orientation.py` could not fit it
 *    at all. So the readiness predicate watches the app's OWN transport scrubber
 *    (`[role=slider][aria-label="Replay position"]`, `aria-valuenow`) and waits
 *    for a full pass: progress seen at/below 5% and then at/above 92%. A view
 *    whose replay never completes a pass fails with exit 3 rather than silently
 *    yielding an early frame.
 *
 * 3. ONE CHROME PER VIEW, KILLED BY PID. The engine is one long-lived EngineHost
 *    portaled between routes and a view change re-applies a manifest over a live
 *    scene, so a fresh browser per view means no view is captured through
 *    another's residue. `pkill -f` on a port pattern matches the shell's own
 *    command line; only a saved PID is safe.
 *
 * OUTPUTS (all under --outdir):
 *   <id>.png                  canvas-only raster, tEXt-stamped
 *   <id>.png.capture.json     shoot-live's per-shot manifest (adapter, sha256, …)
 *   <id>.png.probe.json       what the page said it had drawn, same eval as the shot
 *   capture-manifest.json     one row per still: adapter, sha256, HEAD, progress
 *   render-tally.json         per-view render verdict, from the in-capture sample
 *   contact-sheet.html        the gallery page
 *   contact-sheet.png         a page shot OF THAT PAGE (a photo of a document —
 *                             it is not evidence about any renderer and says so)
 */

import { spawn } from 'node:child_process';
import { execFileSync } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync, readdirSync, existsSync, mkdtempSync, rmSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SHOWCASE_DIR = resolve(__dirname, '..');       // apps/showcase
const REPO = resolve(SHOWCASE_DIR, '..', '..');      // DynaCharting
const VIEWS_DIR = join(SHOWCASE_DIR, 'views');

const args = process.argv.slice(2);
const flag = (n, d) => {
  const i = args.indexOf(`--${n}`);
  return i >= 0 && args[i + 1] && !args[i + 1].startsWith('--') ? args[i + 1] : d;
};
const has = (n) => args.includes(`--${n}`);

const VITE_PORT = flag('vite-port', '5650');
const URL_BASE = flag('url', `http://localhost:${VITE_PORT}/`);
const CDP_PORT = flag('port', '9530');
const OUTDIR = resolve(flag('outdir', join(SHOWCASE_DIR, 'stills')));
const TIMEOUT = Number(flag('timeout', '180000'));
const CONTACT_ONLY = has('contact-sheet-only');

/* shoot-live.mjs lives in the WORKSPACE repo (the chart-quality-bar proposal
 * directory), which is a sibling of this one. Walk up for it rather than
 * counting `..`s: run from a worktree, DynaCharting sits at
 * trees/<group>/ENC-nnnn-dynacharting and the specs dir is two levels above
 * that, not three. Take the OUTERMOST match — a project group dir holds wt.mjs's
 * pinned sibling checkouts, and one of those would be a stale copy. */
function findShootLive() {
  if (process.env.SHOOT_LIVE) return process.env.SHOOT_LIVE;
  const rel = join('specs', '2026-09-19-chart-quality-bar', 'harness', 'shoot-live.mjs');
  let found = null;
  let d = REPO;
  while (d !== '/') {
    const p = join(d, rel);
    if (existsSync(p)) found = p;
    d = dirname(d);
  }
  return found;
}

const SHOOT_LIVE = findShootLive();
const CHROMEDIR = process.env.CHROMEDIR ||
  join(process.env.HOME || '', '.cache', 'ms-playwright', 'chromium-1228', 'chrome-linux64');
const CHROME = join(CHROMEDIR, 'chrome');

/* All three flags are required for a hardware adapter on this box; without them
 * Chrome silently picks SwiftShader and shoot-live refuses the shot (exit 4).
 * `--allow-software` is NEVER passed from here — a gallery of software frames is
 * precisely the sort of artifact DC-L15 is about. */
const CHROME_ARGS = [
  '--headless=new',
  '--enable-unsafe-webgpu',
  '--enable-features=Vulkan',
  '--ignore-gpu-blocklist',
  '--use-angle=vulkan',
  '--no-sandbox',
  '--disable-gpu-sandbox',
];

const TIER_RANK = { native: 0, composed: 1, walled: 2 };

function discoverViews() {
  const ids = readdirSync(VIEWS_DIR).filter((d) => existsSync(join(VIEWS_DIR, d, 'view.json')));
  const views = ids.map((id) => {
    const meta = JSON.parse(readFileSync(join(VIEWS_DIR, id, 'view.json'), 'utf8'));
    return { id: meta.id, title: meta.title, tier: meta.tier, referenceTool: meta.referenceTool };
  });
  views.sort((a, b) => (TIER_RANK[a.tier] - TIER_RANK[b.tier]) || a.title.localeCompare(b.title));
  return views;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const cdpUp = async (port) => {
  try {
    const r = await fetch(`http://localhost:${port}/json/version`);
    return r.ok;
  } catch { return false; }
};

/* The readiness predicate. See note 2 in the header: it must observe a FULL
 * replay pass, not a wall-clock delay. It accumulates the minimum progress it
 * has seen in a page global, so "we are near the end of a pass that we watched
 * start" is decidable from a stateless poll. */
const untilExpr = () => `(() => {
  const el = document.querySelector('[role="slider"][aria-label="Replay position"]');
  const c = document.querySelector('canvas.engine-canvas');
  if (!el || !c || c.width <= 16 || c.height <= 16) return false;
  const p = Number(el.getAttribute('aria-valuenow'));
  if (!Number.isFinite(p)) return false;
  window.__dcRecapMin = Math.min(window.__dcRecapMin ?? 100, p);
  return window.__dcRecapMin <= 5 && p >= 92;
})()`;

/* Switch to the view through the app's own router, from the hero route. */
const execExpr = (id) => `(async () => {
  await new Promise((r) => setTimeout(r, 6000));
  location.hash = ${JSON.stringify('#/view/' + id)};
  await new Promise((r) => setTimeout(r, 1500));
  window.__dcRecapMin = undefined;
  return location.hash;
})()`;

/* What the page is asked, in the SAME eval as the capture. The render verdict in
 * render-tally.json is computed from this sample rather than from a second,
 * later read: these views loop, so a read taken after the shot describes a
 * different frame. The DOM counts are here so "the raster contains no overlay"
 * is an observation rather than an assumption. */
const probeExpr = (id) => `(() => {
  const out = {
    view: ${JSON.stringify(id)},
    axis: (window.__dcEngineAxis || {})[${JSON.stringify(id)}] || null,
    domain: (window.__dcAxisDomain || {})[${JSON.stringify(id)}] || null,
    boundaryErrors: (window.__dcBoundaryErrors || []).length,
    dom: {
      overlaySvgs: document.querySelectorAll('.chrome-overlay svg').length,
      overlaySvgText: document.querySelectorAll('.chrome-overlay svg text').length,
      overlayNodes: document.querySelectorAll('.chrome-overlay *').length
    },
    progress: (() => {
      const el = document.querySelector('[role="slider"][aria-label="Replay position"]');
      return el ? Number(el.getAttribute('aria-valuenow')) : null;
    })()
  };
  const c = document.querySelector('canvas.engine-canvas');
  if (!c || !c.width || !c.height) { out.pix = { ok: false, reason: 'no canvas' }; return out; }
  const off = document.createElement('canvas');
  off.width = c.width; off.height = c.height;
  const ctx = off.getContext('2d');
  ctx.drawImage(c, 0, 0);
  let data;
  try { data = ctx.getImageData(0, 0, off.width, off.height).data; }
  catch (e) { out.pix = { ok: false, reason: 'readback blocked: ' + (e && e.message) }; return out; }
  const colors = new Set();
  let sampled = 0, nonbg = 0, chroma = 0;
  const stride = 4 * 11;
  for (let i = 0; i < data.length; i += stride) {
    sampled++;
    const r = data[i], g = data[i + 1], b = data[i + 2];
    if (sampled % 7 === 0) colors.add(r + ',' + g + ',' + b);
    const dClear = Math.abs(r - 8) + Math.abs(g - 8) + Math.abs(b - 13);
    const dApp = Math.abs(r - 13) + Math.abs(g - 13) + Math.abs(b - 20);
    if (Math.min(dClear, dApp) > 36) nonbg++;
    const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
    if (mx - mn > 22 && mx > 30) chroma++;
  }
  out.pix = {
    ok: true, w: c.width, h: c.height,
    distinctColors: colors.size,
    coverage: sampled ? nonbg / sampled : 0,
    chroma: sampled ? chroma / sampled : 0,
    nonbgPixels: nonbg
  };
  return out;
})()`;

/* Unchanged from snap-stills.mjs: coverage alone under-reports thin vector
 * geometry, so a colour-saturation signal rides alongside it. */
function classify(pix) {
  if (!pix || !pix.ok) return 'none';
  const cov = pix.coverage ?? 0;
  const chroma = pix.chroma ?? 0;
  const colors = pix.distinctColors ?? 0;
  if (cov < 0.004 && chroma < 0.004) return 'none';
  if (cov >= 0.04 || chroma >= 0.02 || (colors >= 8 && (cov >= 0.015 || chroma >= 0.01))) return 'full';
  return 'partial';
}

function run(cmd, argv, opts = {}) {
  return new Promise((res) => {
    const p = spawn(cmd, argv, { stdio: ['ignore', 'inherit', 'inherit'], ...opts });
    p.on('exit', (code) => res(code ?? 1));
  });
}

async function shootOne(view) {
  const profile = mkdtempSync(join(tmpdir(), `ENC-1288-${view.id}-`));
  const out = join(OUTDIR, `${view.id}.png`);

  if (await cdpUp(CDP_PORT)) {
    console.error(`FATAL something already answers CDP on ${CDP_PORT}; refusing to drive ` +
      `another session's browser. Pass --port <n>.`);
    process.exit(2);
  }

  const chrome = spawn(CHROME, [
    ...CHROME_ARGS,
    `--remote-debugging-port=${CDP_PORT}`,
    '--window-size=1280,800',
    `--user-data-dir=${join(profile, 'profile')}`,
    'about:blank',
  ], {
    stdio: 'ignore',
    detached: false,
    env: { ...process.env, DISPLAY: '', WAYLAND_DISPLAY: '' },
  });

  try {
    for (let i = 0; i < 40; i++) { if (await cdpUp(CDP_PORT)) break; await sleep(500); }
    const code = await run(process.execPath, [
      SHOOT_LIVE,
      '--port', String(CDP_PORT),
      // ?svgAxis=0 goes BEFORE the hash — routing is hash-based. It is the only
      // configuration in which the app's "no DOM/SVG axis overlay" claim is true
      // (ENC-1253); canvas-only capture makes it moot for the raster, but the
      // probe's DOM counts are then a statement about the same page the sweep
      // scored.
      '--url', `${URL_BASE}?svgAxis=0#/`,
      '--exec', execExpr(view.id),
      '--until', untilExpr(),
      '--canvas', 'canvas.engine-canvas',
      '--mode', 'canvas',
      '--dwell', '0',
      '--timeout', String(TIMEOUT),
      '--out', out,
      '--probe', probeExpr(view.id),
    ]);
    return { code, out };
  } finally {
    try { chrome.kill('SIGTERM'); } catch { /* already gone */ }
    await sleep(300);
    try { chrome.kill('SIGKILL'); } catch { /* already gone */ }
    for (let i = 0; i < 20; i++) { if (!(await cdpUp(CDP_PORT))) break; await sleep(500); }
    try { rmSync(profile, { recursive: true, force: true }); } catch { /* best effort */ }
  }
}

const VERDICT_COLOR = { full: '#3ddc84', partial: '#f5b14c', none: '#e5534b' };
const TIER_COLOR = { native: '#3ddc84', composed: '#4c9bf5', walled: '#f5b14c' };

function contactSheet(results, head, adapterLine) {
  const cards = results.map((r) => {
    const tc = TIER_COLOR[r.tier] || '#888';
    const vc = VERDICT_COLOR[r.verdict] || '#888';
    const img = r.still
      ? `<img src="./${r.id}.png" alt="${r.title}" loading="lazy" />`
      : `<div class="missing">no still</div>`;
    const cov = r.pix && r.pix.coverage != null ? (r.pix.coverage * 100).toFixed(1) + '%' : '—';
    return `    <figure class="card" data-tier="${r.tier}" data-verdict="${r.verdict}">
      <div class="frame">${img}</div>
      <figcaption>
        <div class="row">
          <span class="title">${r.title}</span>
          <span class="badge tier" style="--c:${tc}">${r.tier}</span>
        </div>
        <div class="row sub">
          <span class="ref">${r.referenceTool || ''}</span>
          <span class="badge verdict" style="--c:${vc}">${r.verdict}</span>
        </div>
        <div class="row meta"><code>${r.id}</code><span>coverage ${cov}</span></div>
      </figcaption>
    </figure>`;
  }).join('\n');

  const tally = results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {});
  const total = results.length;

  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>DynaCharting · Frontier — Contact Sheet (${total} views)</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin: 0; background: #0a0a12; color: #e8e8f0;
         font: 14px/1.5 ui-sans-serif, system-ui, -apple-system, sans-serif; }
  header { padding: 28px 32px 12px; border-bottom: 1px solid #20202e; }
  h1 { margin: 0 0 6px; font-size: 22px; font-weight: 650; letter-spacing: -0.01em; }
  .lede { margin: 0; color: #9a9ab0; max-width: 78ch; }
  .prov { margin: 10px 0 0; color: #6f6f86; font-size: 12.5px; max-width: 90ch; }
  .prov code { color: #8a8aff; }
  .tally { margin: 14px 0 0; display: flex; gap: 10px; flex-wrap: wrap; }
  .tally .chip { padding: 4px 12px; border-radius: 999px; font-size: 13px;
                 border: 1px solid var(--c); color: var(--c); }
  .grid { display: grid; gap: 18px; padding: 24px 32px 56px;
          grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); }
  .card { margin: 0; background: #11111c; border: 1px solid #20202e;
          border-radius: 12px; overflow: hidden; }
  .frame { aspect-ratio: 11 / 7; background: #05050a; display: flex;
           align-items: center; justify-content: center; }
  .frame img { width: 100%; height: 100%; object-fit: contain; display: block; }
  .missing { color: #e5534b; font-size: 13px; }
  figcaption { padding: 10px 12px 12px; }
  .row { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
  .row.sub { margin-top: 3px; }
  .row.meta { margin-top: 8px; color: #6f6f86; font-size: 12px; }
  .title { font-weight: 600; }
  .ref { color: #8a8aa0; font-size: 12.5px; }
  code { color: #8a8aff; font-size: 12px; }
  .badge { padding: 1px 9px; border-radius: 999px; font-size: 11px; font-weight: 600;
           text-transform: uppercase; letter-spacing: 0.03em;
           color: var(--c); border: 1px solid var(--c); }
</style>
</head>
<body>
<header>
  <h1>DynaCharting · Frontier — Contact Sheet</h1>
  <p class="lede">Every showcase view, captured live from the built app through a faithful data path and a real WebGPU renderer. Tiers: <b style="color:${TIER_COLOR.native}">native</b> (rendered straight from the manifest), <b style="color:${TIER_COLOR.composed}">composed</b> (manifest + upstream precompute), <b style="color:${TIER_COLOR.walled}">walled</b> (precomputed output; live-GPU compute is the frontier).</p>
  <p class="prov"><b>Provenance.</b> Canvas-only — each frame is <code>canvas.engine-canvas</code>'s own pixels via <code>toDataURL('image/png')</code>, so no DOM or SVG chrome is composited into any image here. ${adapterLine} DynaCharting <code>${head}</code>. Per-still adapter, sha256 and replay position: <code>capture-manifest.json</code>. Regenerate with <code>apps/showcase/tools/recapture-stills.mjs</code> (ENC-1288).</p>
  <div class="tally">
    <span class="chip" style="--c:${VERDICT_COLOR.full}">fully rendered ${tally.full || 0}/${total}</span>
    <span class="chip" style="--c:${VERDICT_COLOR.partial}">partial ${tally.partial || 0}/${total}</span>
    <span class="chip" style="--c:${VERDICT_COLOR.none}">none ${tally.none || 0}/${total}</span>
  </div>
</header>
<main class="grid">
${cards}
</main>
</body>
</html>
`;
}

/* The contact sheet PNG is a photograph of a DOCUMENT — an HTML page of already-
 * captured stills. It is the one page screenshot in this directory, and it is
 * not evidence about any renderer; the per-view PNGs beside it are. It is taken
 * with `--mode page`, which stamps `captureMode=page` and `tier1Scorable:false`
 * into the file, so it can never be mistaken for one of them. */
const SHEET_WIDTH = 1958;

/* One pass of the contact sheet: launch a headless Chrome at a given window
 * height and let shoot-live take a `page` shot of the local HTML file. */
async function sheetPass(height, out, probe) {
  const htmlPath = join(OUTDIR, 'contact-sheet.html');
  const profile = mkdtempSync(join(tmpdir(), 'ENC-1288-sheet-'));
  if (await cdpUp(CDP_PORT)) {
    console.error(`FATAL something already answers CDP on ${CDP_PORT}`);
    process.exit(2);
  }
  const chrome = spawn(CHROME, [
    '--headless=new', '--no-sandbox', '--disable-gpu-sandbox',
    `--remote-debugging-port=${CDP_PORT}`,
    `--window-size=${SHEET_WIDTH},${height}`,
    '--force-device-scale-factor=1',
    `--user-data-dir=${join(profile, 'profile')}`,
    'about:blank',
  ], { stdio: 'ignore', env: { ...process.env, DISPLAY: '', WAYLAND_DISPLAY: '' } });
  try {
    for (let i = 0; i < 40; i++) { if (await cdpUp(CDP_PORT)) break; await sleep(500); }
    const argv = [
      SHOOT_LIVE,
      '--port', String(CDP_PORT),
      '--url', `file://${htmlPath}`,
      '--mode', 'page',
      '--dwell', '4000',
      '--timeout', '30000',
      // There is no canvas on this page, so the default readiness predicate
      // would never fire. The sheet is ready when every <img> has decoded.
      '--until', `(() => Array.from(document.images).every(i => i.complete && i.naturalWidth > 0))()`,
      '--out', out,
      // A file:// page gets no WebGPU adapter at all, so D8's gate has nothing to
      // rule on. This is the one shot in the directory that is not a render.
      '--allow-software',
    ];
    if (probe) argv.push('--probe', probe);
    return await run(process.execPath, argv);
  } finally {
    try { chrome.kill('SIGTERM'); } catch { /* gone */ }
    await sleep(300);
    try { chrome.kill('SIGKILL'); } catch { /* gone */ }
    for (let i = 0; i < 20; i++) { if (!(await cdpUp(CDP_PORT))) break; await sleep(500); }
    try { rmSync(profile, { recursive: true, force: true }); } catch { /* best effort */ }
  }
}

/* TWO passes, because `Page.captureScreenshot` shoots the VIEWPORT and the
 * sheet's height depends on how the grid reflows — guessing it leaves either a
 * band of dead background under the last row or a truncated sheet, and both are
 * silent. Pass 1 is a throwaway shot whose only purpose is the probe:
 * `document.documentElement.scrollHeight` at this exact width. Pass 2 shoots at
 * that height. Cropping the pass-1 PNG instead would mean re-encoding it, which
 * would drop shoot-live's tEXt provenance chunks — the thing that lets this file
 * answer for itself. */
async function shootContactSheet(results) {
  const out = join(OUTDIR, 'contact-sheet.png');
  const scratch = mkdtempSync(join(tmpdir(), 'ENC-1288-measure-'));
  const probeOut = join(scratch, 'measure.png');
  let height = 260 + Math.ceil(results.length / 5) * 430;   // fallback estimate
  const m = await sheetPass(1200, probeOut, 'document.documentElement.scrollHeight');
  if (m === 0) {
    try {
      const v = JSON.parse(readFileSync(probeOut + '.probe.json', 'utf8'))?.value;
      if (Number.isFinite(v) && v >= 400 && v <= 16000) height = Math.ceil(v);
      else console.log(`[recap] contact sheet: scrollHeight ${v} out of range; using ${height}`);
    } catch (e) { console.log('[recap] contact sheet: could not read scrollHeight: ' + e.message); }
  } else {
    console.log(`[recap] contact sheet: measuring pass exited ${m}; using estimate ${height}`);
  }
  try { rmSync(scratch, { recursive: true, force: true }); } catch { /* best effort */ }
  console.log(`[recap] contact sheet: ${SHEET_WIDTH}x${height}`);
  return sheetPass(height, out, null);
}

async function main() {
  if (!SHOOT_LIVE || !existsSync(SHOOT_LIVE)) {
    console.error('FATAL cannot find shoot-live.mjs.');
    console.error('      Expected <workspace>/specs/2026-09-19-chart-quality-bar/harness/shoot-live.mjs');
    console.error('      above this repo. Set SHOOT_LIVE=/abs/path/to/shoot-live.mjs to override.');
    process.exit(2);
  }
  if (!existsSync(CHROME)) {
    console.error(`FATAL no chrome at ${CHROME} (set CHROMEDIR)`);
    process.exit(2);
  }
  mkdirSync(OUTDIR, { recursive: true });

  const all = discoverViews();
  const wanted = flag('views', '').trim();
  const views = wanted ? all.filter((v) => wanted.split(/\s+/).includes(v.id)) : all;
  if (!views.length) { console.error('FATAL no views selected'); process.exit(2); }

  const head = execFileSync('git', ['-C', REPO, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();

  if (CONTACT_ONLY) {
    const tally = JSON.parse(readFileSync(join(OUTDIR, 'render-tally.json'), 'utf8'));
    const results = tally.views.map((v) => ({ ...v, pix: { coverage: v.coverage }, still: true }));
    const man = JSON.parse(readFileSync(join(OUTDIR, 'capture-manifest.json'), 'utf8'));
    writeFileSync(join(OUTDIR, 'contact-sheet.html'), contactSheet(results, man.dynachartingHead, man.adapterLine));
    console.log('[recap] contact sheet exit', await shootContactSheet(results));
    return;
  }

  if (!(await fetch(URL_BASE).then((r) => r.ok).catch(() => false))) {
    console.error(`FATAL no showcase dev server on ${URL_BASE}`);
    console.error(`      cd ${REPO} && pnpm --filter @repo/showcase dev --port ${VITE_PORT} --strictPort`);
    process.exit(2);
  }

  console.log(`[recap] ${views.length} views · url=${URL_BASE} · cdp=${CDP_PORT} · out=${OUTDIR}`);
  console.log(`[recap] DynaCharting HEAD ${head}`);
  console.log(`[recap] shoot-live ${SHOOT_LIVE}`);

  const results = [];
  const rows = [];
  for (const v of views) {
    const headNow = execFileSync('git', ['-C', REPO, 'rev-parse', 'HEAD'], { encoding: 'utf8' }).trim();
    if (headNow !== head) console.log(`[recap] !! HEAD MOVED to ${headNow} since the start of this run`);
    const { code, out } = await shootOne(v);
    let cap = null, probe = null;
    try { cap = JSON.parse(readFileSync(out + '.capture.json', 'utf8')); } catch { /* no shot */ }
    try { probe = JSON.parse(readFileSync(out + '.probe.json', 'utf8')); } catch { /* no probe */ }
    const pix = probe?.value?.pix ?? null;
    const verdict = classify(pix);
    const still = !!cap;
    results.push({ ...v, pix, verdict, still });
    rows.push({
      id: v.id,
      still: still ? `${v.id}.png` : null,
      shootLiveExit: code,
      capturedAt: cap?.capturedAt ?? null,
      dynachartingHead: headNow,
      captureMode: cap?.captureMode ?? null,
      tier1Scorable: cap?.tier1Scorable ?? null,
      source: cap?.source ?? null,
      selector: cap?.selector ?? null,
      backingStore: cap?.backingStore ?? null,
      sha256: cap?.sha256 ?? null,
      bytes: cap?.bytes ?? null,
      adapter: cap?.adapter ?? null,
      replayProgressPct: probe?.value?.progress ?? null,
      boundaryErrors: probe?.value?.boundaryErrors ?? null,
      chromeOverlayNodes: probe?.value?.dom?.overlayNodes ?? null,
      verdict,
    });
    console.log(`[recap] ${v.id.padEnd(22)} exit=${code} ${verdict.padEnd(7)} ` +
      `cov=${pix?.coverage != null ? (pix.coverage * 100).toFixed(1) + '%' : 'n/a'} ` +
      `progress=${probe?.value?.progress ?? '?'}% ` +
      `adapter=${cap?.adapter?.adapter?.vendor ?? '?'}/${cap?.adapter?.adapter?.architecture ?? '?'} ` +
      `fallback=${cap?.adapter?.infoIsFallbackAdapter ?? '?'}`);
  }

  // ── acceptance, asserted here rather than left to a reader ────────────────
  const bad = rows.filter((r) =>
    !r.still ||
    r.captureMode !== 'canvas' ||
    r.tier1Scorable !== true ||
    r.adapter?.infoIsFallbackAdapter !== 'false' ||
    r.adapter?.software !== false);
  const adapters = [...new Set(rows.filter((r) => r.adapter?.adapter)
    .map((r) => `${r.adapter.adapter.vendor}/${r.adapter.adapter.architecture}`))];
  const adapterLine = adapters.length === 1
    ? `Every frame was rendered on <b>${adapters[0]}</b>, a hardware adapter — <code>adapter.info.isFallbackAdapter = false</code> on all ${rows.length} (SPEC D8).`
    : `Adapters used: ${adapters.join(', ')}.`;

  writeFileSync(join(OUTDIR, 'capture-manifest.json'), JSON.stringify({
    ticket: 'ENC-1288',
    spec: 'specs/2026-09-19-chart-quality-bar/SPEC.md D8, D10; LIMITATIONS.md DC-L15',
    tool: 'apps/showcase/tools/recapture-stills.mjs',
    capturer: 'specs/2026-09-19-chart-quality-bar/harness/shoot-live.mjs',
    capturedAt: new Date().toISOString(),
    dynachartingHead: head,
    url: `${URL_BASE}?svgAxis=0#/`,
    captureMode: 'canvas',
    source: "canvas.engine-canvas -> canvas.toDataURL('image/png')",
    readiness: 'one full replay pass: scrubber aria-valuenow seen <=5% then >=92%',
    chromeArgs: CHROME_ARGS,
    adapters,
    adapterLine,
    allSoftwareFree: bad.length === 0,
    stills: rows,
  }, null, 2) + '\n');

  writeFileSync(join(OUTDIR, 'render-tally.json'), JSON.stringify({
    capturedAt: new Date().toISOString(),
    tool: 'apps/showcase/tools/recapture-stills.mjs',
    total: results.length,
    tally: results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {}),
    views: results.map((r) => ({
      id: r.id, title: r.title, tier: r.tier, referenceTool: r.referenceTool,
      verdict: r.verdict,
      coverage: r.pix?.coverage ?? null,
      chroma: r.pix?.chroma ?? null,
      distinctColors: r.pix?.distinctColors ?? null,
    })),
  }, null, 2) + '\n');

  writeFileSync(join(OUTDIR, 'contact-sheet.html'), contactSheet(results, head, adapterLine));
  const sheet = await shootContactSheet(results);
  console.log(`[recap] contact-sheet.png exit ${sheet}`);

  const t = results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {});
  console.log(`\n[recap] DONE — full=${t.full || 0} partial=${t.partial || 0} none=${t.none || 0} of ${results.length}`);
  if (bad.length) {
    console.log(`[recap] ${bad.length} still(s) FAILED the capture contract: ` +
      bad.map((b) => b.id).join(', '));
    process.exit(1);
  }
  console.log(`[recap] all ${rows.length} stills: canvas-only, tier1-scorable, ` +
    `hardware adapter (${adapters.join(', ')})`);
}

main().catch((e) => { console.error('[recap] FAILED:', e?.stack ?? e); process.exit(1); });
