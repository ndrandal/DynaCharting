/* apps/showcase/views/streamgraph/records.gen.mjs  (ENC-583)
 *
 * GENERATOR (zero-dep node) for the LIVE streamgraph (Linear ENC-583).
 *
 * ── What changed (STATIC → LIVE) ──────────────────────────────────────────
 * The streamgraph used to bake ONE static tessellation of four stacked volume
 * bands as a single triGradient@1 upload — the river never moved. This generator
 * makes it FLOW via GEOMETRY-FRAME REPLAY: at N timesteps it re-evaluates each
 * band's thickness as a smooth sum-of-sines (deterministic, no RNG), re-stacks
 * the bands on the wiggle baseline g0(t) = -½·Σ values, re-tessellates the whole
 * silhouette, and emits one UPDATE_RANGE record per timestep that OVERWRITES the
 * entire band vertex buffer with that frame's geometry. The replay engine
 * (useReplay → enqueueData) streams those frames in place; ENC-569's triGradient
 * backend re-reads + redraws the UPDATE_RANGE'd vertex buffer every frame, so the
 * bands undulate and the silhouette breathes live as the thicknesses evolve.
 *
 * ── MECHANISM ─────────────────────────────────────────────────────────────
 * Each band's thickness at time bucket t and frame phase p is a smooth, slowly
 * drifting positive function (sum of a few sines with per-band phase offsets).
 * The whole stack is centered on the wiggle baseline so the silhouette flows
 * symmetrically about y=0 — the signature streamgraph organic shape — and the
 * baseline itself wanders as the totals shift. The VERTEX COUNT IS CONSTANT
 * across frames (always NB bands × (T-1) segments × 6 verts — only the corner
 * positions move), so the pre-sized buffer is stable and every frame is a
 * full-buffer UPDATE_RANGE at offset 0 (matches treemap / sankey).
 *
 * The geometry math here mirrors manifest.ts's stacking/clip mapping; the static
 * manifest stays the at-rest seed (build-time volume tessellation), and this
 * replay drives the live flow on top of the same buffer id / format / stride.
 *
 * USAGE: node apps/showcase/views/streamgraph/records.gen.mjs
 *   (overwrites ./records.json next to this file with the geometry-frame timeline.)
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

// ── record format (mirrors src/scene/commands.ts encodeUpload) ──────────────
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13; // [1B op][4B bufferId LE][4B offset LE][4B payloadBytes LE]
const BAND_BUFFER = 601; // pos2_color4 triangle list — 6 floats/vert, 24B/vert
const FLOATS_PER_VERT = 6; // pos2_color4

// ── replay timeline ─────────────────────────────────────────────────────────
const FRAME_COUNT = 60; // geometry frames over the span
const DURATION_MS = 20000; // total replay span
const CADENCE_MS = Math.round(DURATION_MS / FRAME_COUNT); // ~333ms between frames

// ── stream model (mirrors manifest.ts framing) ──────────────────────────────
// Six flowing bands across T time buckets. Per-vertex hue per band, with a gentle
// left→right brightening so the river reads as flowing.
const NB = 6; // number of bands
const T = 40; // time buckets along x (segments = T-1)
const BANDS = [
  { hue: [0.38, 0.72, 0.55] }, // teal-green
  { hue: [0.36, 0.55, 0.92] }, // blue
  { hue: [0.80, 0.58, 0.32] }, // amber
  { hue: [0.82, 0.40, 0.55] }, // magenta
  { hue: [0.55, 0.45, 0.85] }, // violet
  { hue: [0.40, 0.78, 0.78] }, // cyan
];

// Clip framing — x over the time axis, y centered on the wiggle baseline.
const X0 = -0.9;
const X1 = 0.9;
const xAt = (t) => X0 + (t / (T - 1)) * (X1 - X0);

// ── thickness model ──────────────────────────────────────────────────────────
// Band `s` thickness at bucket t and phase p∈[0,1): a strictly-positive sum of a
// few sines with per-band phase offsets, slow spatial frequency along t, and a
// temporal term that advances a full 2π over the timeline so frame N wraps onto
// frame 0 (seamless loop). Different per-band phases make bands swell/shrink
// against each other (the river re-routes), not pulse in unison.
function thickness(s, t, p) {
  const tt = t / (T - 1); // 0..1 along time
  const ph = (s / NB) * Math.PI * 2; // per-band phase
  const tau = 2 * Math.PI * p; // temporal phase (loops)
  const base = 1.0;
  const wave =
    0.55 * Math.sin(2 * Math.PI * (tt * 1.3 + 0.20 * s) + tau + ph) +
    0.30 * Math.sin(2 * Math.PI * (tt * 0.6 - 0.15 * s) - tau * 0.5 + ph * 1.7) +
    0.18 * Math.sin(2 * Math.PI * (tt * 2.1) + tau * 1.5 + ph * 0.5);
  // Keep strictly positive (floor) so bands never invert / vanish.
  return Math.max(0.18, base + wave);
}

// Vertical scale chosen so the fattest stack across all frames fits the band.
// Probe a dense grid of (t, p) to find the peak total, then map to ~1.7 clip-y.
function peakTotal() {
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

/** Lower & upper stacked clip-y offsets of every band at bucket t, phase p. */
function offsets(t, p) {
  let total = 0;
  for (let s = 0; s < NB; s++) total += thickness(s, t, p);
  let acc = -0.5 * total; // wiggle baseline g0 = -½·Σ
  const lo = [];
  const hi = [];
  for (let s = 0; s < NB; s++) {
    lo[s] = acc * Y_SCALE;
    acc += thickness(s, t, p);
    hi[s] = acc * Y_SCALE;
  }
  return { lo, hi };
}

/** Tessellate the whole silhouette at phase p into a pos2_color4 float array. */
function tessellate(p) {
  const floats = [];
  // Per-bucket stacked offsets for this phase.
  const lohi = [];
  for (let t = 0; t < T; t++) lohi.push(offsets(t, p));
  for (let s = 0; s < NB; s++) {
    const [hr, hg, hb] = BANDS[s].hue;
    const colAt = (t) => {
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
      // ribbon quad (la,ua)-(lb,ub) → two triangles (same winding as manifest)
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

/** Encode one full-buffer UPDATE_RANGE record (offset 0) for `bufferId`. */
function encodeUpdateRange(bufferId, floats) {
  const buf = Buffer.alloc(HEADER_SIZE + floats.length * 4);
  buf.writeUInt8(OP_UPDATE_RANGE, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(0, 5); // offset 0 — overwrite the whole buffer
  buf.writeUInt32LE(floats.length * 4, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

function main() {
  // Sanity: vertex count must be constant across frames so the buffer stays
  // stable (full-buffer UPDATE_RANGE). Verify two phases agree.
  const f0 = tessellate(0);
  const fMid = tessellate(0.37);
  if (f0.length !== fMid.length) {
    throw new Error('vertex count drifted across frames — buffer would not be stable');
  }
  const VERTEX_COUNT = f0.length / FLOATS_PER_VERT;
  const EXPECTED = NB * (T - 1) * 6;
  if (VERTEX_COUNT !== EXPECTED) {
    throw new Error(`vertex count ${VERTEX_COUNT} != expected ${EXPECTED}`);
  }

  const frames = [];
  for (let k = 0; k < FRAME_COUNT; k++) {
    const p = k / FRAME_COUNT; // 0 .. <1 (frame N wraps onto frame 0)
    const floats = tessellate(p);
    const t = Math.round((k / FRAME_COUNT) * DURATION_MS);
    frames.push({ t, b64: encodeUpdateRange(BAND_BUFFER, floats) });
  }

  const out = {
    meta: {
      viewId: 'streamgraph',
      durationMs: DURATION_MS,
      frameCount: frames.length,
      cadenceMs: CADENCE_MS,
    },
    frames,
  };

  writeFileSync(RECORDS_PATH, JSON.stringify(out));
  console.log(
    `streamgraph records: ${frames.length} geometry frames, ` +
      `${NB} bands × ${T - 1} segs = ${VERTEX_COUNT} verts/frame, ` +
      `${VERTEX_COUNT * 24}B payload/frame, ` +
      `t∈[0, ${frames[frames.length - 1].t}]ms, cadence=${CADENCE_MS}ms → buffer ${BAND_BUFFER}`,
  );
}

main();
