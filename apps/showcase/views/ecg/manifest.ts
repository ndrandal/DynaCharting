/* apps/showcase/views/ecg/manifest.ts
 *
 * CROSS-DOMAIN view (ENC-538 / T4.6) — "medical / ECG monitor".
 *
 * A STREAMING computed view (ENC-570): the engine's native `line2d@1` pipeline
 * draws a green-on-dark ECG trace that GROWS live like a hospital monitor — the
 * PQRST waveform is no longer baked as a static geometry, it is streamed sample-
 * by-sample into a growing `pos2_clip` vertex buffer over the recorded timeline
 * (records.json), the SAME live data path the candle views use (no `uploads`).
 *
 * --- The streaming model (compound-line stride-8, mirrors candle-overlays SMA) ---
 *
 * Each ECG sample becomes ONE pos2_clip vertex appended to the trace buffer:
 *   [ x = sampleIndex (static, per the record's position),
 *     y = amplitude   (the streamed value) ]
 * = 2×f32 = 8 bytes. As records land, useReplay's GrowthSync advances
 * geometry.vertexCount = floor(byteLength / 8), so the line lengthens to the
 * right exactly like the live candle6 series lengthens. The trace buffer starts
 * EMPTY (byteLength 0) and the records.json frames grow it — there is no baked
 * `uploads` block any more. records.json is produced by records.gen.mjs from the
 * synthetic dataset (apps/showcase/data/synthetic/ecg.json: 250 Hz, 72 bpm).
 *
 * --- DEPENDENCY: ENC-569 (line2d / vertex-buffer GROWTH) ---
 *
 * line2d@1 is a WebGPU **LineList** (GL_LINES) pipeline — discrete segment PAIRS,
 * not a strip. A growing one-vertex-per-sample append forms a fully connected,
 * animating polyline only once ENC-569 lands the line2d growth fix (the very
 * same limitation that keeps candle-overlays' SMA captured-but-not-drawn). This
 * manifest + records.json AUTHOR the streaming view now; the buffer/geometry
 * grow correctly and the live scroll animates after ENC-569 merges.
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
const TRACE_BUFFER = 500; // pos2_clip 8B (x, y) — MUST match records.gen.mjs
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

    // Trace buffer (grown at replay time, sample-by-sample) -> line2d geometry.
    // byteLength 0: the buffer starts EMPTY and the records.json frames grow it.
    { cmd: 'createBuffer', id: TRACE_BUFFER, byteLength: 0 },
    // vertexCount starts at 2 (line2d LineList needs >=1 segment = 2 verts); the
    // replay engine advances it to the real vertex count (bytes / 8) as samples
    // arrive — and, once ENC-569 lands the line2d growth fix, the trace draws and
    // scrolls live. (vertexCount derives from buffer size as the buffer grows.)
    {
      cmd: 'createGeometry',
      id: TRACE_GEOMETRY,
      vertexBufferId: TRACE_BUFFER,
      format: 'pos2_clip',
      vertexCount: 2,
    },

    // Data->clip transform. The baked sx/sy/tx/ty are seeded from view.json by
    // the replay controller (here we create it; values set via setTransform).
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    { cmd: 'createDrawItem', id: TRACE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: TRACE_DRAWITEM, pipeline: 'line2d@1', geometryId: TRACE_GEOMETRY },
    // Classic green medical-monitor trace.
    { cmd: 'setDrawItemColor', drawItemId: TRACE_DRAWITEM, r: 0.18, g: 0.95, b: 0.42, a: 1 },
    { cmd: 'setDrawItemStyle', drawItemId: TRACE_DRAWITEM, lineWidth: 2, pointSize: 0 },
    { cmd: 'attachTransform', drawItemId: TRACE_DRAWITEM, transformId: TRANSFORM },
  ],
  // STREAMING: no uploads — the trace buffer is grown sample-by-sample at replay
  // time from records.json via useReplay over the data plane (the live path).
};

/**
 * Growth descriptor: the pos2_clip trace buffer is the live-growing series the
 * replay engine advances. Each appended record is one pos2_clip VERTEX
 * (x = sampleIndex, y = amplitude) = 8 bytes, so vertexCount = byteLength / 8 as
 * the trace lengthens. xField = 0 (x is the first f32 of each vertex).
 *
 * NOTE: line2d@1 is WebGPU LineList — the growing trace becomes a fully
 * connected, animating polyline once ENC-569 lands the line2d growth fix (see
 * the header note). The descriptor is authored now so the buffer/geometry grow.
 */
export const growth: GrowthSync = {
  bufferId: TRACE_BUFFER,
  geometryId: TRACE_GEOMETRY,
  drawItemId: TRACE_DRAWITEM,
  layerId: LAYER,
  stride: 8, // pos2_clip: [x, y] = 2×f32
  format: 'pos2_clip',
  pipeline: 'line2d@1',
  transformId: TRANSFORM,
  xField: 0, // byte offset of x (sampleIndex) within a pos2_clip vertex
};
