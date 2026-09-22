#!/usr/bin/env node
/* apps/showcase/tools/snap-stills.mjs — composited still capture + contact sheet
 * (ENC-550 / T6.2; ENC-586 mid-animation; ENC-1288 stills/ refusal; ENC-1267 adapter + frame rate)
 *
 *   node apps/showcase/tools/snap-stills.mjs --outdir /tmp/<ENC>-stills
 *     [--url http://localhost:5178/] [--wait 10000] [--port 9530]
 *     [--views "a b c"] [--raf-window 1000]
 *     [--allow-software]  proceed on a software adapter. Exit 4 without it.
 *     [--force-software]  the negative control for that gate (see ENC-1267 below).
 *     [--headed]          run on $DISPLAY instead of headless. Recorded either way.
 *
 * Drives the BUILT showcase (served by `vite preview`, or the dev server) through
 * a real WebGPU Chrome and, for EACH view in the registry, navigates to its
 * single-view route (`#/view/<id>`), then — every view has a live/animating
 * loop-replay (ENC-57x..58x merged) — waits a fixed MID-ANIMATION delay (~10s of
 * the ~20s loop timeline) before sampling, so the screenshot freezes the view
 * MID-EVOLUTION rather than at t=0 or the settled end: texture views
 * (FFT/KDE/marching-squares) show a mid-evolution field, streaming views show a
 * partially-grown trace, geometry-frame views show mid-flow. The replay
 * auto-plays and loops, so this fixed wait lands inside the timeline naturally —
 * no seek/scrub needed. It then samples the live canvas to classify the render as
 * full / partial / none, and writes a PNG screenshot of the FULL view region —
 * the engine canvas PLUS the logical-chart chrome overlay (axes / gridlines /
 * legend / colorbar) and the FPS HUD composited over it — to
 *   <outdir>/<view-id>.png
 *
 * The render classifier still samples the bare <canvas> (the GPU render proof),
 * but the screenshot is taken of the `.single-canvas-region` container so the
 * stills read as logical charts (ENC-567).
 *
 * It then assembles a CONTACT SHEET — <outdir>/contact-sheet.html — a tiered,
 * tiled grid of every still with title + tier badge + render verdict, and writes
 * <outdir>/render-tally.json with the per-view results.
 *
 * The showcase is the only thing this touches; the preview server's lifecycle
 * (start/stop by PID) is owned by the caller, never this script.
 *
 * ── ENC-1288: THIS SCRIPT MAY NO LONGER WRITE INTO apps/showcase/stills/ ─────
 *
 * It produced the 23 stills that were committed for three months while every one
 * of them was upside down (LIMITATIONS.md DC-L15), and two of its properties are
 * why the mirror survived:
 *
 *   - it screenshots `.single-canvas-region`, i.e. the engine canvas PLUS the
 *     DOM/SVG chrome overlay. A DOM overlay in a still passes a tier-1 check on
 *     the engine's behalf, and the flipped axis numbers agreed with the flipped
 *     geometry, so nothing in the frame contradicted it (SPEC §1.3, D10);
 *   - it never named the adapter, so a SwiftShader frame and an NVIDIA frame were
 *     indistinguishable afterwards (SPEC D8).
 *
 * The gallery is now captured by `recapture-stills.mjs`, canvas-only and under a
 * recorded hardware adapter. This script is kept because a composited
 * canvas+chrome screenshot is a real thing to want — it is how the app LOOKS —
 * but it refuses the stills directory, so the defect cannot be re-committed by
 * someone reaching for the obvious tool. Point `--outdir` somewhere else.
 *
 * The SECOND of those two properties is fixed here; the first is inherent to what
 * this tool photographs and is why the refusal above stays.
 *
 * ── ENC-1267: THE ADAPTER IS ASKED FOR, RECORDED AND ENFORCED ────────────────
 *
 * Before this ticket the only evidence that a still came off hardware was the
 * flag list in this file. "These were captured on hardware" was an INFERENCE FROM
 * FLAGS, and SPEC D8 makes such a number inadmissible however likely the
 * inference is — Chrome falls back to SwiftShader silently, and a SwiftShader
 * frame of a chart looks exactly like an NVIDIA one.
 *
 * So `navigator.gpu.requestAdapter()` is probed in-page ONCE, before any capture,
 * on the showcase's own origin, and the answer is written into
 * `render-tally.json` under `adapter` and shown on the contact sheet. A software
 * adapter EXITS 4 before a single PNG is written, unless `--allow-software`.
 *
 * READ THE RIGHT PROPERTY. The flag is `adapter.info.isFallbackAdapter`, on
 * GPUAdapterInfo. `GPUAdapter.isFallbackAdapter` — the top-level one — does not
 * exist on Chromium 1228, so `!!adapter.isFallbackAdapter` is `false` on
 * SwiftShader and on an RTX 3070 Ti alike; reading it is what produced the
 * original ENC-1248 "isFallbackAdapter reads false in both" finding, corrected by
 * ENC-1263. Both are reported — the top-level one as the STRING "undefined" —
 * precisely so nobody re-derives the false-in-both belief from its absence.
 * Measured 2026-09-22 on this box, chromium-1228, the two controls this ticket's
 * acceptance criterion names:
 *
 *   with    --ignore-gpu-blocklist --use-angle=vulkan
 *           vendor=nvidia architecture=ampere  info.isFallbackAdapter=false
 *           maxBufferSize=2 GiB  maxTextureDimension2D=16384  features=21
 *   without --ignore-gpu-blocklist
 *           vendor=google architecture=swiftshader  info.isFallbackAdapter=true
 *           maxBufferSize=1 GiB  maxTextureDimension2D=8192   features=18
 *
 * `--force-software` reproduces the second row on demand (it drops
 * `--ignore-gpu-blocklist` AND pins `--use-webgpu-adapter=swiftshader`, so the
 * control is deterministic on a box whose blocklist would have passed anyway).
 * That is the gate's negative control: a check never seen to fail is not a check.
 *
 * DISPLAY IS NO LONGER INHERITED. This script used to launch headed and take
 * whatever `DISPLAY` the calling shell happened to export — so the same command
 * rendered on the GPU or not depending on the terminal it was typed into, and
 * nothing recorded which. It now launches HEADLESS with `DISPLAY`/`WAYLAND_DISPLAY`
 * explicitly cleared in the child environment (the hardware adapter above was
 * measured that way), `--headed` opts back in, and the resolved value is written
 * into the tally under `capture.display`.
 *
 * ── ENC-1267: fps IS A MEASUREMENT, AND OF A NAMED QUANTITY ──────────────────
 *
 * `fps` here is the rate the page services requestAnimationFrame on the MAIN
 * THREAD. That is the same quantity the FPS HUD's left-hand figure means
 * (ENC-1265, apps/showcase/src/chrome/FpsHud.tsx) — a RESPONSIVENESS number, not
 * a render rate and not a frame budget: `EngineHost.tick` calls `updateHud` every
 * tick whether or not a frame was rendered, so a long WASM render, the
 * framebuffer readback, the canvas blit and unrelated main-thread JS all depress
 * it identically.
 *
 * It is recorded because this capture path HAS a real frame loop to measure: the
 * engine runs its own rAF loop and the replay auto-plays for the whole `--wait`
 * dwell. Two independent readings go in, and neither is derived from the other:
 *
 *   frame.hudFps     what the HUD is SHOWING, read out of `.chrome-fps-value`.
 *                    Integer (the HUD prints `toFixed(0)`). `null` when the HUD
 *                    shows its em-dash, with the reason in `frame.hudFpsAbsent`.
 *   frame.rafHz      this harness's OWN count of rAF callbacks over a measured
 *                    window (`--raf-window`, default 1000ms) ending immediately
 *                    before the canvas sample. Unrounded, and its provenance is
 *                    not ambiguous.
 *
 * NOTHING IS DERIVED OR INVENTED. `frame.renderCpuMs` is the HUD's right-hand
 * figure — CPU wall-clock inside `DawnSceneRenderer::render` — and is `null` when
 * the HUD shows an em-dash, never `1000 / fps`. That substitution is exactly the
 * defect ENC-1265 removed from the HUD, and PERF-CLAIMS C1/C3 struck the 22
 * stills it was burned into; re-introducing it one file away would be worse than
 * recording nothing.
 *
 * ── ENC-1267: THE DRIVER IS RAW CDP, NOT PLAYWRIGHT ──────────────────────────
 *
 * This file used to `import` Playwright from a `~/pw` harness outside the
 * workspace. That directory does not exist on this machine — the ENC-1288 header
 * note above says so in as many words — so the script could not be RUN here, and
 * an acceptance criterion that cannot be executed is not one. It now speaks CDP
 * directly to the `chromium-1228` binary in the Playwright browser cache, the
 * same binary and the same launch flags `recapture-stills.mjs` uses, with no
 * package dependency at all. `--chrome` / `CHROMEDIR` override the location.
 */

import { spawn } from 'node:child_process';
import { mkdirSync, writeFileSync, readFileSync, readdirSync, existsSync, mkdtempSync, rmSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { tmpdir } from 'node:os';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SHOWCASE_DIR = resolve(__dirname, '..');

const VIEWS_DIR = join(SHOWCASE_DIR, 'views');

const args = process.argv.slice(2);
const flag = (n, d) => {
  const i = args.indexOf(`--${n}`);
  return i >= 0 && args[i + 1] && !args[i + 1].startsWith('--') ? args[i + 1] : d;
};
const has = (n) => args.includes(`--${n}`);

const URL_BASE = flag('url', process.env.SHOWCASE_URL || 'http://localhost:5178/');
// Mid-animation delay (ms): the loop-replay timeline is ~20s and auto-plays, so
// ~10s lands roughly mid-evolution (texture field half-swapped, traces partially
// grown, geometry-frame flows mid-stride) — the ENC-586 "show motion" capture.
const WAIT = Number(flag('wait', '10000'));
const OUTDIR = flag('outdir', join(SHOWCASE_DIR, 'stills'));
const CDP_PORT = flag('port', '9530');
// The rAF-rate sampling window. 1000ms at ~60Hz is ~60 callbacks, i.e. ±1.7% of
// quantisation — enough resolution that a 60 and a 30 can never be confused, and
// short enough to sit inside the mid-animation moment being photographed.
const RAF_WINDOW = Number(flag('raf-window', '1000'));
const ALLOW_SOFTWARE = has('allow-software');
const FORCE_SOFTWARE = has('force-software');
const HEADED = has('headed');

// ENC-1288: see the header. A region screenshot may not become a committed still.
if (resolve(OUTDIR) === resolve(SHOWCASE_DIR, 'stills')) {
  console.error(
    `[snap] REFUSING to write into ${resolve(OUTDIR)}.\n` +
    `[snap] This script screenshots '.single-canvas-region' — the engine canvas WITH the\n` +
    `[snap] DOM/SVG chrome composited over it. That is why the stills committed at 537c995\n` +
    `[snap] stayed upside down for three months without anything in the frame contradicting\n` +
    `[snap] it (LIMITATIONS.md DC-L15, SPEC D8/D10).\n` +
    `[snap] To regenerate the gallery:  node apps/showcase/tools/recapture-stills.mjs\n` +
    `[snap] To use this script anyway:  --outdir <somewhere that is not stills/>`);
  process.exit(2);
}

const CHROMEDIR = process.env.CHROMEDIR ||
  join(process.env.HOME || '', '.cache', 'ms-playwright', 'chromium-1228', 'chrome-linux64');
const CHROME = flag('chrome', join(CHROMEDIR, 'chrome'));

/* All of these are required for a hardware adapter on this box. `--force-software`
 * is the negative control for the D8 gate and drops the blocklist flag the way the
 * acceptance criterion describes, then pins SwiftShader so the control does not
 * depend on the local blocklist. */
const CHROME_ARGS = [
  ...(HEADED ? ['--ozone-platform=x11'] : ['--headless=new']),
  '--enable-unsafe-webgpu',
  '--enable-features=Vulkan',
  ...(FORCE_SOFTWARE ? ['--use-webgpu-adapter=swiftshader'] : ['--ignore-gpu-blocklist']),
  '--use-angle=vulkan',
  '--no-sandbox',
  '--disable-gpu-sandbox',
];

// Explicit, never inherited (ENC-1267). Headless renders through the Vulkan ICD
// with no display at all; headed needs one and says which.
const DISPLAY_VALUE = HEADED ? (flag('display', process.env.DISPLAY || ':0')) : '';

const TIER_RANK = { native: 0, composed: 1, walled: 2 };

/**
 * Human-verified verdict overrides. The automatic classifier reads a canvas
 * sample (coverage + chroma); a handful of views render crisp, complete, and
 * recognizable but with such thin 1px strokes on a large dark canvas that the
 * pixel sample under-reports them. These verdicts were confirmed by eye against
 * the captured still and pinned here so the tally reflects what actually renders.
 *   - radial-seasonality: the full polar clock (ring + 24 spokes + closed series
 *     loop) renders correctly; its 1px blue `line2d` strokes just read faint.
 *   - ecg: the live PQRST trace renders crisply (lineAA@1 thick trace, ENC-587),
 *     but at a MID-ANIMATION freeze (ENC-586) the partially-grown window can land
 *     on a quiet inter-beat baseline segment, so the pixel sample under-reports a
 *     near-flat thin green line. Confirmed rendering by eye; pinned to full.
 */
const VERDICT_OVERRIDE = {
  'radial-seasonality': 'full',
  'ecg': 'full',
};

/** Discover the catalog the same way registry.ts does: every views/<id> with a view.json. */
function discoverViews() {
  const ids = readdirSync(VIEWS_DIR).filter((d) => existsSync(join(VIEWS_DIR, d, 'view.json')));
  const views = ids.map((id) => {
    const meta = JSON.parse(readFileSync(join(VIEWS_DIR, id, 'view.json'), 'utf8'));
    return { id: meta.id, title: meta.title, tier: meta.tier, referenceTool: meta.referenceTool };
  });
  // Mirror registry sort: tier (native→composed→walled) then title.
  views.sort((a, b) => (TIER_RANK[a.tier] - TIER_RANK[b.tier]) || a.title.localeCompare(b.title));
  return views;
}

/* ── minimal CDP client ────────────────────────────────────────────────────── */

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const getJSON = async (p) => (await fetch(`http://localhost:${CDP_PORT}${p}`)).json();
const cdpUp = async () => { try { return (await fetch(`http://localhost:${CDP_PORT}/json/version`)).ok; } catch { return false; } };

function makeClient(wsUrl, onEvent) {
  const ws = new WebSocket(wsUrl);
  let id = 0;
  const pending = new Map();
  ws.addEventListener('message', (ev) => {
    const msg = JSON.parse(ev.data);
    if (msg.id && pending.has(msg.id)) {
      const { resolve: res, reject } = pending.get(msg.id);
      pending.delete(msg.id);
      if (msg.error) reject(new Error(JSON.stringify(msg.error)));
      else res(msg.result);
    } else if (msg.method && onEvent) {
      onEvent(msg);
    }
  });
  const ready = new Promise((res, rej) => {
    ws.addEventListener('open', res);
    ws.addEventListener('error', rej);
  });
  const send = (method, params = {}) =>
    new Promise((res, rej) => {
      const mid = ++id;
      pending.set(mid, { resolve: res, reject: rej });
      ws.send(JSON.stringify({ id: mid, method, params }));
    });
  return { ws, ready, send, close: () => { try { ws.close(); } catch { /* already gone */ } } };
}

async function evalExpr(client, expr, { awaitPromise = true } = {}) {
  const r = await client.send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise });
  if (r.exceptionDetails) throw new Error('eval: ' + JSON.stringify(r.exceptionDetails));
  return r.result.value;
}

/* ── the adapter probe (ENC-1267 / SPEC D8) ────────────────────────────────── */

/* Three independent discriminators, because the obvious one was wrong from the
 * moment it was written (ENC-1248 → ENC-1263, same day). See the header. */
const ADAPTER_PROBE = `(async () => {
  if (!navigator.gpu) return { webgpu: false, reason: 'navigator.gpu undefined (not a secure context?)' };
  try {
    const a = await navigator.gpu.requestAdapter();
    if (!a) return { webgpu: true, adapter: null, reason: 'requestAdapter() returned null' };
    const info = a.info || (a.requestAdapterInfo ? await a.requestAdapterInfo() : {});
    const arch = String(info.architecture ?? '');
    const fallback = info.isFallbackAdapter;
    return {
      webgpu: true,
      // THE AUTHORITATIVE FLAG. On GPUAdapterInfo, not on GPUAdapter.
      infoIsFallbackAdapter: String(fallback),
      // Reported as a STRING, and it is "undefined" on Chromium 1228 — the
      // property moved. Never discriminate on this one.
      adapterIsFallbackAdapter: String(a.isFallbackAdapter),
      software: fallback === true || /swiftshader|lavapipe|llvmpipe|warp/i.test(arch),
      adapter: {
        vendor: info.vendor ?? '?',
        architecture: info.architecture ?? '?',
        device: info.device ?? '?',
        description: info.description ?? '?',
      },
      // Limits that differ by construction (SwiftShader 1 GiB / 8192 / 18 vs
      // Ampere 2 GiB / 16384 / 21): a corroborating signal that rests on no flag.
      corroborating: {
        maxBufferSize: a.limits?.maxBufferSize ?? null,
        maxTextureDimension2D: a.limits?.maxTextureDimension2D ?? null,
        featureCount: [...a.features].length,
      },
    };
  } catch (e) { return { webgpu: true, error: String(e) }; }
})()`;

function adapterLabel(ad) {
  if (!ad) return 'adapter NOT PROBED';
  if (ad.webgpu === false) return `NO WebGPU (${ad.reason ?? '?'})`;
  if (ad.error) return `adapter probe error: ${ad.error}`;
  if (!ad.adapter) return `no adapter (${ad.reason ?? '?'})`;
  return `${ad.adapter.vendor}/${ad.adapter.architecture} ` +
    `info.isFallbackAdapter=${ad.infoIsFallbackAdapter} ` +
    `(GPUAdapter.isFallbackAdapter=${ad.adapterIsFallbackAdapter})`;
}

/* ── per-view frame-rate measurement (ENC-1267) ───────────────────────────────
 *
 * Counts rAF callbacks over a measured window and reports the window it actually
 * got, not the one it asked for. Returns Hz, the raw frame count and the elapsed
 * ms, so the number can be re-derived from the record. */
const rafProbe = (ms) => `(() => new Promise((resolve) => {
  const t0 = performance.now();
  let frames = 0;
  const tick = () => {
    frames++;
    const dt = performance.now() - t0;
    if (dt >= ${ms}) { resolve({ frames, elapsedMs: dt, hz: (frames * 1000) / dt }); return; }
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);
}))()`;

/* What the HUD is SHOWING. Read as text, from the element the HUD renders, so
 * this is a statement about the pixels in the screenshot beside it. The em-dash
 * is the HUD's "no measurement" and becomes null, never a number. */
const HUD_PROBE = `(() => {
  const el = document.querySelector('.chrome-fps');
  if (!el) return { hudPresent: false, hudFps: null, hudFpsAbsent: 'no .chrome-fps in the DOM (HUD hidden?)', renderCpuMs: null };
  const txt = (sel) => { const n = el.querySelector(sel); return n ? n.textContent.trim() : null; };
  const num = (s) => (s == null || !/^[0-9]+(\\.[0-9]+)?$/.test(s) ? null : Number(s));
  const fpsRaw = txt('.chrome-fps-value');
  const msRaw = txt('.chrome-fps-ms');
  return {
    hudPresent: true,
    hudFpsRaw: fpsRaw,
    hudFps: num(fpsRaw),
    hudFpsAbsent: num(fpsRaw) == null ? 'HUD shows ' + JSON.stringify(fpsRaw) + ' — no fps sample yet' : null,
    renderCpuMsRaw: msRaw,
    renderCpuMs: num(msRaw),
    renderCpuMsAbsent: num(msRaw) == null ? 'HUD shows ' + JSON.stringify(msRaw) + ' — no frame rendered yet' : null,
  };
})()`;

/**
 * Sample the live canvas. Returns geometry + a coverage measure: the fraction
 * of sampled pixels that differ meaningfully from the pane-clear background.
 * The showcase panes clear to a near-black (~(8..13, 8..13, 13..20)); anything
 * with appreciable color/brightness above that is rendered content.
 */
const CANVAS_SAMPLE = `(() => {
  const c = document.querySelector('canvas');
  if (!c || !c.width || !c.height) return { ok: false, reason: 'no canvas' };
  const off = document.createElement('canvas');
  off.width = c.width;
  off.height = c.height;
  const ctx = off.getContext('2d');
  ctx.drawImage(c, 0, 0);
  let data;
  try {
    data = ctx.getImageData(0, 0, off.width, off.height).data;
  } catch (e) {
    return { ok: false, reason: 'readback blocked: ' + (e && e.message) };
  }
  const colors = new Set();
  let sampled = 0;
  let nonbg = 0; // pixels meaningfully brighter/different than the bg
  let chroma = 0; // pixels with appreciable color saturation (vector geometry)
  // Stride so the sample is cheap but representative across the whole canvas.
  const stride = 4 * 11;
  for (let i = 0; i < data.length; i += stride) {
    sampled++;
    const r = data[i], g = data[i + 1], b = data[i + 2];
    if (sampled % 7 === 0) colors.add(r + ',' + g + ',' + b);
    // distance from the darkest pane clear (~8,8,13) and the app bg (~13,13,20)
    const dClear = Math.abs(r - 8) + Math.abs(g - 8) + Math.abs(b - 13);
    const dApp = Math.abs(r - 13) + Math.abs(g - 13) + Math.abs(b - 20);
    if (Math.min(dClear, dApp) > 36) nonbg++;
    // Saturation: max−min channel spread. The bg is near-grey (spread≈7), so a
    // green candle, a red ladder bar, a blue spoke or a teal trace all stand
    // out — this catches thin vector geometry that low raw "coverage" misses.
    const mx = Math.max(r, g, b);
    const mn = Math.min(r, g, b);
    if (mx - mn > 22 && mx > 30) chroma++;
  }
  return {
    ok: true,
    w: c.width,
    h: c.height,
    distinctColors: colors.size,
    coverage: sampled ? nonbg / sampled : 0,
    chroma: sampled ? chroma / sampled : 0,
    nonbgPixels: nonbg,
  };
})()`;

/** Where to put the clip rectangle: the composited region, else the bare canvas. */
const REGION_RECT = `(() => {
  const sel = document.querySelector('.single-canvas-region') ? '.single-canvas-region' : 'canvas';
  const el = document.querySelector(sel);
  if (!el) return null;
  const r = el.getBoundingClientRect();
  if (!(r.width > 0 && r.height > 0)) return null;
  return { selector: sel, x: r.x, y: r.y, width: r.width, height: r.height };
})()`;

/**
 * Classify a render from its canvas sample. "Coverage" alone under-reports thin
 * vector geometry (a candle wall, an ECG trace, a polar clock are mostly dark
 * canvas with crisp colored strokes), so we combine two signals:
 *   - coverage: fraction of pixels brighter/different than the bg (fills/textures)
 *   - chroma:   fraction of pixels with appreciable color saturation (strokes)
 * Either one clearing the bar means real content is on the canvas. A render is
 * `full` when there is a clear, structured signal; `partial` when only a faint
 * sliver is present; `none` when the canvas is effectively empty.
 */
function classify(pix) {
  if (!pix || !pix.ok) return 'none';
  const cov = pix.coverage ?? 0;
  const chroma = pix.chroma ?? 0;
  const colors = pix.distinctColors ?? 0;
  // Effectively empty canvas.
  if (cov < 0.004 && chroma < 0.004) return 'none';
  // Clear structured content: a meaningful fill OR a meaningful colored stroke set.
  if (cov >= 0.04 || chroma >= 0.02 || (colors >= 8 && (cov >= 0.015 || chroma >= 0.01))) {
    return 'full';
  }
  // Something is there, but only faintly (a sparse 1px point cloud, a sliver).
  return 'partial';
}

const VERDICT_COLOR = { full: '#3ddc84', partial: '#f5b14c', none: '#e5534b' };
const TIER_COLOR = { native: '#3ddc84', composed: '#4c9bf5', walled: '#f5b14c' };

function contactSheet(results, adapter, capture) {
  const cards = results
    .map((r) => {
      const tc = TIER_COLOR[r.tier] || '#888';
      const vc = VERDICT_COLOR[r.verdict] || '#888';
      const img = r.still ? `<img src="./${r.id}.png" alt="${r.title}" loading="lazy" />` : `<div class="missing">no still</div>`;
      const cov = r.pix && r.pix.coverage != null ? (r.pix.coverage * 100).toFixed(1) + '%' : '—';
      // rAF Hz, not "fps": the unit is named for what was measured (ENC-1265).
      const hz = r.frame && r.frame.rafHz != null ? r.frame.rafHz.toFixed(1) + ' Hz rAF' : 'rAF —';
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
        <div class="row meta"><code>${r.id}</code><span>coverage ${cov} · ${hz}</span></div>
      </figcaption>
    </figure>`;
    })
    .join('\n');

  const tally = results.reduce(
    (a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a),
    {},
  );
  const total = results.length;
  const softWarn = adapter && adapter.software
    ? `<p class="warn"><b>SOFTWARE ADAPTER.</b> These frames were rendered by a CPU rasterizer, not a GPU. They are not admissible as evidence about this renderer's output or its performance (SPEC D8).</p>`
    : '';

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
  .prov { margin: 10px 0 0; color: #6f6f86; font-size: 12.5px; max-width: 95ch; }
  .prov code { color: #8a8aff; }
  .warn { margin: 10px 0 0; color: #e5534b; font-size: 12.5px; max-width: 95ch; }
  .tally { margin: 14px 0 0; display: flex; gap: 10px; flex-wrap: wrap; }
  .tally .chip { padding: 4px 12px; border-radius: 999px; font-size: 13px;
                 border: 1px solid var(--c); color: var(--c); }
  .grid { display: grid; gap: 18px; padding: 24px 32px 56px;
          grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); }
  .card { margin: 0; background: #11111c; border: 1px solid #20202e;
          border-radius: 12px; overflow: hidden; }
  .frame { aspect-ratio: 11 / 7; background: #05050a; display: flex;
           align-items: center; justify-content: center; }
  .frame img { width: 100%; height: 100%; object-fit: cover; display: block; }
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
  <p class="prov"><b>Provenance.</b> Each frame is a screenshot of <code>.single-canvas-region</code> — the engine canvas <em>with</em> the DOM/SVG chrome overlay and the FPS HUD composited over it, so <b>no image here is tier-1 scorable</b> (SPEC D10); the canvas-only gallery is <code>apps/showcase/stills/</code>. Adapter: <code>${adapterLabel(adapter)}</code>, probed in-page before any capture and enforced (SPEC D8, ENC-1267). Chrome <code>${capture.headless ? 'headless' : 'headed on ' + capture.display}</code>. The per-view <code>rAF Hz</code> is this harness's own count of main-thread requestAnimationFrame callbacks over ${capture.rafWindowMs}ms — a responsiveness number, not a render rate and not a frame budget (ENC-1265). Full record: <code>render-tally.json</code>.</p>
  ${softWarn}
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

/* ── driving one view ─────────────────────────────────────────────────────── */

async function openPage(onEvent) {
  const t = await (await fetch(`http://localhost:${CDP_PORT}/json/new?about:blank`, { method: 'PUT' })).json();
  const client = makeClient(t.webSocketDebuggerUrl, onEvent);
  await client.ready;
  await client.send('Page.enable');
  await client.send('Runtime.enable');
  return { client, targetId: t.id };
}

async function closePage(client, targetId) {
  client.close();
  try { await fetch(`http://localhost:${CDP_PORT}/json/close/${targetId}`); } catch { /* already gone */ }
}

async function main() {
  if (!existsSync(CHROME)) {
    console.error(`[snap] FATAL no chrome at ${CHROME}\n` +
      `[snap] set CHROMEDIR=<dir containing ./chrome> or pass --chrome /abs/path/to/chrome`);
    process.exit(2);
  }
  if (!(await fetch(URL_BASE).then((r) => r.ok).catch(() => false))) {
    console.error(`[snap] FATAL nothing serving ${URL_BASE}\n` +
      `[snap] start one:  pnpm --filter @repo/showcase preview --port <n> --strictPort`);
    process.exit(2);
  }
  if (await cdpUp()) {
    console.error(`[snap] FATAL something already answers CDP on ${CDP_PORT}; refusing to drive ` +
      `another session's browser. Pass --port <n>.`);
    process.exit(2);
  }

  mkdirSync(OUTDIR, { recursive: true });
  const all = discoverViews();
  const wanted = flag('views', '').trim();
  const views = wanted ? all.filter((v) => wanted.split(/\s+/).includes(v.id)) : all;
  if (!views.length) { console.error('[snap] FATAL no views selected'); process.exit(2); }

  console.log(`[snap] ${views.length} views; url=${URL_BASE} wait=${WAIT}ms out=${resolve(OUTDIR)}`);
  console.log(`[snap] chrome ${CHROME}`);
  console.log(`[snap] flags  ${CHROME_ARGS.join(' ')}`);
  console.log(`[snap] display ${HEADED ? DISPLAY_VALUE : '(cleared — headless)'}`);

  const profile = mkdtempSync(join(tmpdir(), 'ENC-1267-snap-'));
  const chrome = spawn(CHROME, [
    ...CHROME_ARGS,
    `--remote-debugging-port=${CDP_PORT}`,
    '--window-size=1180,760',
    '--force-device-scale-factor=1',
    `--user-data-dir=${join(profile, 'profile')}`,
    'about:blank',
  ], {
    stdio: 'ignore',
    env: { ...process.env, DISPLAY: DISPLAY_VALUE, WAYLAND_DISPLAY: HEADED ? (process.env.WAYLAND_DISPLAY ?? '') : '' },
  });

  let exitCode = 0;
  const results = [];
  let adapter = null;
  const capture = {
    tool: 'apps/showcase/tools/snap-stills.mjs',
    driver: 'raw CDP (no playwright)',
    chrome: CHROME,
    chromeArgs: CHROME_ARGS,
    headless: !HEADED,
    display: HEADED ? DISPLAY_VALUE : null,
    url: URL_BASE,
    dwellMs: WAIT,
    rafWindowMs: RAF_WINDOW,
    captureMode: 'region',
    captureDetail: '.single-canvas-region — engine canvas WITH DOM/SVG chrome and the FPS HUD composited over it',
    tier1Scorable: false,
    allowSoftware: ALLOW_SOFTWARE,
    forceSoftware: FORCE_SOFTWARE,
  };

  try {
    for (let i = 0; i < 60; i++) { if (await cdpUp()) break; await sleep(500); }
    if (!(await cdpUp())) { console.error('[snap] FATAL chrome never answered CDP'); process.exit(1); }

    /* ENC-1267 / SPEC D8 — ASK BEFORE SHOOTING. The probe runs on the showcase's
     * own origin (WebGPU needs a secure context; `about:blank` reports
     * `navigator.gpu undefined` and would look like a broken box) and BEFORE any
     * PNG is written, so a software run leaves the output directory untouched. */
    {
      const { client, targetId } = await openPage();
      await client.send('Page.navigate', { url: URL_BASE });
      for (let i = 0; i < 60; i++) {
        if (await evalExpr(client, `document.readyState === 'complete'`)) break;
        await sleep(250);
      }
      adapter = await evalExpr(client, ADAPTER_PROBE);
      adapter.probedAt = new Date().toISOString();
      adapter.probedOn = URL_BASE;
      await closePage(client, targetId);
    }
    console.log(`[snap] adapter: ${adapterLabel(adapter)}`);
    if (adapter.corroborating) {
      console.log(`[snap]          limits maxBufferSize=${adapter.corroborating.maxBufferSize} ` +
        `maxTextureDimension2D=${adapter.corroborating.maxTextureDimension2D} ` +
        `features=${adapter.corroborating.featureCount}`);
    }
    const noHardware = !adapter.adapter || adapter.software === true;
    if (noHardware && !ALLOW_SOFTWARE) {
      console.error(
        `[snap] REFUSING to capture: ${adapter.software ? 'SOFTWARE adapter' : 'no WebGPU adapter'}.\n` +
        `[snap] A SwiftShader frame of a chart is indistinguishable from an NVIDIA one after the\n` +
        `[snap] fact, and any frame-rate taken off one describes a CPU rasterizer (SPEC D8,\n` +
        `[snap] ENC-1263). Nothing has been written to ${resolve(OUTDIR)}.\n` +
        `[snap] Fix the flags, or pass --allow-software to record it as software on purpose.`);
      process.exit(4);
    }
    if (noHardware) {
      console.warn('[snap] WARNING --allow-software: capturing on a software adapter. The tally ' +
        'and the contact sheet will say so.');
    }

    for (const v of views) {
      const logs = [];
      const { client, targetId } = await openPage((msg) => {
        if (msg.method === 'Runtime.consoleAPICalled') {
          logs.push(`[${msg.params.type}] ` + msg.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
        } else if (msg.method === 'Runtime.exceptionThrown') {
          logs.push('PAGEERR: ' + (msg.params.exceptionDetails?.exception?.description ??
            msg.params.exceptionDetails?.text ?? '?'));
        }
      });
      let pix = null;
      let frame = null;
      let still = false;
      try {
        await client.send('Page.navigate', { url: URL_BASE + '#/view/' + v.id });
        for (let i = 0; i < 120; i++) {
          if (await evalExpr(client, `document.readyState === 'complete'`)) break;
          await sleep(250);
        }
        // engine bring-up + manifest apply + let the loop-replay run to ~mid-timeline
        // (NOT "settle") so the frozen frame reads as mid-animation motion (ENC-586).
        // The rAF window is spent INSIDE the dwell, not after it, so the frame rate
        // describes the moment being photographed rather than a later one.
        await sleep(Math.max(0, WAIT - RAF_WINDOW));
        const raf = await evalExpr(client, rafProbe(RAF_WINDOW));
        const hud = await evalExpr(client, HUD_PROBE);
        frame = {
          // What the HUD is showing, in the same frame as the screenshot below.
          ...hud,
          // This harness's own measurement of the same quantity. Independent of
          // the HUD, and not derived from it or from anything else.
          rafHz: raf?.hz ?? null,
          rafFrames: raf?.frames ?? null,
          rafWindowMs: raf?.elapsedMs ?? null,
          quantity: 'main-thread requestAnimationFrame callback rate (responsiveness), NOT a render rate or a frame budget — ENC-1265',
        };
        pix = await evalExpr(client, CANVAS_SAMPLE);
        // Screenshot the FULL view region — the engine canvas PLUS the composited
        // chrome overlay (axes/gridlines/legend/colorbar) + FPS HUD — so the still
        // reads as the logical chart, not the bare canvas. The `.single-canvas-region`
        // container is the slot the shared canvas portals into and the chrome
        // overlay mounts over; falls back to the canvas if it's not found.
        const rect = await evalExpr(client, REGION_RECT);
        if (!rect) throw new Error('no .single-canvas-region and no canvas to clip to');
        const shot = await client.send('Page.captureScreenshot', {
          format: 'png',
          clip: { x: rect.x, y: rect.y, width: rect.width, height: rect.height, scale: 1 },
          captureBeyondViewport: false,
        });
        writeFileSync(join(OUTDIR, `${v.id}.png`), Buffer.from(shot.data, 'base64'));
        still = true;
        frame.regionSelector = rect.selector;
      } catch (e) {
        logs.push('CAPTURE_ERR: ' + (e && e.message));
      }
      await closePage(client, targetId);

      const autoVerdict = classify(pix);
      const verdict = VERDICT_OVERRIDE[v.id] ?? autoVerdict;
      const rejects = logs.filter((l) => /ID_TAKEN|FRAME_REJECTED|rejected/.test(l)).length;
      results.push({ ...v, pix, frame, verdict, autoVerdict, still });
      const ov = verdict !== autoVerdict ? ` (auto=${autoVerdict}, overridden)` : '';
      console.log(
        `[snap] ${v.id.padEnd(22)} ${verdict.padEnd(7)} ` +
        `cov=${pix && pix.coverage != null ? (pix.coverage * 100).toFixed(1) + '%' : 'n/a'} ` +
        `chroma=${pix && pix.chroma != null ? (pix.chroma * 100).toFixed(1) + '%' : 'n/a'} ` +
        `colors=${pix?.distinctColors ?? '-'} ` +
        `raf=${frame?.rafHz != null ? frame.rafHz.toFixed(1) + 'Hz' : 'n/a'} ` +
        `hud=${frame?.hudFps ?? '–'}fps/${frame?.renderCpuMs ?? '–'}ms ` +
        `rejects=${rejects}${ov}`,
      );
      if (verdict === 'none' || !still) {
        const err = logs.find((l) => /PAGEERR|CAPTURE_ERR|error/i.test(l));
        if (err) console.log('   ↳', err);
      }
    }
  } finally {
    try { chrome.kill('SIGTERM'); } catch { /* already gone */ }
    await sleep(300);
    try { chrome.kill('SIGKILL'); } catch { /* already gone */ }
    for (let i = 0; i < 20; i++) { if (!(await cdpUp())) break; await sleep(500); }
    try { rmSync(profile, { recursive: true, force: true }); } catch { /* best effort */ }
  }

  // Contact sheet + machine-readable tally.
  writeFileSync(join(OUTDIR, 'contact-sheet.html'), contactSheet(results, adapter, capture));
  writeFileSync(
    join(OUTDIR, 'render-tally.json'),
    JSON.stringify(
      {
        capturedAt: new Date().toISOString(),
        capture,
        // SPEC D8: an observation, not an inference from the flag list above.
        adapter,
        total: results.length,
        tally: results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {}),
        views: results.map((r) => ({
          id: r.id,
          title: r.title,
          tier: r.tier,
          verdict: r.verdict,
          autoVerdict: r.autoVerdict,
          coverage: r.pix?.coverage ?? null,
          chroma: r.pix?.chroma ?? null,
          distinctColors: r.pix?.distinctColors ?? null,
          // ENC-1267. `fps` is the HUD's figure; `frame` carries both readings,
          // their provenance, and an explicit reason whenever one is null.
          fps: r.frame?.hudFps ?? null,
          fpsSource: r.frame?.hudFps != null ? 'FPS HUD .chrome-fps-value (engine rAF rate)' : null,
          renderCpuMs: r.frame?.renderCpuMs ?? null,
          frame: r.frame ?? null,
        })),
      },
      null,
      2,
    ),
  );

  const t = results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {});
  console.log(`\n[snap] DONE — full=${t.full || 0} partial=${t.partial || 0} none=${t.none || 0} of ${results.length}`);
  console.log(`[snap] adapter: ${adapterLabel(adapter)}`);
  console.log(`[snap] contact sheet: ${join(resolve(OUTDIR), 'contact-sheet.html')}`);
  console.log(`[snap] tally:         ${join(resolve(OUTDIR), 'render-tally.json')}`);
  process.exit(exitCode);
}

main().catch((err) => {
  console.error('[snap] FAILED:', err?.stack ?? err);
  process.exit(1);
});
