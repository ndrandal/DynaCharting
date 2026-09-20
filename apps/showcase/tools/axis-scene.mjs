#!/usr/bin/env node
/* apps/showcase/tools/axis-scene.mjs — ENC-1253 (chart-quality-bar SPEC D5/D7/D10)
 *
 * Dump the scene JSON that `specs/2026-09-19-chart-quality-bar/harness/score.py`
 * grades, FROM THE RUNNING CHART, over CDP.
 *
 * The scorer grades a scene declaration corroborated by pixel probes, and
 * `harness/scenes/README.md` states where that declaration is supposed to come
 * from: "a scene dumped by the renderer cannot describe an axis the renderer did
 * not draw." Hand-writing one would re-introduce exactly the failure this
 * project exists to end — a number beside the geometry that nobody measured.
 *
 * So this reads `window.__dcEngineAxis[viewId].scene`, which `useEngineAxis`
 * publishes from the SAME `AxisPlan` the engine was driven with: every declared
 * label box is where a glyph run was actually laid out, and every declared
 * gridline row is one the engine actually drew a line on.
 *
 * What it does NOT invent, and why each absence is stated in the output rather
 * than filled in:
 *
 *   tier0.rampCheck — needs `scripts/tier0.sh`, which needs a `-DDC_FETCH_DAWN=ON`
 *     build (LIMITATIONS.md DC-L01) and a hardware adapter. Absent ⇒ tier 0
 *     UNPROVEN ⇒ every tier above it is NOT SCORED. Run the scorer with
 *     `--diagnose` to see the tier-1 verdict this work moves.
 *   series.symbol — the showcase replays a captured tape, not the live feed. SPEC
 *     D2 binds the REFERENCE chart to the real path; this is the gallery.
 *
 * Usage:
 *   node apps/showcase/tools/axis-scene.mjs \
 *     --port 9514 --view candles-aapl --raster /tmp/shot.png --out /tmp/scene.json
 *
 * `--raster` is the still `harness/shoot-live.mjs --mode canvas` wrote; its
 * sha256 is stamped into the scene so the scorer refuses a scene/raster pair
 * that does not belong together.
 */

import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[++i];
}
const PORT = args.port ?? '9222';
const VIEW = args.view ?? 'candles-aapl';
const RASTER = args.raster;
const OUT = args.out;
if (!RASTER || !OUT) {
  console.error('usage: axis-scene.mjs --port <cdp> --view <id> --raster <png> --out <json>');
  process.exit(2);
}

/** One CDP evaluate against the first page target. Zero-dep, like shoot-live.mjs. */
async function evaluate(expression) {
  const targets = await (await fetch(`http://localhost:${PORT}/json`)).json();
  const page = targets.find((t) => t.type === 'page');
  if (!page) throw new Error(`no page target on CDP port ${PORT}`);
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((res, rej) => {
    ws.onopen = res;
    ws.onerror = rej;
  });
  try {
    return await new Promise((res) => {
      const onMsg = (ev) => {
        const m = JSON.parse(ev.data);
        if (m.id !== 1) return;
        ws.removeEventListener('message', onMsg);
        res(m.result?.result?.value);
      };
      ws.addEventListener('message', onMsg);
      ws.send(
        JSON.stringify({
          id: 1,
          method: 'Runtime.evaluate',
          params: { expression, returnByValue: true },
        }),
      );
    });
  } finally {
    ws.close();
  }
}

const payload = await evaluate(
  `JSON.stringify({
     report: (window.__dcEngineAxis || {})[${JSON.stringify(VIEW)}] || null,
     domain: (window.__dcAxisDomain || {})[${JSON.stringify(VIEW)}] || null,
     svgPresent: !!document.querySelector('.chrome-axis-svg'),
     canvas: (() => { const c = document.querySelector('canvas.engine-canvas');
                      return c ? { w: c.width, h: c.height } : null; })(),
   })`,
);
if (!payload) {
  console.error('CDP returned nothing — is the page open on the view?');
  process.exit(3);
}
const { report, domain, svgPresent, canvas } = JSON.parse(payload);
if (!report?.scene) {
  console.error(
    `the chart published no engine axis for view ${VIEW}` +
      (report ? ` (fontLoaded=${report.fontLoaded}, attempts=${report.labelAttempts})` : ''),
  );
  process.exit(4);
}

const sha256 = createHash('sha256').update(readFileSync(RASTER)).digest('hex');
const s = report.scene;

// The candle colours this view draws with. They are a literal in the view's own
// manifest, and saying so in the scene is the point: SPEC D1's tier-3 row asks
// where a colour CAME FROM, and "the manifest typed it" is the honest answer for
// the data marks even now that the axis furniture comes from a Theme (D4 /
// ENC-1259 owns closing that half).
const CANDLE_PALETTE = [
  { rgb: [51, 191, 115], role: 'dataMark', what: 'up candle — manifest.ts literal' },
  { rgb: [217, 76, 76], role: 'dataMark', what: 'down candle — manifest.ts literal' },
];

const scene = {
  id: `showcase-${VIEW}-engine-axis`,
  description:
    `${VIEW} in the DynaCharting showcase with the axis drawn BY THE ENGINE ` +
    '(ENC-1253): gridlines, tick marks and spine as lineAA@1 clip-space geometry, ' +
    'tick labels and axis titles as textSDF@1 glyph runs, captured canvas-only ' +
    `(SPEC D10). SVG overlay present in the DOM at capture: ${svgPresent} — ` +
    'irrelevant to this raster by construction, which is the point of D10.',
  raster: { path: RASTER, sha256, canvas },

  tier0: {
    rampCheck: null,
    why:
      'No tier-0 ramp-check artifact for this build. scripts/tier0.sh (ENC-1249) ' +
      'needs a -DDC_FETCH_DAWN=ON build, which the default configure excludes at ' +
      'configure time (LIMITATIONS.md DC-L01), and its documented lavapipe ' +
      'fallback is a software adapter, which D8 rules inadmissible anyway. So ' +
      'tier 0 is UNPROVEN and gates the ladder — run score.py --diagnose to read ' +
      'the tier-1 verdict.',
  },

  theme: {
    ...s.theme,
    why:
      "The AXIS furniture's colours are dc::darkTheme()'s gridColor / tickColor / " +
      'labelColor, mirrored into TypeScript by packages/dc-wasm/src/chart/theme.ts ' +
      '(theme.test.ts parses the C++ and fails if the two drift). The DATA marks ' +
      'are still manifest literals — D4 / ENC-1259.',
  },

  text: s.text,
  grid: s.grid,
  axis: s.axis,
  plot: s.plot,

  series: {
    kind: 'candles',
    symbol: VIEW,
    xDomain: 'time',
    domainSource: domain?.x?.source ?? 'unknown',
    why:
      'The x axis is labelled from a MEASURED TimeBasis fitted off the replayed ' +
      "stream (ENC-1254), on the tape's own timeline — the capture carries no " +
      'epoch, so the labels are elapsed-from-tape-zero instants in UTC, not a ' +
      'wall clock. LIMITATIONS.md DC-L16.',
  },

  palette: [...s.palette, ...CANDLE_PALETTE],
};

writeFileSync(OUT, `${JSON.stringify(scene, null, 2)}\n`);
console.log(`wrote ${OUT}`);
console.log(
  `  ${s.text.length} engine text runs, ${s.grid.length} gridlines, ` +
    `${s.axis.spines.length} spines, plot ${JSON.stringify(s.plot)}`,
);
console.log(`  raster sha256 ${sha256.slice(0, 16)}  svg overlay in DOM: ${svgPresent}`);
