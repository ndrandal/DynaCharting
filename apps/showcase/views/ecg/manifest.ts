/* apps/showcase/views/ecg/manifest.ts
 *
 * CROSS-DOMAIN view (ENC-538 / T4.6) — "medical / ECG monitor".
 *
 * A STREAMING computed view (ENC-570): the engine's native `lineAA@1` pipeline
 * draws a THICK, anti-aliased green-on-dark ECG trace that GROWS live like a
 * hospital monitor — the PQRST waveform is no longer baked as a static geometry,
 * it is streamed segment-by-segment into a growing `rect4` instance buffer over
 * the recorded timeline (records.json), the SAME live data path the candle views
 * use (no `uploads`).
 *
 * --- The streaming model (connected rect4 segments, lineAA stride-16) ---
 *
 * Each NEW ECG sample becomes ONE rect4 instance appended to the trace buffer:
 *   [ x0 = prevSampleIndex, y0 = prevAmplitude,
 *     x1 = thisSampleIndex, y1 = thisAmplitude ]
 * = 4×f32 = 16 bytes — ONE clip-space line segment p0->p1. Consecutive segments
 * SHARE an endpoint (segment i.p1 == segment i+1.p0), so the appended instances
 * form a single CONNECTED polyline — the bold ECG trace. As records land,
 * useReplay's GrowthSync advances geometry.vertexCount = floor(byteLength / 16)
 * (= the lineAA instance count), so the line lengthens to the right exactly like
 * the live candle6 series lengthens. The trace buffer starts EMPTY (byteLength 0)
 * and the records.json frames grow it — there is no baked `uploads` block.
 * records.json is produced by records.gen.mjs from the synthetic dataset
 * (apps/showcase/data/synthetic/ecg.json: 250 Hz, 72 bpm).
 *
 * --- DEPENDENCY: ENC-569 (lineAA / instance-buffer GROWTH) ---
 *
 * lineAA@1 is a WebGPU INSTANCED quad-expansion pipeline: each rect4 instance is
 * expanded into a thick AA quad in the vertex shader, so a growing one-segment-
 * per-sample append forms a fully connected, animating thick polyline. The lineAA
 * backend (ENC-569) re-gathers + re-counts its instance buffer from CpuBufferStore
 * on every buffer-version bump, deriving instanceCount = bufferBytes / 16, so it
 * supports the same streaming APPEND growth path line2d used (ENC-587).
 *
 * The cross-domain point still holds: the SAME vector renderer that streams
 * candles streams a medical monitor trace — the geometry is domain-agnostic.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (local to this view; scene is reset between views) ---
const PANE = 100;
const LAYER = 101;
export const TRANSFORM = 110;
const TRACE_BUFFER = 500; // rect4 16B (x0,y0,x1,y1) — MUST match records.gen.mjs
const TRACE_GEOMETRY = 200;
const TRACE_DRAWITEM = 300;

// Baked data->clip transform (clip_x = sx*x + tx, clip_y = sy*y + ty).
//   x: sampleIndex 0..1999 (first ~8 s window, ~10 beats) -> clip [-0.92, 0.92]
//   y: amplitude  [-0.40, 1.20] mV (matches view.json chrome) -> clip [-0.65, 0.82]
// The replay controller bakes view.json.transform into TRANSFORM (110); these
// constants mirror view.json so the static framing matches the overlay chrome.
const SX = 0.00092046; // 1.84 clip / 1999 indices
const SY = 0.994655;
const TX = -0.92;
const TY = -0.335291;

export const manifest: SceneManifest = {
  label: 'ECG monitor — PQRST trace',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.97, clipXMax: 0.97, clipYMin: -0.95, clipYMax: 0.95 },
    // Dark, slightly green-tinted "monitor" background.
    { cmd: 'setPaneClearColor', id: PANE, r: 0.02, g: 0.05, b: 0.04, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Trace buffer (grown at replay time, segment-by-segment) -> lineAA geometry.
    // byteLength 0: the buffer starts EMPTY and the records.json frames grow it.
    { cmd: 'createBuffer', id: TRACE_BUFFER, byteLength: 0 },
    // vertexCount = the lineAA instance count (one rect4 segment per instance).
    // It starts at 1 (a valid >=1 instance count); the replay engine advances it
    // to the real instance count (bytes / 16) as samples arrive, and the lineAA
    // backend re-counts instanceCount = bufferBytes / 16 on each version bump, so
    // the thick connected trace draws and scrolls live as the buffer grows.
    {
      cmd: 'createGeometry',
      id: TRACE_GEOMETRY,
      vertexBufferId: TRACE_BUFFER,
      format: 'rect4',
      vertexCount: 1,
    },

    // Data->clip transform. The baked sx/sy/tx/ty are seeded from view.json by
    // the replay controller (here we create it; values set via setTransform).
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    { cmd: 'createDrawItem', id: TRACE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: TRACE_DRAWITEM, pipeline: 'lineAA@1', geometryId: TRACE_GEOMETRY },
    // Classic green medical-monitor trace, thick + anti-aliased (lineAA@1).
    { cmd: 'setDrawItemColor', drawItemId: TRACE_DRAWITEM, r: 0.18, g: 0.95, b: 0.42, a: 1 },
    // lineWidth is in pixels for lineAA@1 (expanded to a quad of width + AA fringe
    // in the vertex shader); ~3px gives a bold hospital-monitor stroke.
    { cmd: 'setDrawItemStyle', drawItemId: TRACE_DRAWITEM, lineWidth: 3, pointSize: 0 },
    { cmd: 'attachTransform', drawItemId: TRACE_DRAWITEM, transformId: TRANSFORM },
  ],
  // STREAMING: no uploads — the trace buffer is grown sample-by-sample at replay
  // time from records.json via useReplay over the data plane (the live path).
};

/**
 * Growth descriptor: the rect4 trace buffer is the live-growing series the
 * replay engine advances. Each appended record is one rect4 SEGMENT
 * (x0, y0, x1, y1) = 16 bytes, so vertexCount (= the lineAA instance count)
 * = byteLength / 16 as the trace lengthens. xField = 0 (x0 is the first f32 of
 * each segment).
 *
 * NOTE: lineAA@1 is a WebGPU instanced quad-expansion pipeline — the growing
 * trace is a fully connected, animating thick polyline because consecutive rect4
 * segments share endpoints (records.gen.mjs emits [prevPt -> newPt] per sample).
 * The lineAA backend re-counts instanceCount = bufferBytes / 16 on each version
 * bump (ENC-569), so the buffer/geometry grow and the trace scrolls live.
 */
export const growth: GrowthSync = {
  bufferId: TRACE_BUFFER,
  geometryId: TRACE_GEOMETRY,
  drawItemId: TRACE_DRAWITEM,
  layerId: LAYER,
  stride: 16, // rect4: [x0, y0, x1, y1] = 4×f32
  format: 'rect4',
  pipeline: 'lineAA@1',
  transformId: TRANSFORM,
  xField: 0, // byte offset of x0 (sampleIndex) within a rect4 segment
};
