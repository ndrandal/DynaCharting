/* apps/showcase/views/sports-shot-chart/records.gen.mjs
 *
 * Records generator for the LIVE sports-shot-chart (Linear ENC-573).
 *
 * Turns the (previously STATIC) made/missed shot clouds into a ~20s game-replay
 * TIMELINE: each shot is APPENDED to a single streaming buffer at a timestamp
 * spread across the game clock, so markers ACCUMULATE one / a few at a time as
 * the "game" progresses (few early, the cloud thickening late).
 *
 * ── Why ONE buffer of colored TRIANGLES (triGradient@1 / pos2_color4) ──────
 * A shot chart needs BOTH colors (green made / red missed) to accumulate live.
 * instancedRect@1 (rect4) carries ONE uniform color per draw item, so two colors
 * would need two buffers — but the replay engine's GrowthSync advances only ONE
 * buffer per view (see candle-overlays/manifest.ts: "GrowthSync advances only ONE
 * buffer per view"). So a second instancedRect stream would render but FREEZE at
 * its seed count (empirically confirmed: red stuck while green grew).
 *
 * The fix: render every shot as a small COLORED QUAD via `triGradient@1`
 * (pos2_color4 = x,y,r,g,b,a per vertex). Each shot = 2 triangles = 6 vertices,
 * with the made/missed color baked into the vertex colors. ALL shots — both
 * outcomes — stream into ONE buffer / one draw item, so the single GrowthSync
 * grows the whole chart and BOTH colors accumulate together.
 *
 * SHOT GEOMETRY (the clustered rim / paint / mid-range / arc / corner-3 zones) is
 * reused VERBATIM from the static manifest's MADE / MISS rect4 arrays — read out
 * of manifest.ts at generation time so the timeline can never drift from the
 * authored shot positions. Each rect4 (x0,y0,x1,y1) is tessellated into its quad.
 *
 * Zero-dep node:  node apps/showcase/views/sports-shot-chart/records.gen.mjs
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));

// --- contract constants (MUST match manifest.ts) ---------------------------
const SHOT_BUFFER = 501; // pos2_color4 triangles (the one streaming buffer)
const OP_APPEND = 1;
const HEADER_SIZE = 13; // [1B op][4B bufferId LE][4B offset LE][4B payloadBytes LE]
const FLOATS_PER_VERTEX = 6; // x, y, r, g, b, a
const VERTS_PER_SHOT = 6; // two triangles
const SHOT_FLOATS = FLOATS_PER_VERTEX * VERTS_PER_SHOT; // 36
const SHOT_BYTES = SHOT_FLOATS * 4; // 144

const DURATION_MS = 20000; // ~20s game replay
const CADENCE_MS = 80; // frame batching interval (shots land on this grid)

// Made (green) / missed (red) marker colors — mirror the view.json legend.
const MADE_RGBA = [0.2, 0.85, 0.4, 0.95];
const MISS_RGBA = [0.9, 0.3, 0.3, 0.95];

// --- pull the exact static shot rects out of manifest.ts -------------------
function grabFloatArray(src, name) {
  const re = new RegExp('const ' + name + ': number\\[\\] = \\[([\\s\\S]*?)\\];');
  const m = src.match(re);
  if (!m) throw new Error('could not find array ' + name + ' in manifest.ts');
  return m[1]
    .split(',')
    .map((s) => s.trim())
    .filter((s) => s.length > 0)
    .map(Number);
}

const manifestSrc = readFileSync(join(HERE, 'manifest.ts'), 'utf8');
const madeFloats = grabFloatArray(manifestSrc, 'MADE');
const missFloats = grabFloatArray(manifestSrc, 'MISS');

/** Slice a flat float list into rect4 tuples [x0,y0,x1,y1]. */
function toRects(floats) {
  const rects = [];
  for (let i = 0; i + 4 <= floats.length; i += 4) {
    rects.push([floats[i], floats[i + 1], floats[i + 2], floats[i + 3]]);
  }
  return rects;
}

const madeRects = toRects(madeFloats); // 152
const missRects = toRects(missFloats); // 168

// --- build the chronological shot timeline ---------------------------------
// Interleave made + missed by pulling from whichever stream has consumed the
// SMALLER fraction of its shots, so the two outcomes are woven together evenly
// across the whole game instead of clumping.
const shots = []; // { rgba, rect }
let mi = 0;
let xi = 0;
const total = madeRects.length + missRects.length; // 320
for (let n = 0; n < total; n++) {
  const madeFrac = madeRects.length ? mi / madeRects.length : 1;
  const missFrac = missRects.length ? xi / missRects.length : 1;
  if (mi < madeRects.length && (madeFrac <= missFrac || xi >= missRects.length)) {
    shots.push({ rgba: MADE_RGBA, rect: madeRects[mi++] });
  } else {
    shots.push({ rgba: MISS_RGBA, rect: missRects[xi++] });
  }
}

// Timestamp each shot: ease-in across DURATION_MS so density grows over time
// (p^1.6 leaves the early game sparse and packs more shots toward the end).
function shotTime(index) {
  const p = total > 1 ? index / (total - 1) : 0;
  const eased = Math.pow(p, 1.6);
  return Math.round(eased * (DURATION_MS - CADENCE_MS));
}

// --- tessellate one shot rect4 into a 6-vertex colored quad ----------------
function shotQuadFloats(rect, rgba) {
  const [x0, y0, x1, y1] = rect;
  const [r, g, b, a] = rgba;
  const v = (x, y) => [x, y, r, g, b, a];
  // Two triangles: (x0,y0)-(x1,y0)-(x1,y1) and (x0,y0)-(x1,y1)-(x0,y1).
  return [
    ...v(x0, y0), ...v(x1, y0), ...v(x1, y1),
    ...v(x0, y0), ...v(x1, y1), ...v(x0, y1),
  ];
}

// --- batch shots into cadence-aligned frames -------------------------------
// All shots whose time falls in [t, t+CADENCE) are packed into ONE frame at the
// grid time t. A frame's binary is a single APPEND record whose payload is the
// concatenation of every shot's quad floats (most frames carry 0-2 shots).
const byFrame = new Map(); // gridT -> array of shots (in order)
for (let i = 0; i < shots.length; i++) {
  const t = shotTime(i);
  const gridT = Math.floor(t / CADENCE_MS) * CADENCE_MS;
  if (!byFrame.has(gridT)) byFrame.set(gridT, []);
  byFrame.get(gridT).push(shots[i]);
}

/** Pack one APPEND record (header + N shot quads) into a byte array. */
function encodeFrame(frameShots) {
  const floats = [];
  for (const s of frameShots) floats.push(...shotQuadFloats(s.rect, s.rgba));
  const payloadBytes = floats.length * 4;
  const buf = new Uint8Array(HEADER_SIZE + payloadBytes);
  const dv = new DataView(buf.buffer);
  dv.setUint8(0, OP_APPEND);
  dv.setUint32(1, SHOT_BUFFER, true);
  dv.setUint32(5, 0, true); // offset ignored for APPEND
  dv.setUint32(9, payloadBytes, true);
  let o = HEADER_SIZE;
  for (const f of floats) {
    dv.setFloat32(o, f, true);
    o += 4;
  }
  return buf;
}

function bytesToB64(bytes) {
  return Buffer.from(bytes).toString('base64');
}

const frames = [];
for (const gridT of [...byFrame.keys()].sort((a, b) => a - b)) {
  frames.push({ t: gridT, b64: bytesToB64(encodeFrame(byFrame.get(gridT))) });
}

const out = {
  meta: {
    viewId: 'sports-shot-chart',
    durationMs: DURATION_MS,
    frameCount: frames.length,
    cadenceMs: CADENCE_MS,
  },
  frames,
};

writeFileSync(join(HERE, 'records.json'), JSON.stringify(out) + '\n');

const madeCount = shots.filter((s) => s.rgba === MADE_RGBA).length;
const missCount = shots.filter((s) => s.rgba === MISS_RGBA).length;
console.log(
  `wrote records.json: ${frames.length} frames, ${shots.length} shots ` +
    `(${madeCount} made / ${missCount} miss), ${VERTS_PER_SHOT} verts/shot ` +
    `(${SHOT_BYTES}B), over ${DURATION_MS}ms @ ${CADENCE_MS}ms cadence`,
);
