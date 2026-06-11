/* apps/showcase/views/streamgraph/manifest.ts
 *
 * COMPOSED — "streamgraph / stacked area" flowing bands over time, a STATIC
 * COMPUTED-GEOMETRY view (Linear ENC-536 / T4.4). The stacked baselines are
 * TESSELLATED AT MANIFEST-BUILD TIME from the market datasets and embedded as
 * static manifest `uploads` — no streaming, capture, or replay.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A streamgraph stacks several time series into flowing bands around a wandering
 * center baseline (ThemeRiver/"wiggle"). We take per-bucket traded volume for
 * the four symbols (40 buckets each). At each time bucket the four values are
 * stacked; the whole stack is CENTERED on a wiggle baseline g0(t) = -½·Σ values,
 * so the silhouette flows symmetrically about y=0 rather than sitting on a flat
 * axis — the signature streamgraph organic shape.
 *
 * ── Geometry (triGradient@1 / pos2_color4) ────────────────────────────────
 * Each band is the ribbon between its lower and upper stacked offsets across
 * time, emitted as a pos2_color4 triangle strip (two triangles per time step).
 * Per-vertex color gives each band its hue with a slight left→right brightness
 * flow; color rides the vertex buffer, so ONE triGradient draw item renders all
 * four bands. Vertices are authored directly in clip space — no transform.
 *
 * No `growth`, no streaming: the bands are fully resolved at build time.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';
import aapl from '../../data/market/AAPL.json';
import msft from '../../data/market/MSFT.json';
import nvda from '../../data/market/NVDA.json';
import tsla from '../../data/market/TSLA.json';

const PANE = 100;
const LAYER = 101;
const BAND_BUFFER = 601; // pos2_color4 triangles
const BAND_GEOMETRY = 201;
const BAND_DRAWITEM = 301;

type Update = { field: string; value: number };
const BANDS: { name: string; data: { updates: Update[] }; hue: [number, number, number] }[] = [
  { name: 'AAPL', data: aapl as { updates: Update[] }, hue: [0.38, 0.72, 0.55] },
  { name: 'MSFT', data: msft as { updates: Update[] }, hue: [0.36, 0.55, 0.92] },
  { name: 'NVDA', data: nvda as { updates: Update[] }, hue: [0.80, 0.58, 0.32] },
  { name: 'TSLA', data: tsla as { updates: Update[] }, hue: [0.82, 0.40, 0.55] },
];

function volumeSeries(updates: Update[]): number[] {
  return updates.filter((u) => u.field === 'volume').map((u) => u.value);
}

// --- build-time stacking with a centered wiggle baseline -------------------
const SERIES = BANDS.map((b) => volumeSeries(b.data.updates));
const T = SERIES[0].length; // 40 time buckets
const NB = BANDS.length;

// Peak total over time → vertical scale that keeps the widest stack on-screen.
let maxTotal = 0;
for (let t = 0; t < T; t++) {
  let tot = 0;
  for (let s = 0; s < NB; s++) tot += SERIES[s][t];
  if (tot > maxTotal) maxTotal = tot;
}

// Clip framing: x over the time axis, y scaled so the fattest stack fills ~1.7.
const X0 = -0.9;
const X1 = 0.9;
const Y_SCALE = 1.7 / maxTotal; // value → clip-y units
const xAt = (t: number) => X0 + (t / (T - 1)) * (X1 - X0);

/** Lower & upper stacked clip-y offset of band `s` at time `t` (wiggle center). */
function offsets(t: number): { lo: number[]; hi: number[] } {
  let total = 0;
  for (let s = 0; s < NB; s++) total += SERIES[s][t];
  let acc = -0.5 * total; // wiggle baseline g0(t) = -½·Σ
  const lo: number[] = [];
  const hi: number[] = [];
  for (let s = 0; s < NB; s++) {
    lo[s] = acc * Y_SCALE;
    acc += SERIES[s][t];
    hi[s] = acc * Y_SCALE;
  }
  return { lo, hi };
}

function buildVertices(): number[] {
  const floats: number[] = [];
  // Precompute per-time offsets.
  const lohi = Array.from({ length: T }, (_, t) => offsets(t));
  for (let s = 0; s < NB; s++) {
    const [hr, hg, hb] = BANDS[s].hue;
    const colAt = (t: number): [number, number, number, number] => {
      const flow = 0.75 + 0.25 * (t / (T - 1)); // gentle left→right brightening
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
      // ribbon quad (la,ua)-(lb,ub) → two triangles
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

const VERTS = buildVertices();
const VERTEX_COUNT = VERTS.length / 6;

export const manifest: SceneManifest = {
  label: 'Streamgraph — volume flow',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.04, g: 0.05, b: 0.07, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    { cmd: 'createBuffer', id: BAND_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: BAND_GEOMETRY, vertexBufferId: BAND_BUFFER, format: 'pos2_color4', vertexCount: VERTEX_COUNT },

    { cmd: 'createDrawItem', id: BAND_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: BAND_DRAWITEM, pipeline: 'triGradient@1', geometryId: BAND_GEOMETRY },
  ],
  // Four stacked volume bands on a wiggle baseline, tessellated at build time as
  // per-vertex-colored triangle strips (one static APPEND upload).
  uploads: [{ bufferId: BAND_BUFFER, floats: VERTS } as BufferUpload],
};
