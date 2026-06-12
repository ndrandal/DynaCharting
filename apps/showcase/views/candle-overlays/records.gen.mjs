#!/usr/bin/env node
/* apps/showcase/views/candle-overlays/records.gen.mjs
 *
 * ENC-585 — LIVE multi-series candle-overlays (candles + volume + SMA), with the
 * SMA(20) overlay STREAMING live and drawn as a thick AA polyline (lineAA@1).
 *
 * BACKGROUND. candle-overlays is a capture/replay catalog view. Its records.json
 * was originally CAPTURED off the faithful data path (Mock GMA -> embassy ->
 * dataplane WS). That capture already streams three series, one APPEND (op 1) per
 * timestep per buffer, all on the same ~75ms cadence:
 *
 *   • 10100 candle6 (24B: recordIndex, open, high, low, close, halfWidth)
 *   • 10130 rect4   (16B: x0, y0, x1, y1 — volume bar)
 *   • 10120 SMA     (8B:  x, value)  — POINTS, in price space, NOT DRAWN
 *
 * The SMA series was captured as one POINT per record (pos2). line2d@1 (a WebGPU
 * LineList) draws DISCONNECTED vertex pairs, so a point-per-append can't form a
 * connected polyline — hence the SMA was growth-tracked but never drawn.
 *
 * WHAT THIS GENERATOR DOES (the only thing that changes vs. the original capture):
 * it REWRITES the 10120 SMA series from pos2 POINTS into rect4 SEGMENTS so the
 * SMA can be drawn with lineAA@1 (foundation #38/ENC-569: a thick AA line whose
 * geometry is rect4 — 16B per instance, [x0,y0,x1,y1] one clip-space segment —
 * and which re-gathers + re-counts instances from CpuBufferStore on every buffer
 * version bump, so a streaming APPEND of segments grows the drawn line live).
 *
 * A connected N-point polyline = N OVERLAPPING segments emitted in lockstep with
 * the point stream: at timestep i we APPEND segment i =
 *
 *     i == 0 : [x0, v0, x0, v0]                 (degenerate; renders nothing —
 *                                                only one point exists yet)
 *     i  > 0 : [x[i-1], v[i-1], x[i], v[i]]      (connects prev point -> new point)
 *
 * so seg[i].p1 == seg[i+1].p0 for all i (the polyline is connected), and the SMA
 * grows ONE segment per timestep at the SAME frame timestamp as the candle/volume
 * APPENDs — all three series animate together.
 *
 * The SMA values stay in PRICE space (same as the candle6 OHLC values); the SMA
 * draw item rides PRICE_TRANSFORM (lineAA applies the DrawItem mat3 to both
 * endpoints), so it overlays the candles correctly.
 *
 * Candle (10100) and volume (10130) records are passed through UNCHANGED — same
 * count, same bytes, same timestamps. Only the SMA payload format/size changes.
 *
 * Idempotent: re-reads records.json, detects whether 10120 is already rect4 (16B),
 * and regenerates segments from the candle x-axis + the captured SMA values
 * (which it recovers either from the original 8B points or from the existing 16B
 * segments). Run: `node records.gen.mjs`.
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

const HEADER_SIZE = 13;
const OP_APPEND = 1;

const CANDLE_BUFFER = 10100; // candle6 24B
const SMA_BUFFER = 10120; // rect4 16B (was pos2 8B)
const VOL_BUFFER = 10130; // rect4 16B

// --- decode helpers ------------------------------------------------------

function b64ToBuf(b64) {
  return Buffer.from(b64, 'base64');
}

function decodeRecord(b64) {
  const buf = b64ToBuf(b64);
  return {
    op: buf.readUInt8(0),
    bufferId: buf.readUInt32LE(1),
    offset: buf.readUInt32LE(5),
    payloadBytes: buf.readUInt32LE(9),
    payload: buf.subarray(HEADER_SIZE, HEADER_SIZE + buf.readUInt32LE(9)),
  };
}

/** Pack one APPEND record (op 1) of float32 LE values into a base64 string. */
function encodeAppend(bufferId, offset, floats) {
  const payloadBytes = floats.length * 4;
  const buf = Buffer.alloc(HEADER_SIZE + payloadBytes);
  buf.writeUInt8(OP_APPEND, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(offset, 5);
  buf.writeUInt32LE(payloadBytes, 9);
  for (let i = 0; i < floats.length; i++) buf.writeFloatLE(floats[i], HEADER_SIZE + i * 4);
  return buf.toString('base64');
}

// --- load + split --------------------------------------------------------

const doc = JSON.parse(readFileSync(RECORDS_PATH, 'utf8'));
const frames = doc.frames;

// Recover the SMA (x, value) point sequence in stream order, from whatever the
// current 10120 payload is: 8B pos2 points (original capture) OR 16B rect4
// segments (a previous run of this generator). For a segment, the SMA point at
// step i is its endpoint p1 = (z, w); for the degenerate step-0 segment p1 == p0
// == the first point, so this recovers the same sequence either way.
const smaPoints = [];
let smaStride = null;
for (const f of frames) {
  const r = decodeRecord(f.b64);
  if (r.bufferId !== SMA_BUFFER) continue;
  if (smaStride === null) smaStride = r.payloadBytes;
  if (r.payloadBytes === 8) {
    smaPoints.push({ x: r.payload.readFloatLE(0), v: r.payload.readFloatLE(4) });
  } else if (r.payloadBytes === 16) {
    smaPoints.push({ x: r.payload.readFloatLE(8), v: r.payload.readFloatLE(12) });
  } else {
    throw new Error(`unexpected SMA payload size ${r.payloadBytes} (want 8 or 16)`);
  }
}

if (smaPoints.length === 0) throw new Error('no SMA (10120) records found');

// --- rebuild the SMA series as connected rect4 segments ------------------
//
// Walk the frames in order; replace each 10120 record's payload with a 16B
// rect4 segment connecting the previous SMA point to this one (degenerate for
// the first), keeping the record's original timestamp + stream position. The
// APPEND offset is ignored by the ingest processor (it always appends at the
// buffer tail), but we set the running byte offset for a clean monotonic stride.

let smaIndex = 0;
let smaByteOffset = 0;
const outFrames = frames.map((f) => {
  const r = decodeRecord(f.b64);
  if (r.bufferId !== SMA_BUFFER) return f; // candle/volume: pass through verbatim

  const cur = smaPoints[smaIndex];
  const prev = smaIndex > 0 ? smaPoints[smaIndex - 1] : cur; // step 0 -> degenerate
  const seg = [prev.x, prev.v, cur.x, cur.v];
  const b64 = encodeAppend(SMA_BUFFER, smaByteOffset, seg);
  smaIndex += 1;
  smaByteOffset += seg.length * 4; // 16B
  return { t: f.t, b64 };
});

doc.frames = outFrames;
writeFileSync(RECORDS_PATH, JSON.stringify(doc) + '\n');

// --- summary -------------------------------------------------------------

const counts = { [CANDLE_BUFFER]: 0, [SMA_BUFFER]: 0, [VOL_BUFFER]: 0 };
for (const f of outFrames) counts[decodeRecord(f.b64).bufferId]++;
console.log(
  `records.gen: wrote ${outFrames.length} frames ` +
    `(candle ${counts[CANDLE_BUFFER]}, SMA ${counts[SMA_BUFFER]} segments [was ${smaStride}B points], ` +
    `volume ${counts[VOL_BUFFER]}); SMA now rect4/16B for lineAA@1.`,
);
