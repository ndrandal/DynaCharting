/* apps/showcase/views/candles-aapl/manifest.ts
 *
 * The candle view's SceneManifest (migrated from src/scene/candleManifest.ts,
 * the ENC-520 vertical slice). Per CONTRACT-view-catalog.md a catalog manifest
 * has NO `uploads` — the candle6 buffer (10100) is grown record-by-record at
 * runtime by useReplay, replaying the captured embassy dataplane frames
 * (records.json) into host.enqueueData. The showcase owns this manifest;
 * embassy's own scene-init is ignored. Buffer 10100 (candle6, 24B records:
 * [x, open, high, low, close, halfWidth]) is the only thing the manifest and
 * the instruction.json agree on (CONTRACT-buffer-id.md).
 *
 * Transform framing lives in view.json (`transform` = baked Y + X; `xAnchor`
 * = re-anchor X to the first replayed record's recordIndex). The replay
 * controller bakes view.json.transform into TRANSFORM (10050) and, when
 * xAnchor is set, re-derives the X part from the first frame so candles always
 * frame from the left regardless of embassy's absolute recordIndex at capture.
 * GROWTH below is the descriptor the replay engine uses to advance the
 * instanced geometry's vertexCount as records land (ENC-558: the instanced
 * backend caches its GPU buffer per geometryId, so growth = fresh-geometry
 * rebind; reused verbatim from the proven slice).
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (shared with instruction.json only via the buffer) ---
const PANE = 10000;
const LAYER = 10001;
export const TRANSFORM = 10050;
const CANDLE_BUFFER = 10100; // candle6 24B — MUST match instruction bufferId
const CANDLE_GEOMETRY = 10200;
const CANDLE_DRAWITEM = 10300;

export const manifest: SceneManifest = {
  label: 'Candlestick — AAPL',
  commands: [
    // Pane + layer.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Candle buffer (grown at replay time) -> candle6 geometry -> drawItem.
    { cmd: 'createBuffer', id: CANDLE_BUFFER, byteLength: 0 },
    // vertexCount starts at 1 (instanced pipelines reject 0); the replay engine
    // advances it to the real record count (bytes/24) as frames arrive.
    {
      cmd: 'createGeometry',
      id: CANDLE_GEOMETRY,
      vertexBufferId: CANDLE_BUFFER,
      format: 'candle6',
      vertexCount: 1,
    },

    // Data->clip transform. The baked sx/sy/tx/ty are seeded from view.json by
    // the replay controller (here we create it; values set via setTransform).
    { cmd: 'createTransform', id: TRANSFORM },

    { cmd: 'createDrawItem', id: CANDLE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: CANDLE_DRAWITEM, pipeline: 'instancedCandle@1', geometryId: CANDLE_GEOMETRY },
    // instancedCandle@1 auto-colors from candle6's open vs close.
    {
      cmd: 'setDrawItemStyle',
      drawItemId: CANDLE_DRAWITEM,
      colorUpR: 0.2, colorUpG: 0.75, colorUpB: 0.45, colorUpA: 1,
      colorDownR: 0.85, colorDownG: 0.3, colorDownB: 0.3, colorDownA: 1,
    },
    { cmd: 'attachTransform', drawItemId: CANDLE_DRAWITEM, transformId: TRANSFORM },
  ],
  // No uploads — data arrives via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth/X-anchor descriptor for the replay engine. The renderer draws
 * geometry.vertexCount instances; the dataplane only grows the buffer's bytes,
 * so the replay engine advances vertexCount = floor(byteLength / stride) as
 * records land (via fresh-geometry rebind — ENC-558 backend caching workaround).
 * When xAnchor is set, it also re-derives the transform's X part from the first
 * replayed record so candles frame from the left. The baked sy/ty (price→clip)
 * come from view.json.transform and are preserved across the X re-anchor.
 */
export const growth: GrowthSync = {
  bufferId: CANDLE_BUFFER,
  geometryId: CANDLE_GEOMETRY,
  drawItemId: CANDLE_DRAWITEM,
  layerId: LAYER,
  stride: 24, // candle6: [x, open, high, low, close, halfWidth] = 6×f32
  format: 'candle6',
  pipeline: 'instancedCandle@1',
  transformId: TRANSFORM,
  xField: 0, // byte offset of x (recordIndex) within a candle6 record
};
