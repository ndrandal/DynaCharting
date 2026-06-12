/* apps/showcase/views/streamgraph/manifest.ts
 *
 * LIVE — "streamgraph / stacked area" flowing bands over time (Linear ENC-583).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A streamgraph stacks several time series into flowing bands around a wandering
 * center baseline (ThemeRiver/"wiggle"). Six bands, each with a thickness that
 * EVOLVES over time, are stacked and centered on a wiggle baseline
 * g0(t) = -½·Σ thicknesses, so the silhouette flows symmetrically about y=0 —
 * the signature streamgraph organic shape.
 *
 * ── LIVE via geometry-frame replay ────────────────────────────────────────
 * The bands no longer sit still. records.gen.mjs re-evaluates each band's
 * thickness (a smooth sum-of-sines), re-stacks + re-tessellates the whole
 * silhouette at N timesteps, and records.json carries one full-buffer
 * UPDATE_RANGE per timestep. ENC-569's triGradient backend re-reads + redraws
 * the UPDATE_RANGE'd vertex buffer every frame, so the river FLOWS and the
 * silhouette undulates live. The VERTEX COUNT IS CONSTANT (6 bands × 39 segments
 * × 6 verts = 1404), so the buffer is PRE-SIZED once and every frame is a stable
 * full-buffer overwrite.
 *
 * ── Geometry (triGradient@1 / pos2_color4) ────────────────────────────────
 * Each band is the ribbon between its lower and upper stacked offsets across
 * time, emitted as a pos2_color4 triangle list (two triangles per segment).
 * Per-vertex color gives each band its hue with a slight left→right brightness
 * flow; color rides the vertex buffer, so ONE triGradient draw item renders all
 * bands. Vertices are authored directly in clip space — no transform.
 *
 * This file is the SINGLE SOURCE OF TRUTH for the at-rest seed: the frame-0
 * geometry below is computed from the SAME flow math (phase 0) as
 * records.gen.mjs, so the seeded UPDATE_RANGE matches records.json frame 0
 * exactly before the replay takes over.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const BAND_BUFFER = 601; // pos2_color4 triangles — 6 floats/vert, 24B/vert
const BAND_GEOMETRY = 201;
const BAND_DRAWITEM = 301;
const BAND_STRIDE = 24; // pos2 (8B) + color4 (16B)

// ── stream model (mirrors records.gen.mjs) ─────────────────────────────────
const NB = 6; // number of bands
const T = 40; // time buckets along x (segments = T-1)
const BANDS: { hue: [number, number, number] }[] = [
  { hue: [0.38, 0.72, 0.55] }, // teal-green
  { hue: [0.36, 0.55, 0.92] }, // blue
  { hue: [0.80, 0.58, 0.32] }, // amber
  { hue: [0.82, 0.40, 0.55] }, // magenta
  { hue: [0.55, 0.45, 0.85] }, // violet
  { hue: [0.40, 0.78, 0.78] }, // cyan
];

const X0 = -0.9;
const X1 = 0.9;
const xAt = (t: number) => X0 + (t / (T - 1)) * (X1 - X0);

// Band thickness at bucket t, phase p — IDENTICAL to records.gen.mjs.
function thickness(s: number, t: number, p: number): number {
  const tt = t / (T - 1);
  const ph = (s / NB) * Math.PI * 2;
  const tau = 2 * Math.PI * p;
  const base = 1.0;
  const wave =
    0.55 * Math.sin(2 * Math.PI * (tt * 1.3 + 0.2 * s) + tau + ph) +
    0.3 * Math.sin(2 * Math.PI * (tt * 0.6 - 0.15 * s) - tau * 0.5 + ph * 1.7) +
    0.18 * Math.sin(2 * Math.PI * (tt * 2.1) + tau * 1.5 + ph * 0.5);
  return Math.max(0.18, base + wave);
}

// Peak-total probe → vertical scale (IDENTICAL to records.gen.mjs).
function peakTotal(): number {
  let mx = 0;
  const PP = 80;
  for (let k = 0; k < PP; k++) {
    const p = k / PP;
    for (let t = 0; t < T; t++) {
      let tot = 0;
      for (let s = 0; s < NB; s++) tot += thickness(s, t, p);
      if (tot > mx) mx = tot;
    }
  }
  return mx;
}
const Y_SCALE = 1.7 / peakTotal();

function offsets(t: number, p: number): { lo: number[]; hi: number[] } {
  let total = 0;
  for (let s = 0; s < NB; s++) total += thickness(s, t, p);
  let acc = -0.5 * total;
  const lo: number[] = [];
  const hi: number[] = [];
  for (let s = 0; s < NB; s++) {
    lo[s] = acc * Y_SCALE;
    acc += thickness(s, t, p);
    hi[s] = acc * Y_SCALE;
  }
  return { lo, hi };
}

/** Tessellate the silhouette at phase p (frame 0 uses p=0). */
function tessellate(p: number): number[] {
  const floats: number[] = [];
  const lohi = Array.from({ length: T }, (_, t) => offsets(t, p));
  for (let s = 0; s < NB; s++) {
    const [hr, hg, hb] = BANDS[s].hue;
    const colAt = (t: number): [number, number, number, number] => {
      const flow = 0.75 + 0.25 * (t / (T - 1));
      return [hr * flow, hg * flow, hb * flow, 0.92];
    };
    for (let t = 0; t < T - 1; t++) {
      const xa = xAt(t);
      const xb = xAt(t + 1);
      const la = lohi[t].lo[s];
      const ua = lohi[t].hi[s];
      const lb = lohi[t + 1].lo[s];
      const ub = lohi[t + 1].hi[s];
      const [ar, ag, ab, aa] = colAt(t);
      const [br, bg, bb, ba] = colAt(t + 1);
      floats.push(xa, la, ar, ag, ab, aa);
      floats.push(xa, ua, ar, ag, ab, aa);
      floats.push(xb, ub, br, bg, bb, ba);
      floats.push(xa, la, ar, ag, ab, aa);
      floats.push(xb, ub, br, bg, bb, ba);
      floats.push(xb, lb, br, bg, bb, ba);
    }
  }
  return floats;
}

const VERTS = tessellate(0); // frame-0 seed (phase 0)
const VERTEX_COUNT = VERTS.length / 6; // 6 floats/vertex → 1404 verts
const BAND_BYTELENGTH = VERTEX_COUNT * BAND_STRIDE; // stable across all frames

export const manifest: SceneManifest = {
  label: 'Streamgraph — bands flow over time',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.04, g: 0.05, b: 0.07, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // PRE-SIZED band vertex buffer: the geometry-frame UPDATE_RANGE frames
    // (records.json) overwrite the whole buffer in place every timestep, so it is
    // sized once to the constant band geometry (VERTEX_COUNT × 24B) and never
    // grows. vertexCount is fixed (bands flex thickness, count is constant).
    { cmd: 'createBuffer', id: BAND_BUFFER, byteLength: BAND_BYTELENGTH },
    { cmd: 'createGeometry', id: BAND_GEOMETRY, vertexBufferId: BAND_BUFFER, format: 'pos2_color4', vertexCount: VERTEX_COUNT },

    { cmd: 'createDrawItem', id: BAND_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: BAND_DRAWITEM, pipeline: 'triGradient@1', geometryId: BAND_GEOMETRY },
  ],
  // Frame-0 geometry seeded via a one-time UPDATE_RANGE into the pre-sized buffer
  // (matches records.json frame 0). The replay then streams the per-timestep
  // UPDATE_RANGE frames that re-flow the bands live.
  uploads: [{ bufferId: BAND_BUFFER, op: 'updateRange', offsetBytes: 0, floats: VERTS } as BufferUpload],
};
