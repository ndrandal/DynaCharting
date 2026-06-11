/* apps/showcase/views/volume-profile/manifest.ts
 *
 * COMPOSED view — "Sierra Chart / ToS TPO" volume-by-price profile, rendered by
 * the SCALAR-FAN / fixed-mode technique (Linear ENC-535 / T4.3).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A volume profile is a horizontal histogram: a vertical column of price
 * buckets, each a horizontal bar whose length ∝ the volume traded at that price.
 * Like the order book it is a *current-state vector* (a fixed set of buckets
 * rewritten as volume accrues), so it rides embassy's **fixed** write mode: N
 * `profile.bucket.k` subscriptions, each bound `{mode:"fixed", offset:k·16+8}`,
 * writing via UPDATE_RANGE (op 2) into one pre-sized rect4 buffer the geometry
 * reads as a live snapshot. One scalar-fan, one buffer, N bars.
 *
 * ── The profile envelope ──────────────────────────────────────────────────
 * The mock GMA derives every synthetic `profile.bucket.k` field from one
 * mid-price oscillator, so the LIVE value each bar receives is ~the same across
 * buckets (it drives the bar's right edge, x1). To give the column the
 * characteristic bell-shaped profile envelope (most volume near the mid price,
 * tapering at the extremes) we bake a per-bucket BASELINE (x0) once via the
 * manifest `uploads`: center buckets get a low x0 (long bar), edge buckets a
 * high x0 (short bar). So bar length = liveValue − bakedBaseline_k — the live
 * fixed-mode value still drives every bar each tick, and the baked baseline
 * supplies the price-bucket shape that a real per-bucket volume feed would.
 *
 * No `growth`: fixed buffers do not grow; the replay just streams UPDATE_RANGE.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const TRANSFORM = 150;
const PROFILE_BUFFER = 601; // rect4 fixed snapshot, N buckets × 16B
const PROFILE_GEOMETRY = 201;
const PROFILE_DRAWITEM = 301;

const BUCKETS = 24;
const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1

// At capture (--seed 1) the synthetic profile.bucket.* fields oscillate through
// ~[400,425]. We bake the baseline near that floor and shape it per bucket.
const BASE_X = 398; // baseline for the PEAK (center) bucket
const SPREAD = 24; // how much the edge buckets are pulled toward the value (shorter bars)

/** Gaussian-ish envelope weight in [0,1], peaking at the center bucket. */
function envelope(k: number): number {
  const c = (BUCKETS - 1) / 2;
  const z = (k - c) / (BUCKETS * 0.28);
  return Math.exp(-0.5 * z * z);
}

/**
 * Static rect corners (one UPDATE_RANGE upload). Per bucket k: a per-bucket
 * baseline x0 that forms the profile envelope (center→low x0→long bar,
 * edge→high x0→short bar), the bucket's y-band, and x1 seeded to x0
 * (zero-length until the first fixed tick overwrites it). Buckets stack bottom
 * (k=0) to top (k=BUCKETS-1).
 */
function staticCorners(): BufferUpload {
  const floats: number[] = [];
  for (let k = 0; k < BUCKETS; k++) {
    const x0 = BASE_X + (1 - envelope(k)) * SPREAD; // envelope baseline
    const y0 = k + 0.12;
    const y1 = k + 0.88;
    floats.push(x0, y0, x0, y1); // x0, y0, x1(seed=x0), y1
  }
  return { bufferId: PROFILE_BUFFER, op: 'updateRange', offsetBytes: 0, floats };
}

export const manifest: SceneManifest = {
  label: 'Volume Profile — volume by price (Sierra/ToS)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Pre-sized fixed-mode buffer (byteLength = BUCKETS × recordBytes).
    { cmd: 'createBuffer', id: PROFILE_BUFFER, byteLength: BUCKETS * RECORD_BYTES },
    { cmd: 'createGeometry', id: PROFILE_GEOMETRY, vertexBufferId: PROFILE_BUFFER, format: 'rect4', vertexCount: BUCKETS },

    // clip_x = sx*x + tx : BASE_X(398)→0, ~426→~0.90 ⇒ sx ≈ 0.0321, tx ≈ -12.78
    // clip_y = sy*y + ty : bucket 0→-0.90, bucket 24→+0.90 ⇒ sy = 0.075, ty = -0.90
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: 0.0321, sy: 0.075, tx: -12.78, ty: -0.9 },

    { cmd: 'createDrawItem', id: PROFILE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: PROFILE_DRAWITEM, pipeline: 'instancedRect@1', geometryId: PROFILE_GEOMETRY },
    // Single uniform color (instancedRect is one color per draw item): a calm
    // composed-tier blue-violet, slightly translucent so overlapping anti-alias
    // edges read cleanly.
    { cmd: 'setDrawItemStyle', drawItemId: PROFILE_DRAWITEM, r: 0.38, g: 0.55, b: 0.92, a: 0.9 },
    { cmd: 'attachTransform', drawItemId: PROFILE_DRAWITEM, transformId: TRANSFORM },
  ],
  // Baked envelope baselines; the fixed-mode UPDATE_RANGE frames then overwrite
  // only x1 (offset k·16+8) of each bucket every tick → the live profile.
  uploads: [staticCorners()],
};
