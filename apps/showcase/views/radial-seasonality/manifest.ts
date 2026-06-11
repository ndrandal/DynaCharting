/* apps/showcase/views/radial-seasonality/manifest.ts
 *
 * COMPOSED / STATIC view — "polar / radial seasonality clock" (ENC-537 / T4.5).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * The engine has NO native polar transform — it only does affine 2D. A polar
 * chart is therefore achieved by PROJECTING (angle, radius) → cartesian at
 * BUILD TIME: for each cyclic bin k, angle θ = 2π·k/BINS and radius r is the
 * (scaled) average return in that bin, giving x = r·cosθ, y = r·sinθ. The
 * projected points are then drawn with the ordinary cartesian line pipeline
 * `line2d@1` (vertex format pos2_clip). This proves "polar via build-time
 * projection", not a renderer feature.
 *
 * ── The series ────────────────────────────────────────────────────────────
 * The 20 s synthetic tapes have no real calendar seasonality, so a cyclic
 * "session clock" is synthesized (precomputed by .build-composed-static.mjs):
 * every lastPrice tick across the four symbols is folded onto one of BINS phase
 * bins of a synthetic trading day, and the per-tick % return is averaged within
 * each bin. The radius array below is that avg-return, affinely scaled into a
 * visible clip-space band about a baseline ring. It is a demonstration of the
 * polar projection, not a seasonality claim.
 *
 * ── Geometry ──────────────────────────────────────────────────────────────
 *  • baseline ring  — the r = rInner circle (zero-return reference), line2d.
 *  • radial spokes  — one short line from center to each bin's point, line2d.
 *  • the series loop — the closed polyline through the BINS projected points,
 *    line2d. DrawMode::Lines draws independent SEGMENTS (vertex pairs), so each
 *    edge is emitted as two vertices; a closed loop of N points = N segments.
 * All three live in one pos2_clip buffer / one draw item (uniform line color);
 * coordinates are authored directly in clip space, no transform.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const LINE_BUFFER = 601; // pos2_clip line segments
const LINE_GEOMETRY = 201;
const LINE_DRAWITEM = 301;

// ── baked cyclic series (radius per bin, clip units) ────────────────────────
// From .build-composed-static.mjs: avg per-tick return per phase bin, scaled.
const RADIUS: number[] = [
  0.16364, 0.11127, 0.24602, 0.32454, 0.16259, 0.18344, 0.09015, 0.1957,
  0.02433, 0.14558, 0.0721, 0.20738, 0.00267, 0.43087, 0.21031, 0.18703,
  0.36571, 0.18309, 0.16379, 0.31107, -0.13, 0.10608, 0.20008, -0.08934,
];
const BINS = RADIUS.length; // 24
const R_BASELINE = 0.18; // baseline ring radius (== rInner in the build script)

// pos2_clip is just (x, y) — DrawMode::Lines consumes vertices in PAIRS.
function pushSeg(out: number[], x0: number, y0: number, x1: number, y1: number): void {
  out.push(x0, y0, x1, y1);
}

const angle = (k: number) => (2 * Math.PI * k) / BINS; // θ for bin k
const project = (k: number, r: number): [number, number] => {
  const t = angle(k);
  // y up: clip-y positive is up. cos→x, sin→y for a conventional clock face
  // (bin 0 at the right / 3-o'clock, advancing counter-clockwise).
  return [r * Math.cos(t), r * Math.sin(t)];
};

const floats: number[] = [];

// 1) baseline ring (zero-return reference): a smooth circle approximated by
//    RING_SEGS segments at radius R_BASELINE.
const RING_SEGS = 96;
for (let s = 0; s < RING_SEGS; s++) {
  const a0 = (2 * Math.PI * s) / RING_SEGS;
  const a1 = (2 * Math.PI * (s + 1)) / RING_SEGS;
  pushSeg(
    floats,
    R_BASELINE * Math.cos(a0), R_BASELINE * Math.sin(a0),
    R_BASELINE * Math.cos(a1), R_BASELINE * Math.sin(a1),
  );
}

// 2) radial spokes: center → each bin's projected point.
for (let k = 0; k < BINS; k++) {
  const [x, y] = project(k, RADIUS[k]);
  pushSeg(floats, 0, 0, x, y);
}

// 3) the closed series loop through the projected points.
for (let k = 0; k < BINS; k++) {
  const [x0, y0] = project(k, RADIUS[k]);
  const kn = (k + 1) % BINS;
  const [x1, y1] = project(kn, RADIUS[kn]);
  pushSeg(floats, x0, y0, x1, y1);
}

const LINE_VERTS = floats.length / 2; // 2 floats per pos2_clip vertex
const lineUpload: BufferUpload = { bufferId: LINE_BUFFER, op: 'append', floats };

export const manifest: SceneManifest = {
  label: 'Radial Seasonality Clock — polar via build-time projection',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.98, clipXMax: 0.98, clipYMin: -0.98, clipYMax: 0.98 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    { cmd: 'createBuffer', id: LINE_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: LINE_GEOMETRY, vertexBufferId: LINE_BUFFER, format: 'pos2_clip', vertexCount: LINE_VERTS },
    { cmd: 'createDrawItem', id: LINE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: LINE_DRAWITEM, pipeline: 'line2d@1', geometryId: LINE_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: LINE_DRAWITEM, r: 0.45, g: 0.78, b: 0.95, a: 1 },
  ],
  uploads: [lineUpload],
};
