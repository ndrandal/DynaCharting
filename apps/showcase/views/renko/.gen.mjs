/* apps/showcase/views/renko/.gen.mjs
 *
 * Generator for the LIVE Renko view's records.json (Linear ENC-572). Zero-dep
 * node script. Reproduces the SAME fixed-brick walk the static manifest used to
 * bake at build time, but instead of emitting one big static `uploads` blob it
 * EMITS A TIMELINE: each brick becomes one rect4 (x0,y0,x1,y1 = 16B) APPENDED
 * to the brick buffer at the next offset, stamped with the timestamp `t` of the
 * AAPL close that produced it. The replay engine (useReplay) streams these
 * APPEND frames into the data plane; ENC-558's instancedRect backend re-reads
 * the growing buffer so bricks materialise live as the simulated price walks
 * across brick thresholds (~20s).
 *
 * Output: apps/showcase/views/renko/records.json
 *   { meta:{viewId,durationMs,frameCount,cadenceMs}, frames:[{t,b64}] }
 *
 * Run:  node apps/showcase/views/renko/.gen.mjs   (from repo root)
 *
 * The brick GEOMETRY (rect corners, transform framing) must stay in lock-step
 * with manifest.ts — both derive from the same brickSize = priceRange / 14 walk
 * and the same gap inset, so the streamed rects land exactly where the baked
 * transform frames them.
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const AAPL = JSON.parse(
  readFileSync(resolve(__dirname, '../../data/market/AAPL.json'), 'utf8'),
);

// --- binary record format (CONTRACT: commands.ts / IngestProcessor) ----------
// [1B op][4B bufferId LE][4B offsetBytes LE][4B payloadBytes LE][payload...]
const OP_APPEND = 1;
const HEADER_SIZE = 13;
const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1 (4×f32)

// MUST match manifest.ts.
const BRICK_BUFFER = 601;
const BRICK_COUNT = 14; // priceRange / BRICK_COUNT = brickSize
const GAP = 0.08; // column/brick gutter inset (matches manifest)

const DURATION_MS = 20000;

/** AAPL close series as {t, value} in capture order. */
function closeSeries() {
  return AAPL.updates
    .filter((u) => u.field === 'close')
    .map((u) => ({ t: u.t, value: u.value }));
}

/**
 * Walk the closes, emitting one fixed-size brick per brick-boundary crossing.
 * Identical logic to manifest.ts.buildBricks, but each brick also carries the
 * `t` of the close that triggered it so we can spread them across the timeline.
 */
function buildBricks() {
  const series = closeSeries();
  const values = series.map((s) => s.value);
  const lo = Math.min(...values);
  const hi = Math.max(...values);
  const brickSize = (hi - lo) / BRICK_COUNT;

  const bricks = [];
  let level = values[0];
  let col = 0;
  for (const { t, value: price } of series) {
    while (price - level >= brickSize) {
      bricks.push({ col, lo: level, hi: level + brickSize, up: true, t });
      level += brickSize;
      col++;
    }
    while (level - price >= brickSize) {
      bricks.push({ col, lo: level - brickSize, hi: level, up: false, t });
      level -= brickSize;
      col++;
    }
  }
  return { bricks, brickSize, cols: col, lo, hi };
}

/** rect4 floats (x0,y0,x1,y1) for one brick, with the gutter inset. */
function rectFor(b, brickSize) {
  const x0 = b.col + GAP;
  const x1 = b.col + 1 - GAP;
  return [x0, b.lo + brickSize * GAP, x1, b.hi - brickSize * GAP];
}

/** Pack one APPEND record (13B header + 16B rect4 payload) → base64. */
function encodeAppend(bufferId, offsetBytes, floats) {
  const payloadBytes = floats.length * 4;
  const buf = Buffer.alloc(HEADER_SIZE + payloadBytes);
  buf.writeUInt8(OP_APPEND, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(offsetBytes, 5); // offset ignored for APPEND but kept exact
  buf.writeUInt32LE(payloadBytes, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

function main() {
  const { bricks, brickSize, cols, lo, hi } = buildBricks();

  // One APPEND frame per brick, at the timestamp of its producing close, packed
  // at the next slot in the growing buffer. Frames stay time-sorted because the
  // close series is already in capture order.
  const frames = [];
  let offset = 0;
  for (const b of bricks) {
    frames.push({
      t: Math.min(b.t, DURATION_MS),
      b64: encodeAppend(BRICK_BUFFER, offset, rectFor(b, brickSize)),
    });
    offset += RECORD_BYTES;
  }

  // cadenceMs: mean gap between brick appends (informational; replay schedules
  // off each frame's absolute `t`).
  const lastT = frames.length ? frames[frames.length - 1].t : 0;
  const cadenceMs = frames.length > 1 ? Math.round(lastT / (frames.length - 1)) : 0;

  const out = {
    meta: {
      viewId: 'renko',
      durationMs: DURATION_MS,
      frameCount: frames.length,
      cadenceMs,
    },
    frames,
  };

  writeFileSync(
    resolve(__dirname, 'records.json'),
    JSON.stringify(out) + '\n',
    'utf8',
  );

  // Build-time framing report (so manifest.ts's baked transform can be checked
  // against the actual brick extents the timeline produces).
  process.stderr.write(
    `[renko.gen] bricks=${bricks.length} cols=${cols} ` +
      `price=[${lo.toFixed(2)},${hi.toFixed(2)}] brickSize=${brickSize.toFixed(4)} ` +
      `cadenceMs=${cadenceMs} lastT=${lastT}\n`,
  );
}

main();
