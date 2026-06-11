/* apps/showcase/views/ridgeline/manifest.ts
 *
 * COMPOSED — "D3/Observable ridgeline" overlapping density bands, a STATIC
 * COMPUTED-GEOMETRY view (Linear ENC-536 / T4.4). The density curves are
 * TESSELLATED AT MANIFEST-BUILD TIME from the market datasets and embedded as
 * static manifest `uploads` — no streaming, capture, or replay.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A ridgeline plot stacks several smoothed distributions, each offset upward
 * and allowed to overlap the band above it. We build one density per symbol
 * (AAPL/MSFT/NVDA/TSLA): histogram each symbol's lastPrice ticks over its own
 * price range into 48 bins, Gaussian-smooth into a KDE-like curve, normalise to
 * unit peak, and assign each band a vertical baseline. The bands are drawn back
 * (top) to front (bottom) so the nearer ridge occludes the one behind — the
 * signature ridgeline overlap.
 *
 * ── Geometry (triGradient@1 / pos2_color4) ────────────────────────────────
 * Each band is a filled area = a triangle STRIP between its baseline and its
 * density curve, emitted as pos2_color4 triangles. Per-vertex color blends from
 * a translucent dark baseline up to the band's bright crest, so each ridge has
 * a vertical gradient fill; the four bands carry four distinct hues. Because
 * color rides the vertex buffer, ONE triGradient draw item renders every band.
 * Vertices are authored directly in clip space — no transform attached.
 *
 * No `growth`, no streaming: the curves are fully resolved at build time.
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
  { name: 'NVDA', data: nvda as { updates: Update[] }, hue: [0.78, 0.55, 0.30] },
  { name: 'TSLA', data: tsla as { updates: Update[] }, hue: [0.82, 0.40, 0.55] },
];

const BINS = 48;

function series(updates: Update[], field: string): number[] {
  return updates.filter((u) => u.field === field).map((u) => u.value);
}

/** Histogram a symbol's lastPrice over its own range, Gaussian-smooth, peak-normalise. */
function density(updates: Update[]): number[] {
  const lp = series(updates, 'lastPrice');
  const min = Math.min(...lp);
  const max = Math.max(...lp);
  const span = max - min || 1;
  const h = new Array(BINS).fill(0);
  for (const v of lp) {
    const b = Math.round(((v - min) / span) * (BINS - 1));
    h[b] += 1;
  }
  const k = [0.06, 0.24, 0.4, 0.24, 0.06];
  const sm = h.map((_, i) => {
    let a = 0;
    for (let j = -2; j <= 2; j++) {
      const x = i + j;
      if (x >= 0 && x < BINS) a += h[x] * k[j + 2];
    }
    return a;
  });
  const peak = Math.max(...sm) || 1;
  return sm.map((v) => v / peak);
}

// --- tessellate bands → pos2_color4 triangle strips ------------------------
// Clip layout: x spans [-0.88,0.88]; bands stacked over [-0.85, 0.9], each band
// given a baseline + an amplitude that lets it overlap the band above.
const X0 = -0.88;
const X1 = 0.88;
const Y_BOTTOM = -0.82;
const Y_TOP = 0.86;
const AMP = 0.46; // crest height in clip units (≈ 2× the per-band spacing → overlap)

function buildVertices(): number[] {
  const floats: number[] = [];
  const n = BANDS.length;
  const spacing = (Y_TOP - Y_BOTTOM) / (n - 1);
  // Draw back (highest baseline) to front (lowest) so nearer ridges occlude.
  for (let bi = n - 1; bi >= 0; bi--) {
    const band = BANDS[bi];
    const d = density(band.data.updates);
    const baseY = Y_BOTTOM + bi * spacing;
    const [hr, hg, hb] = band.hue;
    // baseline color: dark + translucent; crest color: bright hue.
    const baseC: [number, number, number, number] = [hr * 0.28, hg * 0.28, hb * 0.28, 0.85];
    const x = (i: number) => X0 + (i / (BINS - 1)) * (X1 - X0);
    const crestC = (v: number): [number, number, number, number] => [
      hr * (0.55 + 0.45 * v),
      hg * (0.55 + 0.45 * v),
      hb * (0.55 + 0.45 * v),
      0.92,
    ];
    // Triangle strip between baseline and curve, expanded to explicit triangles.
    for (let i = 0; i < BINS - 1; i++) {
      const xa = x(i);
      const xb = x(i + 1);
      const ya = baseY + d[i] * AMP;
      const yb = baseY + d[i + 1] * AMP;
      const [ar, ag, ab, aa] = crestC(d[i]);
      const [br, bg, bb, ba] = crestC(d[i + 1]);
      const [er, eg, eb, ea] = baseC;
      // quad: (xa,base)(xa,ya)(xb,yb)(xb,base) → two triangles
      // tri 1: base_a, crest_a, crest_b
      floats.push(xa, baseY, er, eg, eb, ea);
      floats.push(xa, ya, ar, ag, ab, aa);
      floats.push(xb, yb, br, bg, bb, ba);
      // tri 2: base_a, crest_b, base_b
      floats.push(xa, baseY, er, eg, eb, ea);
      floats.push(xb, yb, br, bg, bb, ba);
      floats.push(xb, baseY, er, eg, eb, ea);
    }
  }
  return floats;
}

const VERTS = buildVertices();
const VERTEX_COUNT = VERTS.length / 6;

export const manifest: SceneManifest = {
  label: 'Ridgeline — price density bands',
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
  // Four overlapping density bands, tessellated at build time as per-vertex
  // gradient-filled triangle strips (one static APPEND upload).
  uploads: [{ bufferId: BAND_BUFFER, floats: VERTS } as BufferUpload],
};
