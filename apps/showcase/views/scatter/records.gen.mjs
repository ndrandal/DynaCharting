/* apps/showcase/views/scatter/records.gen.mjs  (ENC-574)
 *
 * Generator for the scatter view's LIVE replay records (zero-dep node).
 *
 * The scatter cloud used to be STATIC: the chrome batch tessellated every
 * captured (lastPrice, volume) point into one static instancedRect@1 upload, so
 * the whole cloud appeared at once. This generator instead emits each point as a
 * small rect4 marker (16B) APPENDED to the marker buffer over the ~20s replay
 * timeline, so the cloud fills IN live — each frame adds one point, the
 * instancedRect geometry grows (ENC-558 growth rebind), and late points are
 * never clipped because the baked transform frames the full price/volume range.
 *
 * SOURCE: the prior records.json captured the real embassy dataplane — 267
 * pos2_clip (8B: x=lastPrice, y=volume) APPEND frames to buffer 700 over ~20s.
 * We decode those (lastPrice, volume) points and their timestamps, then re-emit
 * the SAME points (same timeline) as rect4 markers into the marker buffer. The
 * marker geometry (half-size in clip → data space) mirrors the old static
 * tessellation so the cloud looks identical — just streamed.
 *
 * USAGE: node apps/showcase/views/scatter/records.gen.mjs
 *   (run from the repo root; reads ./records.json relative to this file and
 *    overwrites it with the streamed rect4 marker timeline.)
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

// --- record format (mirrors src/scene/commands.ts encodeUpload) -------------
const OP_APPEND = 1;
const HEADER_SIZE = 13; // [1B op][4B bufferId LE][4B offset LE][4B payloadBytes LE]
const SRC_POINT_BUFFER = 700; // pos2_clip 8B (x=lastPrice, y=volume) — capture source
const MARKER_BUFFER = 701; // rect4 16B marker squares — the streamed cloud

// --- marker sizing (mirrors view.json transform; same as the old manifest) ---
// view.json transform (data→clip). Used to invert a CLIP-space marker half-size
// into DATA space per axis so every square renders the same on-screen size
// regardless of the price/volume scales.
const SX = 0.1705381;
const SY = 0.000009792795;
const HALF_CLIP = 0.007; // ~7px on a 1024px canvas
const HALF_X = HALF_CLIP / SX; // ≈ 0.041 price units
const HALF_Y = HALF_CLIP / SY; // ≈ 715 volume units

function b64ToBytes(b64) {
  const buf = Buffer.from(b64, 'base64');
  return new Uint8Array(buf);
}

/**
 * Decode every captured (lastPrice, volume) point + its frame timestamp.
 *
 * Idempotent: the SOURCE capture appended 8B pos2_clip points to buffer 700.
 * This generator overwrites records.json with 16B rect4 markers on buffer 701,
 * so a re-run must also recover the original (x,y) centers from already-emitted
 * markers — we read whichever buffer the frames carry and recover the center
 * from a rect4's corners ((x0+x1)/2, (y0+y1)/2). Either way the output is the
 * same streamed marker timeline.
 */
function decodeSource() {
  const raw = JSON.parse(readFileSync(RECORDS_PATH, 'utf8'));
  const points = []; // { t, x, y }
  for (const frame of raw.frames) {
    const bytes = b64ToBytes(frame.b64);
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let o = 0;
    while (o + HEADER_SIZE <= dv.byteLength) {
      const op = dv.getUint8(o);
      const bufId = dv.getUint32(o + 1, true);
      const payloadBytes = dv.getUint32(o + 9, true);
      o += HEADER_SIZE;
      if (o + payloadBytes > dv.byteLength) break;
      if (op === OP_APPEND && bufId === SRC_POINT_BUFFER) {
        // pos2_clip 8B: x=lastPrice, y=volume.
        for (let p = 0; p + 8 <= payloadBytes; p += 8) {
          points.push({
            t: frame.t,
            x: dv.getFloat32(o + p, true),
            y: dv.getFloat32(o + p + 4, true),
          });
        }
      } else if (op === OP_APPEND && bufId === MARKER_BUFFER) {
        // rect4 16B marker (already-generated): recover the center.
        for (let p = 0; p + 16 <= payloadBytes; p += 16) {
          const x0 = dv.getFloat32(o + p, true);
          const y0 = dv.getFloat32(o + p + 4, true);
          const x1 = dv.getFloat32(o + p + 8, true);
          const y1 = dv.getFloat32(o + p + 12, true);
          points.push({ t: frame.t, x: (x0 + x1) / 2, y: (y0 + y1) / 2 });
        }
      }
      o += payloadBytes;
    }
  }
  return points;
}

/** Encode one rect4 marker APPEND record (16B payload) for buffer 701. */
function encodeMarker(x, y) {
  const floats = [x - HALF_X, y - HALF_Y, x + HALF_X, y + HALF_Y];
  const buf = Buffer.alloc(HEADER_SIZE + floats.length * 4);
  buf.writeUInt8(OP_APPEND, 0);
  buf.writeUInt32LE(MARKER_BUFFER, 1);
  buf.writeUInt32LE(0, 5); // APPEND ignores offset
  buf.writeUInt32LE(floats.length * 4, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

function main() {
  const points = decodeSource();
  if (!points.length) throw new Error('no source points decoded from records.json');

  // One streamed frame per point — preserves the captured cadence/timeline so
  // the cloud fills in at the original tick rate.
  const frames = points.map((p) => ({ t: p.t, b64: encodeMarker(p.x, p.y) }));

  const lastT = frames[frames.length - 1].t;
  const durationMs = 20000;
  const cadenceMs = Math.round(lastT / Math.max(1, frames.length - 1));

  const out = {
    meta: {
      viewId: 'scatter',
      durationMs,
      frameCount: frames.length,
      cadenceMs,
    },
    frames,
  };

  writeFileSync(RECORDS_PATH, JSON.stringify(out));
  console.log(
    `scatter records: ${frames.length} rect4 markers, t∈[${frames[0].t}, ${lastT}]ms, ` +
      `cadence≈${cadenceMs}ms → buffer ${MARKER_BUFFER}`,
  );
}

main();
