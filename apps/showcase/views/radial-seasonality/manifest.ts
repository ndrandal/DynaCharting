/* apps/showcase/views/radial-seasonality/manifest.ts
 *
 * COMPOSED / LIVE view — "polar / radial seasonality clock" (ENC-537 / T4.5,
 * made LIVE in ENC-584 — "the clock sweeps").
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * The engine has NO native polar transform — it only does affine 2D. A polar
 * chart is therefore achieved by PROJECTING (angle, radius) → cartesian at
 * BUILD TIME: for each cyclic bin k, angle θ = 2π·k/BINS and radius r is the
 * (scaled) seasonal value in that bin, giving x = r·cosθ, y = r·sinθ. The
 * projected geometry is then drawn with the ordinary colored-triangle pipeline
 * `triGradient@1` (vertex format pos2_color4: x,y,r,g,b,a per vertex). This
 * proves "polar via build-time projection" with FILLED wedges, not a renderer
 * feature.
 *
 * ── LIVE / animating (ENC-584) — "THE CLOCK SWEEPS" ────────────────────────
 * Radial seasonality = a value-by-cyclic-phase series laid out around a circle
 * (here BINS phase bins of a synthetic session around 360°). The view is made
 * LIVE as a GEOMETRY-FRAME REPLAY: a sweep hand advances around the dial 0→360°
 * over the 20 s timeline, and as it passes each bin that bin's WEDGE is
 * REVEALED — its radius eases from the baseline ring out to the bin's seasonal
 * value. records.gen.mjs re-tessellates the rose at N timesteps and records.json
 * carries one UPDATE_RANGE (op 2, offset 0) per frame that OVERWRITES the whole
 * wedge vertex buffer in place. The replay engine (useReplay → enqueueData)
 * streams those frames on the recorded timeline; ENC-569's triGradient backend
 * re-reads + redraws the UPDATE_RANGE'd buffer each frame, so the wedges fill in
 * as the hand sweeps round.
 *
 * The VERTEX COUNT IS CONSTANT across frames: every wedge is always tessellated
 * into the same SEG fan triangles; an un-revealed wedge is drawn at radius 0
 * (degenerate / zero-area), so the buffer is PRE-SIZED once and every frame is a
 * stable full-buffer overwrite at offset 0. No `growth` export.
 *
 * The geometry math here (RADIUS profile + wedge tessellation + sweep reveal)
 * is mirrored EXACTLY by records.gen.mjs; this manifest seeds frame 0 (sweep at
 * angle 0 — nothing revealed yet but the baseline ring) so the dial is correct
 * before/while paused and after each loop reset.
 *
 * ── Geometry (triGradient@1 / pos2_color4) ────────────────────────────────
 *  • the rose wedges — one filled wedge per phase bin, fanned from the dial
 *    center out to (the revealed fraction of) the bin's seasonal radius. Color
 *    is per-vertex: a hot→cool ramp by radius so longer (higher-return) wedges
 *    read brighter (a polar-area / rose chart).
 *  • the sweep hand — a thin bright ribbon (a tall narrow wedge) at the current
 *    sweep angle, the clock hand leading the reveal.
 * All in one pos2_color4 buffer / one triGradient draw item; coordinates are
 * authored directly in clip space, no transform.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const WEDGE_BUFFER = 601; // pos2_color4 triangle list (wedge fans + sweep hand)
const WEDGE_GEOMETRY = 201;
const WEDGE_DRAWITEM = 301;

// ── baked cyclic series (radius per bin, clip units) — mirrors records.gen.mjs
// From .build-composed-static.mjs: avg per-tick return per phase bin, scaled
// into a visible clip-space band about a baseline ring.
const RADIUS: number[] = [
  0.16364, 0.11127, 0.24602, 0.32454, 0.16259, 0.18344, 0.09015, 0.1957,
  0.02433, 0.14558, 0.0721, 0.20738, 0.00267, 0.43087, 0.21031, 0.18703,
  0.36571, 0.18309, 0.16379, 0.31107, -0.13, 0.10608, 0.20008, -0.08934,
];
const BINS = RADIUS.length; // 24
const R_BASELINE = 0.18; // baseline ring radius (zero-return reference)
const R_MAX = 0.92; // outer clip radius the strongest wedge maps to
const SEG = 5; // fan sub-segments per wedge (smooth arc edge)
const HAND_HALF = 0.012; // half-angular-width (rad) of the sweep hand ribbon

// Map a raw seasonal value to a clip radius about the baseline ring. The raw
// values span roughly [-0.13, 0.43]; affinely lift them so the baseline ring
// sits at R_BASELINE and the strongest bin reaches ~R_MAX. (Mirrors gen.)
const RAW_MIN = -0.13;
const RAW_MAX = 0.43087;
function radiusOf(raw: number): number {
  const t = (raw - RAW_MIN) / (RAW_MAX - RAW_MIN); // 0..1
  return R_BASELINE + t * (R_MAX - R_BASELINE);
}

// bin 0 at 12 o'clock, sweeping CLOCKWISE (a real clock). θ measured from +y.
const angle = (k: number) => (2 * Math.PI * k) / BINS;
function project(theta: number, r: number): [number, number] {
  // clock face: 0 at top, clockwise → x = r·sinθ, y = r·cosθ.
  return [r * Math.sin(theta), r * Math.cos(theta)];
}

// Per-vertex color: radius ramp (cool baseline → hot tip) so taller wedges read
// brighter. tNorm ∈ [0,1] over [R_BASELINE, R_MAX].
function wedgeColor(r: number): [number, number, number, number] {
  const t = Math.max(0, Math.min(1, (r - R_BASELINE) / (R_MAX - R_BASELINE)));
  // teal → amber ramp
  const rr = 0.18 + 0.74 * t;
  const gg = 0.5 + 0.28 * (1 - Math.abs(t - 0.5) * 2) + 0.18 * t;
  const bb = 0.62 - 0.42 * t;
  return [rr, gg, bb, 0.92];
}

// Push one triangle (3 verts) in pos2_color4.
function pushTri(
  out: number[],
  x0: number, y0: number, x1: number, y1: number, x2: number, y2: number,
  c: [number, number, number, number],
): void {
  const [r, g, b, a] = c;
  out.push(x0, y0, r, g, b, a);
  out.push(x1, y1, r, g, b, a);
  out.push(x2, y2, r, g, b, a);
}

/**
 * Tessellate the whole rose for a given sweep angle `sweep` ∈ [0, 2π]. A bin is
 * revealed by how far the sweep hand has passed its center: reveal ∈ [0,1] eases
 * the wedge radius from the baseline ring (0) to the bin's seasonal radius (1).
 * The sweep hand is appended as a tall narrow bright ribbon at `sweep`.
 *
 * VERTEX COUNT IS CONSTANT: BINS wedges × SEG fan-tris × 3 + sweep-hand 2 tris.
 */
export function tessellate(sweep: number): number[] {
  const out: number[] = [];
  const sectorHalf = Math.PI / BINS; // half angular width of a wedge

  for (let k = 0; k < BINS; k++) {
    const a0 = angle(k) - sectorHalf;
    const a1 = angle(k) + sectorHalf;
    // reveal: 1 once the hand has fully passed this bin's trailing edge, easing
    // in over the sector as the hand crosses it; 0 before the hand arrives.
    const lead = sweep - a0;
    let reveal = lead <= 0 ? 0 : lead >= 2 * sectorHalf ? 1 : lead / (2 * sectorHalf);
    // smoothstep for a softer fill
    reveal = reveal * reveal * (3 - 2 * reveal);
    const rTip = R_BASELINE + reveal * (radiusOf(RADIUS[k]) - R_BASELINE);
    const c = wedgeColor(rTip);
    // fan SEG triangles from center across [a0, a1]
    for (let s = 0; s < SEG; s++) {
      const t0 = a0 + ((a1 - a0) * s) / SEG;
      const t1 = a0 + ((a1 - a0) * (s + 1)) / SEG;
      const [x1, y1] = project(t0, rTip);
      const [x2, y2] = project(t1, rTip);
      pushTri(out, 0, 0, x1, y1, x2, y2, c);
    }
  }

  // sweep hand: a tall narrow bright ribbon (2 tris) from center to R_MAX.
  const hc: [number, number, number, number] = [0.98, 0.95, 0.72, 0.95];
  const ha0 = sweep - HAND_HALF;
  const ha1 = sweep + HAND_HALF;
  const [hx0, hy0] = project(ha0, R_MAX);
  const [hx1, hy1] = project(ha1, R_MAX);
  pushTri(out, 0, 0, hx0, hy0, hx1, hy1, hc);
  // second (degenerate-safe) triangle to keep an even, stable quad footprint.
  pushTri(out, 0, 0, hx1, hy1, hx0, hy0, hc);

  return out;
}

// Seed = sweep angle 0 (matches records.gen.mjs frame 0): nothing revealed yet,
// only the baseline-radius stubs + the hand parked at 12 o'clock.
const seed = tessellate(0);
const WEDGE_VERTS = seed.length / 6; // 6 floats per pos2_color4 vertex
const WEDGE_BYTES = seed.length * 4;

// Frame-0 seed uploaded as UPDATE_RANGE into the pre-sized buffer (NOT append),
// so the at-rest dial is correct and the buffer size never changes.
const wedgeSeed: BufferUpload = { bufferId: WEDGE_BUFFER, op: 'updateRange', offsetBytes: 0, floats: seed };

export const manifest: SceneManifest = {
  label: 'Radial Seasonality Clock — the clock sweeps (live polar rose)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.98, clipXMax: 0.98, clipYMin: -0.98, clipYMax: 0.98 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Pre-sized wedge buffer (constant WEDGE_VERTS × 24B); the replay's
    // UPDATE_RANGE frames overwrite it in place as the hand sweeps.
    { cmd: 'createBuffer', id: WEDGE_BUFFER, byteLength: WEDGE_BYTES },
    { cmd: 'createGeometry', id: WEDGE_GEOMETRY, vertexBufferId: WEDGE_BUFFER, format: 'pos2_color4', vertexCount: WEDGE_VERTS },
    { cmd: 'createDrawItem', id: WEDGE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: WEDGE_DRAWITEM, pipeline: 'triGradient@1', geometryId: WEDGE_GEOMETRY },
  ],
  // Frame-0 seed (sweep 0). The live geometry then arrives via useReplay's
  // UPDATE_RANGE frames (records.json) — the rose fills as the hand sweeps.
  uploads: [wedgeSeed],
};
