#!/usr/bin/env node
/* apps/showcase/views/.build-composed-static.mjs
 *
 * AUTHORING-TIME precompute for the three STATIC composed showcase views
 * (Linear ENC-537 / T4.5): sankey, radial-seasonality, correlation-heatmap.
 *
 * These views build their geometry/texture FROM the market datasets at manifest
 * time. To keep the browser bundle free of the ~2.5 MB of raw dataset JSON, the
 * heavy numeric work is done HERE, once, against apps/showcase/data/market/*,
 * and the compact results are inlined into each view's manifest.ts as plain
 * constants. Re-run this when the datasets change:
 *
 *   node apps/showcase/views/.build-composed-static.mjs   # prints the baked blocks
 *
 * Zero dependencies; deterministic (the datasets are seeded). It only PRINTS the
 * computed arrays — it does not write manifest.ts (the manifests are authored by
 * hand around these baked constants, like the other showcase views).
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const dataDir = join(here, '..', 'data', 'market');
const SYMBOLS = ['AAPL', 'MSFT', 'NVDA', 'TSLA'];

function load(sym) {
  return JSON.parse(readFileSync(join(dataDir, sym + '.json'), 'utf8'));
}

const round = (x, n = 4) => Number(x.toFixed(n));

// ── series extraction ───────────────────────────────────────────────────────
const closes = {}; // sym -> [close per bucket]
const volumes = {}; // sym -> total traded volume
const lastPrices = {}; // sym -> [{t, value}]
for (const sym of SYMBOLS) {
  const d = load(sym);
  closes[sym] = d.updates.filter((u) => u.field === 'close').map((u) => u.value);
  volumes[sym] = d.updates.filter((u) => u.field === 'volume').reduce((a, u) => a + u.value, 0);
  lastPrices[sym] = d.updates.filter((u) => u.field === 'lastPrice').map((u) => ({ t: u.t, value: u.value }));
}

// ═════════════════════════════════════════════════════════════════════════════
// 1) CORRELATION HEATMAP — N×N Pearson correlation of per-bucket log returns.
// ═════════════════════════════════════════════════════════════════════════════
function logReturns(series) {
  const r = [];
  for (let i = 1; i < series.length; i++) r.push(Math.log(series[i] / series[i - 1]));
  return r;
}
function pearson(a, b) {
  const n = Math.min(a.length, b.length);
  let ma = 0, mb = 0;
  for (let i = 0; i < n; i++) { ma += a[i]; mb += b[i]; }
  ma /= n; mb /= n;
  let num = 0, da = 0, db = 0;
  for (let i = 0; i < n; i++) {
    const xa = a[i] - ma, xb = b[i] - mb;
    num += xa * xb; da += xa * xa; db += xb * xb;
  }
  const den = Math.sqrt(da * db);
  return den === 0 ? 0 : num / den;
}
const rets = SYMBOLS.map((s) => logReturns(closes[s]));
const N = SYMBOLS.length;
const corr = [];
for (let i = 0; i < N; i++) {
  const row = [];
  for (let j = 0; j < N; j++) row.push(round(pearson(rets[i], rets[j]), 4));
  corr.push(row);
}

// Diverging blue↔red colormap. -1 → blue, 0 → near-white, +1 → red.
function diverging(c) {
  const t = (c + 1) / 2; // [-1,1] -> [0,1]
  // blue (0.13,0.40,0.78) -> white (0.96,0.96,0.96) -> red (0.84,0.19,0.19)
  const lerp = (a, b, u) => a + (b - a) * u;
  let r, g, b;
  if (t < 0.5) {
    const u = t / 0.5;
    r = lerp(0.13, 0.96, u); g = lerp(0.40, 0.96, u); b = lerp(0.78, 0.96, u);
  } else {
    const u = (t - 0.5) / 0.5;
    r = lerp(0.96, 0.84, u); g = lerp(0.96, 0.19, u); b = lerp(0.96, 0.19, u);
  }
  return [Math.round(r * 255), Math.round(g * 255), Math.round(b * 255)];
}

// Rasterize the N×N matrix to an UPSCALED RGBA8 image (nearest-neighbour, so
// each correlation cell is a crisp block of pixels). cellPx px per matrix cell.
const cellPx = 64;
const dim = N * cellPx; // 256
const px = new Uint8Array(dim * dim * 4);
for (let py = 0; py < dim; py++) {
  // row 0 of the matrix at the TOP of the image; the texturedQuad shader maps
  // uv.y=0 to the quad's y0 corner, so image row 0 lands at the quad top.
  const i = Math.floor(py / cellPx);
  for (let pxX = 0; pxX < dim; pxX++) {
    const j = Math.floor(pxX / cellPx);
    const [r, g, b] = diverging(corr[i][j]);
    const o = (py * dim + pxX) * 4;
    const edge = (pxX % cellPx < 2) || (py % cellPx < 2);
    px[o] = edge ? Math.round(r * 0.45) : r;
    px[o + 1] = edge ? Math.round(g * 0.45) : g;
    px[o + 2] = edge ? Math.round(b * 0.45) : b;
    px[o + 3] = 255;
  }
}
const b64 = Buffer.from(px).toString('base64');

console.log('═══ CORRELATION HEATMAP ═══');
console.log('symbols:', SYMBOLS.join(', '));
console.log('corr matrix:');
for (let i = 0; i < N; i++) console.log('  ', corr[i].join('\t'));
console.log(`texture: ${dim}x${dim} RGBA8, base64 length = ${b64.length}`);
console.log('B64_START');
console.log(b64);
console.log('B64_END');

// ═════════════════════════════════════════════════════════════════════════════
// 2) RADIAL SEASONALITY — avg per-tick return by cyclic phase bin, polar→cart.
// ═════════════════════════════════════════════════════════════════════════════
// The 20 s synthetic tape has no real calendar seasonality, so we synthesize a
// cyclic "session clock": map each lastPrice tick's timestamp onto one of BINS
// phase bins (a synthetic trading day folded into the 20 s), and average the
// per-tick % return within each bin. This proves POLAR via build-time
// projection (angle=bin, radius=scaled avg-return), NOT a real seasonality claim.
const BINS = 24; // 24 "hours" of a synthetic session
// Pool all four symbols' tick returns into the phase bins for a smoother clock.
const binSum = new Array(BINS).fill(0);
const binCnt = new Array(BINS).fill(0);
const DUR = 20000;
for (const sym of SYMBOLS) {
  const lp = lastPrices[sym];
  for (let i = 1; i < lp.length; i++) {
    const ret = (lp[i].value - lp[i - 1].value) / lp[i - 1].value;
    const phase = (lp[i].t % DUR) / DUR; // 0..1 around the clock
    const bin = Math.min(BINS - 1, Math.floor(phase * BINS));
    binSum[bin] += ret;
    binCnt[bin] += 1;
  }
}
const avgRet = binSum.map((s, k) => (binCnt[k] ? s / binCnt[k] : 0));
// Scale returns (≈±0.001) into a visible radius band [rMin, rMax] in clip units.
const maxAbs = Math.max(...avgRet.map((v) => Math.abs(v)), 1e-9);
const rInner = 0.18; // baseline ring (radius for zero return)
const rSpan = 0.62; // ± swing about the baseline
// radius = rInner + (avgRet/maxAbs)*rSpan*0.5 ... keep within [~0.0, 0.80]
const radial = avgRet.map((v) => round(rInner + (v / maxAbs) * rSpan * 0.5, 5));
console.log('\n═══ RADIAL SEASONALITY ═══');
console.log('BINS:', BINS, 'maxAbsRet:', maxAbs.toExponential(3));
console.log('radius per bin (clip units):');
console.log('  [' + radial.join(', ') + ']');

// ═════════════════════════════════════════════════════════════════════════════
// 3) SANKEY — sector→sector flow ribbons synthesized from symbol volumes.
// ═════════════════════════════════════════════════════════════════════════════
// We treat each symbol as a "source sector" and synthesize a deterministic flow
// matrix into three "destination buckets" (Buy / Hold / Sell) proportional to
// the symbol's traded volume and its net price drift over the tape. This is a
// synthesized fund-flow Sankey (documented), not a real capital-flow dataset.
const dests = ['Inflow', 'Rotation', 'Outflow'];
const flow = {}; // src -> [toInflow, toRotation, toOutflow]
for (const sym of SYMBOLS) {
  const c = closes[sym];
  const drift = (c[c.length - 1] - c[0]) / c[0]; // net % change over tape
  const vol = volumes[sym];
  // Drift>0 skews to Inflow, drift<0 to Outflow, |drift| small -> Rotation.
  const inflow = Math.max(0, drift) * 40 + 0.2;
  const outflow = Math.max(0, -drift) * 40 + 0.2;
  const rotation = 0.6;
  const tot = inflow + rotation + outflow;
  flow[sym] = [
    round((inflow / tot) * vol, 1),
    round((rotation / tot) * vol, 1),
    round((outflow / tot) * vol, 1),
  ];
}
console.log('\n═══ SANKEY ═══');
console.log('sources:', SYMBOLS.join(', '), '| dests:', dests.join(', '));
console.log('flow matrix (volume units):');
for (const sym of SYMBOLS) console.log('  ', sym, '->', flow[sym].join('\t'));

// ═════════════════════════════════════════════════════════════════════════════
// 4) CORRELATION HEATMAP — ANIMATED TEXTURE TRACK (ENC-568).
// ═════════════════════════════════════════════════════════════════════════════
// The static heatmap (§1) shows the FULL-tape correlation. To prove the replay
// engine's animated TEXTURE track, we additionally compute a ROLLING-WINDOW
// correlation (a WIN-sample window slid across the per-bucket log returns) and
// rasterize one RGBA8 frame per window step, then WRITE them into the view's
// records.json as a `textures` TextureFrame[] track. useReplay swaps the bound
// texture via setTexturePixels at each frame's `t` (looping), so the heatmap
// evolves over time. (Unlike the printed blocks above, this section WRITES a
// file — records.json — because the frame data is too large to inline by hand.)
{
  const WIN = 16;            // rolling window length (log-return samples)
  const cellPx = 32;         // px per matrix cell (nearest-neighbour upscale)
  const dim = N * cellPx;    // 128
  const FRAME_MS = 1000;     // 1 s per frame
  const FRAMES = 10;         // target frame count (last window always included)
  const TEXTURE_ID = 700;    // MUST match correlation-heatmap/manifest.ts

  const rwRets = SYMBOLS.map((s) => logReturns(closes[s]));
  const T = Math.min(...rwRets.map((r) => r.length));

  function rasterize(c) {
    const px = new Uint8Array(dim * dim * 4);
    for (let py = 0; py < dim; py++) {
      const i = Math.floor(py / cellPx);
      for (let pxX = 0; pxX < dim; pxX++) {
        const j = Math.floor(pxX / cellPx);
        const [r, g, b] = diverging(c[i][j]);
        const o = (py * dim + pxX) * 4;
        const edge = (pxX % cellPx < 2) || (py % cellPx < 2);
        px[o] = edge ? Math.round(r * 0.45) : r;
        px[o + 1] = edge ? Math.round(g * 0.45) : g;
        px[o + 2] = edge ? Math.round(b * 0.45) : b;
        px[o + 3] = 255;
      }
    }
    return px;
  }

  const starts = [];
  for (let s = 0; s + WIN <= T; s++) starts.push(s);
  const step = Math.max(1, Math.floor(starts.length / FRAMES));
  const chosen = [];
  for (let k = 0; k < starts.length && chosen.length < FRAMES; k += step) chosen.push(starts[k]);
  if (starts.length && chosen[chosen.length - 1] !== starts[starts.length - 1]) {
    chosen.push(starts[starts.length - 1]);
  }

  const frames = chosen.map((s, k) => {
    const win = rwRets.map((r) => r.slice(s, s + WIN));
    const c = [];
    for (let i = 0; i < N; i++) {
      const row = [];
      for (let j = 0; j < N; j++) row.push(pearson(win[i], win[j]));
      c.push(row);
    }
    return {
      t: k * FRAME_MS,
      textureId: TEXTURE_ID,
      width: dim,
      height: dim,
      pixelsB64: Buffer.from(rasterize(c)).toString('base64'),
      format: 1,
    };
  });

  const records = {
    meta: {
      viewId: 'correlation-heatmap',
      durationMs: frames.length * FRAME_MS,
      frameCount: 0,
      cadenceMs: FRAME_MS,
    },
    frames: [],      // texture-only timeline (no binary records)
    textures: frames,
  };
  const outPath = join(here, 'correlation-heatmap', 'records.json');
  writeFileSync(outPath, JSON.stringify(records));
  console.log('\n═══ CORRELATION HEATMAP — ANIMATED TRACK (ENC-568) ═══');
  console.log(`wrote ${frames.length} frames (${dim}x${dim} RGBA8, ${FRAME_MS}ms apart) -> ${outPath}`);
}

console.log('\nDone.');
