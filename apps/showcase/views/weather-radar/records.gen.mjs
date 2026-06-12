/* apps/showcase/views/weather-radar/records.gen.mjs
 *
 * GENERATOR (zero-dep node) for the LIVE weather-radar view (Linear ENC-575).
 *
 * ── What this emits ───────────────────────────────────────────────────────
 * A TEXTURE-FRAME replay: a `records.json` whose `textures` track carries N
 * RGBA8 frames (192x192, format 1) scheduled across a 20s loop. The replay
 * engine (apps/showcase/src/engine/useReplay.ts, ENC-568 texture timeline)
 * schedules each frame at its `t` and applies it via host.setTexturePixels,
 * LOOPING with the timeline — so the manifest's static `texturedQuad@1`
 * drawItem (bound to textureId RADAR_TEXTURE) animates by SWAPPING its texture.
 * `frames` stays empty: no vertex-buffer growth, the texture track alone drives
 * the animation.
 *
 * ── The evolving field ────────────────────────────────────────────────────
 * Instead of the static t=0 frame of heatmap.json, this synthesizes a moving
 * meteorological field: a handful of STORM CELLS (gaussian intensity blobs)
 * that DRIFT across the radar face on a prevailing wind vector, each PULSING in
 * intensity (cells grow, peak, then decay) on its own phase, plus a faint
 * roaming drizzle band. The continuous field is sampled per pixel and mapped
 * through the SAME NEXRAD palette the static manifest baked (dark -> green ->
 * yellow -> orange -> red -> magenta) — the palette stops below were recovered
 * by decoding the manifest's RADAR_PIXELS_B64 and binning color-vs-value, so
 * the look matches the static fallback. Drift WRAPS toroidally so the loop is
 * seamless (frame N == frame 0 conditions).
 *
 * Run:  node apps/showcase/views/weather-radar/records.gen.mjs
 * Out:  apps/showcase/views/weather-radar/records.json
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));

// ── must match manifest.ts ─────────────────────────────────────────────────
const VIEW_ID = 'weather-radar';
const RADAR_TEXTURE = 700; // logical textureId the texturedQuad drawItem binds
const WIDTH = 192;
const HEIGHT = 192;
const FORMAT_RGBA8 = 1;

// ── replay timing ──────────────────────────────────────────────────────────
const DURATION_MS = 20000;
const FRAME_COUNT = 50; // ~50 frames
const CADENCE_MS = DURATION_MS / FRAME_COUNT; // 400ms between sweeps

// ── NEXRAD reflectivity palette ────────────────────────────────────────────
// (value 0..1) -> [r,g,b]; recovered by decoding the static manifest texture
// and binning color against the underlying field value. Piecewise-linear.
const PALETTE = [
  [0.0, [10, 15, 25]], // near-zero: dark radar background
  [0.05, [10, 28, 28]],
  [0.13, [12, 60, 30]], // dark green
  [0.22, [23, 135, 42]], // green
  [0.3, [40, 180, 50]],
  [0.4, [110, 200, 46]], // green-yellow
  [0.5, [215, 226, 40]], // yellow-green
  [0.55, [235, 210, 35]], // yellow
  [0.62, [236, 181, 28]],
  [0.68, [240, 151, 20]], // orange
  [0.75, [232, 110, 22]],
  [0.82, [222, 49, 29]], // red
  [0.88, [220, 38, 60]],
  [0.95, [220, 32, 140]], // magenta
  [1.0, [220, 30, 165]],
];

function colormap(v) {
  if (v <= PALETTE[0][0]) return PALETTE[0][1];
  if (v >= PALETTE[PALETTE.length - 1][0]) return PALETTE[PALETTE.length - 1][1];
  for (let i = 1; i < PALETTE.length; i++) {
    const [hi, chi] = PALETTE[i];
    if (v <= hi) {
      const [lo, clo] = PALETTE[i - 1];
      const t = (v - lo) / (hi - lo);
      return [
        Math.round(clo[0] + (chi[0] - clo[0]) * t),
        Math.round(clo[1] + (chi[1] - clo[1]) * t),
        Math.round(clo[2] + (chi[2] - clo[2]) * t),
      ];
    }
  }
  return PALETTE[PALETTE.length - 1][1];
}

// ── moving storm cells ─────────────────────────────────────────────────────
// Field coords are normalized [0,1) in x,y (sampled at pixel centers). Cells
// drift on a shared wind plus their own jitter, wrapping toroidally so the 20s
// loop is seamless. Each cell pulses: peak intensity rises and falls over the
// loop on its own phase. Distances are computed on the torus (min wrap) so a
// cell straddling an edge stays a single blob.
const TAU = Math.PI * 2;

// Prevailing wind: total displacement over one full loop (wraps to 0).
const WIND = { x: 0.85, y: 0.28 };

const CELLS = [
  // x0,y0: position at phase 0. sx,sy: gaussian radii. peak: max intensity.
  // pulsePhase: where in the loop the cell peaks. pulses: pulse count over loop.
  { x0: 0.18, y0: 0.30, sx: 0.14, sy: 0.11, peak: 1.02, pulsePhase: 0.10, pulses: 1, drift: { x: 0.0, y: 0.0 } },
  { x0: 0.62, y0: 0.55, sx: 0.10, sy: 0.13, peak: 0.86, pulsePhase: 0.55, pulses: 1, drift: { x: -0.12, y: 0.18 } },
  { x0: 0.40, y0: 0.78, sx: 0.16, sy: 0.10, peak: 0.70, pulsePhase: 0.32, pulses: 2, drift: { x: 0.10, y: -0.10 } },
  { x0: 0.80, y0: 0.20, sx: 0.09, sy: 0.09, peak: 0.60, pulsePhase: 0.78, pulses: 1, drift: { x: -0.20, y: 0.25 } },
  { x0: 0.10, y0: 0.70, sx: 0.11, sy: 0.14, peak: 0.52, pulsePhase: 0.18, pulses: 2, drift: { x: 0.22, y: -0.05 } },
];

// toroidal signed delta (nearest wrap) in [-0.5,0.5)
function wrapDelta(a, b) {
  let d = a - b;
  d -= Math.round(d);
  return d;
}

function frac(x) {
  return x - Math.floor(x);
}

// Intensity field at (px,py) for loop phase `ph` in [0,1).
function fieldAt(px, py, ph) {
  let v = 0;

  // Faint roaming drizzle band: a broad low sinusoidal swell drifting with the
  // wind so the whole field gently breathes (never empty, never saturated).
  const bandX = frac(px - WIND.x * ph);
  v += 0.10 + 0.06 * Math.sin(TAU * (bandX * 1.5 + ph)) * Math.cos(TAU * py * 0.8);

  for (const c of CELLS) {
    // Current center: phase-0 position + (prevailing wind + own drift)*phase.
    const cx = frac(c.x0 + (WIND.x + c.drift.x) * ph);
    const cy = frac(c.y0 + (WIND.y + c.drift.y) * ph);

    // Pulse: smooth rise/fall (raised cosine) repeated `pulses` times/loop.
    const pulse = 0.5 - 0.5 * Math.cos(TAU * (ph * c.pulses - c.pulsePhase));
    const amp = c.peak * (0.35 + 0.65 * pulse);

    const dx = wrapDelta(px, cx) / c.sx;
    const dy = wrapDelta(py, cy) / c.sy;
    v += amp * Math.exp(-0.5 * (dx * dx + dy * dy));
  }

  return Math.max(0, Math.min(1, v));
}

// ── rasterize one frame to tightly-packed RGBA8, base64 it ─────────────────
function renderFrame(ph) {
  const buf = Buffer.alloc(WIDTH * HEIGHT * 4);
  let o = 0;
  for (let y = 0; y < HEIGHT; y++) {
    const fy = (y + 0.5) / HEIGHT;
    for (let x = 0; x < WIDTH; x++) {
      const fx = (x + 0.5) / WIDTH;
      const v = fieldAt(fx, fy, ph);
      const [r, g, b] = colormap(v);
      buf[o++] = r;
      buf[o++] = g;
      buf[o++] = b;
      buf[o++] = 255;
    }
  }
  return buf.toString('base64');
}

// ── generate the texture track ─────────────────────────────────────────────
const textures = [];
for (let f = 0; f < FRAME_COUNT; f++) {
  const ph = f / FRAME_COUNT; // 0 .. (1 - 1/N); wraps seamlessly back to 0
  const t = Math.round(f * CADENCE_MS);
  textures.push({
    t,
    textureId: RADAR_TEXTURE,
    width: WIDTH,
    height: HEIGHT,
    pixelsB64: renderFrame(ph),
    format: FORMAT_RGBA8,
  });
}

const records = {
  meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: FRAME_COUNT, cadenceMs: CADENCE_MS },
  frames: [],
  textures,
};

writeFileSync(join(HERE, 'records.json'), JSON.stringify(records));

const bytesPerFrame = WIDTH * HEIGHT * 4;
// eslint-disable-next-line no-console
console.log(
  `wrote records.json: ${FRAME_COUNT} texture frames, ${WIDTH}x${HEIGHT} RGBA8, ` +
    `tid ${RADAR_TEXTURE}, ${bytesPerFrame}B/frame, cadence ${CADENCE_MS}ms, ${DURATION_MS}ms loop`,
);
