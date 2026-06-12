/* apps/showcase/views/radial-seasonality/records.gen.mjs  (ENC-584)
 *
 * GENERATOR (zero-dep node) for the LIVE radial-seasonality clock — "the clock
 * sweeps".
 *
 * ── What this emits ───────────────────────────────────────────────────────
 * A geometry-frame replay: a `records.json` whose every frame is a single
 * UPDATE_RANGE record (op 2, offset 0) that OVERWRITES the entire wedge vertex
 * buffer with a freshly re-tessellated polar rose. A sweep hand advances around
 * the dial 0→360° across the timeline; as it passes each phase bin, that bin's
 * filled WEDGE is revealed (its radius eases from the baseline ring out to the
 * bin's seasonal value). ENC-569 makes the triGradient vertex backend re-read +
 * redraw an UPDATE_RANGE'd buffer each frame, so the rose fills in live as the
 * hand sweeps.
 *
 * The VERTEX COUNT is constant across all frames: every wedge is always
 * tessellated into the same SEG fan triangles (un-revealed wedges drawn at the
 * baseline radius), plus the constant sweep-hand ribbon — so the pre-sized
 * buffer is stable and every frame is a full-buffer overwrite at offset 0.
 *
 * The tessellation math here is the SINGLE SOURCE OF TRUTH and is mirrored
 * EXACTLY by manifest.ts (frame 0 / sweep 0 seed).
 *
 * Run:  node apps/showcase/views/radial-seasonality/records.gen.mjs
 * Out:  apps/showcase/views/radial-seasonality/records.json
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

// ── record format (mirrors src/scene/commands.ts encodeUpload) ──────────────
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13; // [1B op][4B bufferId LE][4B offset LE][4B payloadBytes LE]
const WEDGE_BUFFER = 601; // pos2_color4 triangle list (24B/vert)

// ── replay timeline ─────────────────────────────────────────────────────────
const VIEW_ID = 'radial-seasonality';
const DURATION_MS = 20000;
const FRAME_COUNT = 72; // ~278ms cadence → smooth sweep
const CADENCE_MS = Math.round(DURATION_MS / FRAME_COUNT);

// ── baked cyclic series + geometry constants (MUST match manifest.ts) ───────
const RADIUS = [
  0.16364, 0.11127, 0.24602, 0.32454, 0.16259, 0.18344, 0.09015, 0.1957,
  0.02433, 0.14558, 0.0721, 0.20738, 0.00267, 0.43087, 0.21031, 0.18703,
  0.36571, 0.18309, 0.16379, 0.31107, -0.13, 0.10608, 0.20008, -0.08934,
];
const BINS = RADIUS.length; // 24
const R_BASELINE = 0.18;
const R_MAX = 0.92;
const SEG = 5;
const HAND_HALF = 0.012;
const RAW_MIN = -0.13;
const RAW_MAX = 0.43087;

function radiusOf(raw) {
  const t = (raw - RAW_MIN) / (RAW_MAX - RAW_MIN);
  return R_BASELINE + t * (R_MAX - R_BASELINE);
}

const angle = (k) => (2 * Math.PI * k) / BINS;
function project(theta, r) {
  // clock face: 0 at top, clockwise → x = r·sinθ, y = r·cosθ.
  return [r * Math.sin(theta), r * Math.cos(theta)];
}

function wedgeColor(r) {
  const t = Math.max(0, Math.min(1, (r - R_BASELINE) / (R_MAX - R_BASELINE)));
  const rr = 0.18 + 0.74 * t;
  const gg = 0.5 + 0.28 * (1 - Math.abs(t - 0.5) * 2) + 0.18 * t;
  const bb = 0.62 - 0.42 * t;
  return [rr, gg, bb, 0.92];
}

function pushTri(out, x0, y0, x1, y1, x2, y2, c) {
  const [r, g, b, a] = c;
  out.push(x0, y0, r, g, b, a);
  out.push(x1, y1, r, g, b, a);
  out.push(x2, y2, r, g, b, a);
}

/** Tessellate the whole rose for a sweep angle (MUST match manifest.ts). */
function tessellate(sweep) {
  const out = [];
  const sectorHalf = Math.PI / BINS;

  for (let k = 0; k < BINS; k++) {
    const a0 = angle(k) - sectorHalf;
    const a1 = angle(k) + sectorHalf;
    const lead = sweep - a0;
    let reveal = lead <= 0 ? 0 : lead >= 2 * sectorHalf ? 1 : lead / (2 * sectorHalf);
    reveal = reveal * reveal * (3 - 2 * reveal);
    const rTip = R_BASELINE + reveal * (radiusOf(RADIUS[k]) - R_BASELINE);
    const c = wedgeColor(rTip);
    for (let s = 0; s < SEG; s++) {
      const t0 = a0 + ((a1 - a0) * s) / SEG;
      const t1 = a0 + ((a1 - a0) * (s + 1)) / SEG;
      const [x1, y1] = project(t0, rTip);
      const [x2, y2] = project(t1, rTip);
      pushTri(out, 0, 0, x1, y1, x2, y2, c);
    }
  }

  const hc = [0.98, 0.95, 0.72, 0.95];
  const ha0 = sweep - HAND_HALF;
  const ha1 = sweep + HAND_HALF;
  const [hx0, hy0] = project(ha0, R_MAX);
  const [hx1, hy1] = project(ha1, R_MAX);
  pushTri(out, 0, 0, hx0, hy0, hx1, hy1, hc);
  pushTri(out, 0, 0, hx1, hy1, hx0, hy0, hc);

  return out;
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
  // Sanity: vertex count must be constant across frames (stable pre-sized buffer).
  const f0 = tessellate(0);
  const fMid = tessellate(Math.PI * 0.83);
  if (f0.length !== fMid.length) {
    throw new Error('vertex count drifted across frames — buffer would not be stable');
  }

  const frames = [];
  for (let k = 0; k < FRAME_COUNT; k++) {
    const p = k / FRAME_COUNT; // 0 .. <1 — full revolution over the timeline
    const sweep = p * 2 * Math.PI; // 0 → 360°
    const floats = tessellate(sweep);
    if (floats.length !== f0.length) {
      throw new Error(`frame ${k}: vertex count drifted`);
    }
    const t = Math.round(p * DURATION_MS);
    frames.push({ t, b64: encodeUpdateRange(WEDGE_BUFFER, floats) });
  }

  const VERTEX_COUNT = f0.length / 6;
  const out = {
    meta: { viewId: VIEW_ID, durationMs: DURATION_MS, frameCount: frames.length, cadenceMs: CADENCE_MS },
    frames,
  };

  writeFileSync(RECORDS_PATH, JSON.stringify(out));
  console.log(
    `radial-seasonality records: ${frames.length} sweep frames, ` +
      `${VERTEX_COUNT} verts/frame (${BINS} wedges × ${SEG} tris + hand), ` +
      `${VERTEX_COUNT * 24}B payload/frame, t∈[0, ${frames[frames.length - 1].t}]ms, ` +
      `cadence=${CADENCE_MS}ms → buffer ${WEDGE_BUFFER}`,
  );
}

main();
