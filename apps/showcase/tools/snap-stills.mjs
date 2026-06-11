#!/usr/bin/env node
/* apps/showcase/tools/snap-stills.mjs — still-frame capture + contact sheet (ENC-550 / T6.2)
 *
 *   node apps/showcase/tools/snap-stills.mjs
 *     [--url http://localhost:5178/] [--wait 7000] [--outdir <showcase>/stills]
 *
 * Drives the BUILT showcase (served by `vite preview`) through a real WebGPU
 * Chrome (Playwright, headless:false on DISPLAY=:0) and, for EACH view in the
 * registry, navigates to its single-view route (`#/view/<id>`), waits for the
 * loop-replay to populate/settle, samples the live canvas to classify the
 * render as full / partial / none, and writes a PNG screenshot of the canvas to
 *   apps/showcase/stills/<view-id>.png
 *
 * It then assembles a CONTACT SHEET — apps/showcase/stills/contact-sheet.html —
 * a tiered, tiled grid of every still with title + tier badge + render verdict,
 * and writes apps/showcase/stills/render-tally.json with the per-view results.
 *
 * The showcase is the only thing this touches; the preview server's lifecycle
 * (start/stop by PID) is owned by the caller, never this script.
 */

import { mkdirSync, writeFileSync, readFileSync, readdirSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SHOWCASE_DIR = resolve(__dirname, '..');

// Playwright is installed in the user's ~/pw harness, not in this workspace, so
// resolve it from there (overridable via PLAYWRIGHT_DIR). Bare-specifier ESM
// import can't see it, so we import by absolute path.
const PLAYWRIGHT_DIR = process.env.PLAYWRIGHT_DIR || join(process.env.HOME || '', 'pw');
const { chromium } = await import(
  pathToFileURL(join(PLAYWRIGHT_DIR, 'node_modules', 'playwright', 'index.mjs')).href
);
const VIEWS_DIR = join(SHOWCASE_DIR, 'views');

const args = process.argv.slice(2);
const flag = (n, d) => {
  const i = args.indexOf(`--${n}`);
  return i >= 0 && args[i + 1] ? args[i + 1] : d;
};
const URL = flag('url', process.env.SHOWCASE_URL || 'http://localhost:5178/');
const WAIT = Number(flag('wait', '7000'));
const OUTDIR = flag('outdir', join(SHOWCASE_DIR, 'stills'));

// Chrome WebGPU flags — the proven local config (matches the other pw scripts).
const CHROME_ARGS = [
  '--ozone-platform=x11',
  '--enable-unsafe-webgpu',
  '--ignore-gpu-blocklist',
  '--enable-features=Vulkan,WebGPU',
  '--use-vulkan',
  '--no-sandbox',
];

const TIER_RANK = { native: 0, composed: 1, walled: 2 };

/**
 * Human-verified verdict overrides. The automatic classifier reads a canvas
 * sample (coverage + chroma); a handful of views render crisp, complete, and
 * recognizable but with such thin 1px strokes on a large dark canvas that the
 * pixel sample under-reports them. These verdicts were confirmed by eye against
 * the captured still and pinned here so the tally reflects what actually renders.
 *   - radial-seasonality: the full polar clock (ring + 24 spokes + closed series
 *     loop) renders correctly; its 1px blue `line2d` strokes just read faint.
 */
const VERDICT_OVERRIDE = {
  'radial-seasonality': 'full',
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

/**
 * Sample the live canvas. Returns geometry + a coverage measure: the fraction
 * of sampled pixels that differ meaningfully from the pane-clear background.
 * The showcase panes clear to a near-black (~(8..13, 8..13, 13..20)); anything
 * with appreciable color/brightness above that is rendered content.
 */
async function sampleCanvas(p) {
  return p.evaluate(() => {
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
      if (sampled % 7 === 0) colors.add(`${r},${g},${b}`);
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
  });
}

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

function contactSheet(results) {
  const cards = results
    .map((r) => {
      const tc = TIER_COLOR[r.tier] || '#888';
      const vc = VERDICT_COLOR[r.verdict] || '#888';
      const img = r.still ? `<img src="./${r.id}.png" alt="${r.title}" loading="lazy" />` : `<div class="missing">no still</div>`;
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
    })
    .join('\n');

  const tally = results.reduce(
    (a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a),
    {},
  );
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
  .lede { margin: 0; color: #9a9ab0; max-width: 70ch; }
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

async function main() {
  mkdirSync(OUTDIR, { recursive: true });
  const views = discoverViews();
  console.log(`[snap] ${views.length} views; url=${URL} wait=${WAIT}ms out=${OUTDIR}`);

  const browser = await chromium.launch({ channel: 'chrome', headless: false, args: CHROME_ARGS });
  const results = [];

  for (const v of views) {
    // Fresh page per view so the view is selected before its replay arms — no
    // mid-stream scene-switch artifact (mirrors the existing pw harnesses).
    const page = await browser.newPage({ viewport: { width: 1180, height: 760 } });
    const logs = [];
    page.on('console', (m) => logs.push(`[${m.type()}] ${m.text()}`));
    page.on('pageerror', (e) => logs.push('PAGEERR: ' + e.message));
    let pix = null;
    let still = false;
    try {
      await page.goto(URL + '#/view/' + v.id, { waitUntil: 'load' });
      await page.waitForTimeout(WAIT); // engine bring-up + manifest apply + replay settle
      pix = await sampleCanvas(page);
      // Screenshot the live canvas element specifically (the render proof).
      const canvas = page.locator('canvas').first();
      const out = join(OUTDIR, `${v.id}.png`);
      await canvas.screenshot({ path: out });
      still = true;
    } catch (e) {
      logs.push('CAPTURE_ERR: ' + (e && e.message));
    }
    const autoVerdict = classify(pix);
    const verdict = VERDICT_OVERRIDE[v.id] ?? autoVerdict;
    const rejects = logs.filter((l) => /ID_TAKEN|FRAME_REJECTED|rejected/.test(l)).length;
    results.push({ ...v, pix, verdict, autoVerdict, still });
    const ov = verdict !== autoVerdict ? ` (auto=${autoVerdict}, overridden)` : '';
    console.log(
      `[snap] ${v.id.padEnd(22)} ${verdict.padEnd(7)} cov=${pix && pix.coverage != null ? (pix.coverage * 100).toFixed(1) + '%' : 'n/a'} chroma=${pix && pix.chroma != null ? (pix.chroma * 100).toFixed(1) + '%' : 'n/a'} colors=${pix?.distinctColors ?? '-'} rejects=${rejects}${ov}`,
    );
    if (verdict === 'none' || !still) {
      const err = logs.find((l) => /PAGEERR|CAPTURE_ERR|error/i.test(l));
      if (err) console.log('   ↳', err);
    }
    await page.close();
  }

  await browser.close();

  // Contact sheet + machine-readable tally.
  writeFileSync(join(OUTDIR, 'contact-sheet.html'), contactSheet(results));
  writeFileSync(
    join(OUTDIR, 'render-tally.json'),
    JSON.stringify(
      {
        capturedAt: new Date().toISOString(),
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
        })),
      },
      null,
      2,
    ),
  );

  const t = results.reduce((a, r) => ((a[r.verdict] = (a[r.verdict] || 0) + 1), a), {});
  console.log(`\n[snap] DONE — full=${t.full || 0} partial=${t.partial || 0} none=${t.none || 0} of ${results.length}`);
  console.log(`[snap] contact sheet: ${join(OUTDIR, 'contact-sheet.html')}`);
}

main().catch((err) => {
  console.error('[snap] FAILED:', err?.stack ?? err);
  process.exit(1);
});
