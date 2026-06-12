#!/usr/bin/env node
/* apps/showcase/views/ecg/records.gen.mjs — ECG streaming-records generator (ENC-587)
 *
 * Zero-dependency Node generator. Reads the synthetic ECG amplitude series from
 * apps/showcase/data/synthetic/ecg.json and emits a STREAMING record timeline
 * (records.json) that GROWS the ECG view's rect4 lineAA buffer one CONNECTED
 * segment per sample, so the trace draws live as a thick, anti-aliased hospital
 * monitor waveform instead of being baked as a static geometry.
 *
 *   node apps/showcase/views/ecg/records.gen.mjs
 *
 * --- The streaming model (connected rect4 segments, lineAA stride-16) ---
 *
 * The candle views grow an instanced candle6 buffer record-by-record; the ECG
 * trace grows a `lineAA@1` (rect4) buffer the SAME way. Each NEW sample becomes
 * ONE rect4 segment connecting the PREVIOUS point to the new one:
 *   [ x0 = prevSampleIndex, y0 = prevAmplitude,
 *     x1 = thisSampleIndex, y1 = thisAmplitude ]
 * = 4×f32 = 16 bytes, APPENDed to the trace buffer. Consecutive segments SHARE
 * an endpoint (segment i.p1 == segment i+1.p0), so the appended instances form a
 * single CONNECTED polyline. As records land, useReplay's GrowthSync advances
 * geometry.vertexCount = floor(byteLength / 16) (= the lineAA instance count), so
 * the trace lengthens to the right exactly like the live candle series lengthens.
 *
 * --- DEPENDENCY: ENC-569 (lineAA / instance-buffer GROWTH) ---
 *
 * lineAA@1 is a WebGPU INSTANCED quad-expansion pipeline: each rect4 segment is
 * expanded into a thick AA quad, so a growing one-segment-per-sample append forms
 * a fully connected, animating thick polyline. The lineAA backend re-counts
 * instanceCount = bufferBytes / 16 on each buffer-version bump (ENC-569), so the
 * streaming APPEND growth path used by line2d works unchanged here (ENC-587).
 *
 * --- Output (apps/showcase/views/<id>/records.json, Records shape) ---
 *
 *   { meta:{ viewId, durationMs, frameCount, cadenceMs }, frames:[{ t, b64 }] }
 *
 * where each frame's `b64` is base64 of one binary dataplane batch:
 *   [1B op=1 APPEND][4B bufferId LE][4B offset=0 LE][4B payloadBytes LE][payload f32 LE...]
 * (the exact format embassy/useReplay use — see src/scene/commands.ts).
 */

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
// apps/showcase/views/ecg -> apps/showcase
const SHOWCASE_DIR = join(__dirname, '..', '..');
const DATASET = join(SHOWCASE_DIR, 'data', 'synthetic', 'ecg.json');
const OUT = join(__dirname, 'records.json');

// --- contract IDs (MUST match manifest.ts) ---
const VIEW_ID = 'ecg';
const TRACE_BUFFER = 500; // rect4 trace buffer the manifest grows
const STRIDE = 16; // rect4 segment = [x0, y0, x1, y1] = 4×f32 = 16 bytes

// --- binary record packing (mirrors src/scene/commands.ts encodeUpload) ---
const OP_APPEND = 1;
const HEADER_SIZE = 13;

/** Pack one APPEND batch of f32 values for `bufferId` into base64. */
function encodeAppend(bufferId, floats) {
  const payloadBytes = floats.length * 4;
  const buf = Buffer.alloc(HEADER_SIZE + payloadBytes);
  buf.writeUInt8(OP_APPEND, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(0, 5); // APPEND ignores offset
  buf.writeUInt32LE(payloadBytes, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

// --- read amplitude samples ---
const dataset = JSON.parse(readFileSync(DATASET, 'utf8'));
const samples = dataset.updates
  .filter((u) => u.field === 'amp')
  .map((u) => u.value);
const sampleCount = samples.length; // 5000
const cadenceMs = dataset.meta.cadenceMs ?? 4; // 4 ms/sample (250 Hz)
const durationMs = dataset.meta.durationMs ?? sampleCount * cadenceMs; // 20000

// --- frame the stream at a ~30 fps render cadence ---
// One rect4 SEGMENT per NEW sample, connecting the previous point to this one
// (x=sampleIndex, y=amplitude); a frame batches the whole-number of samples that
// fall in its ~FRAME_MS window so the buffer grows smoothly (≈30 fps) instead of
// ~5000 single-sample frames. x is the sampleIndex; consecutive segments share an
// endpoint, so the appended instances form a single connected polyline.
const FRAME_MS = 33; // ~30 fps render cadence
const samplesPerFrame = Math.max(1, Math.round(FRAME_MS / cadenceMs)); // ~8

const frames = [];
for (let i = 0; i < sampleCount; i += samplesPerFrame) {
  const end = Math.min(i + samplesPerFrame, sampleCount);
  const floats = [];
  for (let s = i; s < end; s++) {
    // A segment needs a previous point; sample 0 starts the trace with no
    // segment of its own (it becomes the p0 of segment 1).
    if (s === 0) continue;
    floats.push(s - 1); // x0 = prev sampleIndex
    floats.push(samples[s - 1]); // y0 = prev amplitude
    floats.push(s); // x1 = this sampleIndex
    floats.push(samples[s]); // y1 = this amplitude
  }
  // The first window may yield no segment only if it is empty; with
  // samplesPerFrame >= 2 the first window always produces >= 1 segment.
  if (floats.length === 0) continue;
  frames.push({ t: i * cadenceMs, b64: encodeAppend(TRACE_BUFFER, floats) });
}

const records = {
  meta: {
    viewId: VIEW_ID,
    durationMs,
    frameCount: frames.length,
    cadenceMs: FRAME_MS,
  },
  frames,
};

writeFileSync(OUT, JSON.stringify(records));

// eslint-disable-next-line no-console
console.log(
  `[ecg.gen] ${sampleCount} samples -> ${frames.length} frames over ${durationMs}ms ` +
    `(${samplesPerFrame} samples/frame, stride ${STRIDE}, buffer ${TRACE_BUFFER}) -> ${OUT}`,
);
