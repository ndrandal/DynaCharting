/* apps/showcase/views/contour/records.gen.mjs
 *
 * GENERATOR (zero-dep node) for the LIVE contour view (Linear ENC-578).
 *
 * ── What this emits ───────────────────────────────────────────────────────
 * A TEXTURE-FRAME replay (ENC-568 foundation): a `records.json` whose `frames`
 * are empty and whose `textures` track is N RGBA8 frames, each base64 of a
 * 256×256 topographic colormap. useReplay schedules each at its `t` and applies
 * it via host.setTexturePixels(textureId, pixels, w, h, format), LOOPING — so
 * the single `texturedQuad@1` instance in manifest.ts SWAPS its colormap over
 * time and the iso-bands + isolines visibly morph.
 *
 * ── The evolving field ────────────────────────────────────────────────────
 * The scalar field is the SAME synthetic 16×16 surface that produced the
 * static frame-0 baseline baked into manifest.ts — a travelling 2D sine wave
 * plus a slow Gaussian blob whose centre ORBITS the grid (lifted verbatim from
 * apps/showcase/data/gen-synthetic.mjs genHeatmap). We evaluate that field as a
 * continuous function of time across the full 20s tape, so peaks rise, the
 * blob orbits, saddles drift, and the contour rings open/close frame to frame.
 *
 * ── Rasterization (identical recipe to the baked manifest texture) ─────────
 * Each frame: bilinearly upsample the 16×16 field to 256×256, quantize into
 * 9 iso-bands (band = floor(v·9)), color each band through the SAME topographic
 * ramp recovered from the manifest's baked texture, and draw a crisp white
 * isoline wherever the quantized band changes between 4-neighbours (the
 * extracted marching-squares contour). RGBA8, row 0 = top → base64.
 *
 * Run:  node apps/showcase/views/contour/records.gen.mjs
 * Out:  apps/showcase/views/contour/records.json
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));

// ── texture / replay constants (MUST match manifest.ts) ────────────────────
const VIEW_ID = 'contour';
const TEXTURE_ID = 80; // manifest TEXTURE_ID
const TEX_W = 256; // manifest texture width
const TEX_H = 256; // manifest texture height
const FORMAT_RGBA8 = 1;

const DURATION_MS = 20000;
const FRAME_COUNT = 48; // ~417ms cadence — 48 frames over 20s
const CADENCE_MS = DURATION_MS / FRAME_COUNT;

// ── field model (verbatim from data/gen-synthetic.mjs genHeatmap) ──────────
const ROWS = 16;
const COLS = 16;
const SRC_FRAMES = 200; // the source field's own 10fps tape length (20s / 100ms)

// Sample the 16×16 scalar field at a CONTINUOUS source-frame position `sf`
// (so we can resample at our own cadence and still trace the exact same
// orbiting-blob + travelling-wave surface). Returns a ROWS×COLS array.
function fieldAt(sf) {
  const phase = (sf / SRC_FRAMES) * 2 * Math.PI * 3; // 3 wave cycles / tape
  const bcx = COLS / 2 + (COLS / 3) * Math.cos((sf / SRC_FRAMES) * 2 * Math.PI);
  const bcy = ROWS / 2 + (ROWS / 3) * Math.sin((sf / SRC_FRAMES) * 2 * Math.PI);
  const g = [];
  for (let r = 0; r < ROWS; r++) {
    const row = [];
    for (let c = 0; c < COLS; c++) {
      const wave = 0.5 + 0.5 * Math.sin((c / COLS) * 2 * Math.PI * 2 + (r / ROWS) * Math.PI + phase);
      const d2 = (c - bcx) ** 2 + (r - bcy) ** 2;
      const blob = Math.exp(-d2 / 8);
      const v = Math.min(1, Math.max(0, 0.6 * wave + 0.7 * blob));
      // match gen-synthetic's round(v, 4) so the surface is bit-for-bit the tape's
      row.push(Math.round(v * 1e4) / 1e4);
    }
    g.push(row);
  }
  return g;
}

// Bilinear sample of the 16×16 grid at fractional cell coords (clamped).
function bilerp(g, fx, fy) {
  fx = Math.max(0, Math.min(COLS - 1, fx));
  fy = Math.max(0, Math.min(ROWS - 1, fy));
  const x0 = Math.floor(fx);
  const y0 = Math.floor(fy);
  const x1 = Math.min(x0 + 1, COLS - 1);
  const y1 = Math.min(y0 + 1, ROWS - 1);
  const tx = fx - x0;
  const ty = fy - y0;
  const a = g[y0][x0];
  const b = g[y0][x1];
  const c = g[y1][x0];
  const d = g[y1][x1];
  return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
}

// ── topographic colormap — recovered from the manifest's baked texture ─────
// 9 iso-bands, low→high elevation, plus the crisp isoline white. These are the
// EXACT 10 colors present in the static frame-0 texture in manifest.ts.
const NB = 9;
const BANDS = [
  [10, 30, 48],    // 0  deep water
  [13, 55, 71],    // 1
  [20, 82, 92],    // 2
  [31, 114, 107],  // 3
  [76, 145, 97],   // 4  lowland green
  [141, 168, 93],  // 5
  [195, 166, 87],  // 6  foothills tan
  [215, 152, 90],  // 7
  [236, 224, 196], // 8  peaks / snow
];
const ISOLINE = [245, 250, 240];

function band(v) {
  let b = Math.floor(v * NB);
  if (b < 0) b = 0;
  if (b > NB - 1) b = NB - 1;
  return b;
}

// ── rasterize one field → 256×256 RGBA8 (row 0 = top) → Buffer ─────────────
function rasterize(g) {
  const px = new Uint8Array(TEX_W * TEX_H * 4);

  // Pass 1: band index per pixel (upsample edge-to-edge so the grid corners map
  // to the texture corners — matches the manifest's baked baseline mapping).
  const bg = new Int16Array(TEX_W * TEX_H);
  for (let y = 0; y < TEX_H; y++) {
    const fy = (y / (TEX_H - 1)) * (ROWS - 1);
    for (let x = 0; x < TEX_W; x++) {
      const fx = (x / (TEX_W - 1)) * (COLS - 1);
      bg[y * TEX_W + x] = band(bilerp(g, fx, fy));
    }
  }

  // Pass 2: fill band color, then overwrite isoline pixels where the quantized
  // band changes between 4-neighbours (the extracted marching-squares contour).
  for (let y = 0; y < TEX_H; y++) {
    for (let x = 0; x < TEX_W; x++) {
      const idx = y * TEX_W + x;
      const b = bg[idx];
      let iso = false;
      if (x + 1 < TEX_W && bg[idx + 1] !== b) iso = true;
      else if (x > 0 && bg[idx - 1] !== b) iso = true;
      else if (y + 1 < TEX_H && bg[idx + TEX_W] !== b) iso = true;
      else if (y > 0 && bg[idx - TEX_W] !== b) iso = true;
      const col = iso ? ISOLINE : BANDS[b];
      const o = idx * 4;
      px[o] = col[0];
      px[o + 1] = col[1];
      px[o + 2] = col[2];
      px[o + 3] = 255;
    }
  }
  return Buffer.from(px.buffer, px.byteOffset, px.byteLength);
}

// ── generate the texture-frame track ───────────────────────────────────────
const textures = [];
for (let f = 0; f < FRAME_COUNT; f++) {
  // Map our frame onto the source field's continuous tape so one full loop of
  // our 20s replay traces one full loop of the field (blob completes its orbit).
  const sf = (f / FRAME_COUNT) * SRC_FRAMES;
  const g = fieldAt(sf);
  const buf = rasterize(g);
  if (buf.length !== TEX_W * TEX_H * 4) {
    throw new Error(`frame ${f}: ${buf.length}B, expected ${TEX_W * TEX_H * 4}`);
  }
  textures.push({
    t: Math.round(f * CADENCE_MS),
    textureId: TEXTURE_ID,
    width: TEX_W,
    height: TEX_H,
    pixelsB64: buf.toString('base64'),
    format: FORMAT_RGBA8,
  });
}

const records = {
  meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: FRAME_COUNT, cadenceMs: CADENCE_MS },
  frames: [],
  textures,
};

writeFileSync(join(HERE, 'records.json'), JSON.stringify(records));

// eslint-disable-next-line no-console
console.log(
  `wrote records.json: ${FRAME_COUNT} texture frames @ ${CADENCE_MS}ms, ` +
    `${TEX_W}×${TEX_H} RGBA8 (${TEX_W * TEX_H * 4}B/frame), textureId ${TEXTURE_ID}`,
);
