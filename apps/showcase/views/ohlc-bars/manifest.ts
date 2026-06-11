/* apps/showcase/views/ohlc-bars/manifest.ts
 *
 * OHLC bar chart — AAPL (tier native, ≈ Bloomberg / most platforms).
 *
 * Structurally identical to candles-aapl (CONTRACT-view-catalog.md catalog
 * shape: NO `uploads`; the candle6 buffer 10100 is grown record-by-record at
 * replay time by useReplay over the captured embassy frames). The ONLY
 * difference from the candlestick view is the rendered width: the compound
 * static `halfWidth` is a small value (0.08 vs the candle's 0.4) so each
 * instancedCandle@1 instance draws as a thin OHLC *bar* (the open/close ticks
 * read as a slim stick around the high-low range) rather than a filled candle
 * body — the Bloomberg / "most platforms" OHLC-bar idiom.
 *
 * Buffer 10100 (candle6, 24B records: [x, open, high, low, close, halfWidth])
 * is the only thing manifest and instruction.json agree on (CONTRACT-buffer-id.md).
 * Transform framing lives in view.json (baked Y price→clip + xAnchor for X);
 * GROWTH below drives the instanced vertexCount as records land.
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
  label: 'OHLC Bars — AAPL',
  commands: [
    // Pane + layer.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // OHLC buffer (grown at replay time) -> candle6 geometry -> drawItem.
    { cmd: 'createBuffer', id: CANDLE_BUFFER, byteLength: 0 },
    {
      cmd: 'createGeometry',
      id: CANDLE_GEOMETRY,
      vertexBufferId: CANDLE_BUFFER,
      format: 'candle6',
      vertexCount: 1,
    },

    // Data->clip transform; sx/sy/tx/ty seeded from view.json by the replay controller.
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

/** Growth/X-anchor descriptor for the replay engine (see candles-aapl). */
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
