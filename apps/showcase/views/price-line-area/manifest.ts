/* apps/showcase/views/price-line-area/manifest.ts
 *
 * Price baseline-area — AAPL lastPrice (tier native, ≈ TradingView baseline/area).
 *
 * A single streamed scalar (AAPL lastPrice) drawn as a filled baseline AREA: per
 * tick, embassy packs a compound `rect4` record [x0, y0, x1, y1] where x0/x1
 * bracket the record index (a 1-wide column via recordIndexPlusConst ±0.5), y0 is
 * a constant baseline (price-space, below the series), and y1 is the streamed
 * lastPrice (slotBit 0). instancedRect@1 fills each column from the baseline up
 * to the price, so the columns abut into a solid area-to-baseline band — the
 * TradingView baseline/area idiom, expressed entirely from one streamed scalar.
 *
 * WHY area-not-line: the engine's line2d@1 is WebGPU LineList (GL_LINES) — it
 * draws disjoint vertex PAIRS at 1px with no width control, so a one-point-per-
 * record append cannot form a connected, visible polyline. The filled
 * instancedRect baseline-area is the native, robustly-rendering equivalent for a
 * single streamed scalar (and reads as TradingView's area mode).
 *
 * Catalog shape (CONTRACT-view-catalog.md): NO `uploads`; buffer 10110 is grown
 * record-by-record at replay time by useReplay. Transform framing (baked Y
 * price→clip + xAnchor for X) lives in view.json; GROWTH below advances the
 * rect4 instance count as records land.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (shared with instruction.json only via the buffer) ---
const PANE = 10000;
const LAYER = 10001;
export const TRANSFORM = 10050;
const AREA_BUFFER = 10110; // rect4 16B (x0, y0, x1, y1) — MUST match instruction bufferId
const AREA_GEOMETRY = 10210;
const AREA_DRAWITEM = 10310;

export const manifest: SceneManifest = {
  label: 'Price Area — AAPL',
  commands: [
    // Pane + layer.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Area buffer (grown at replay time) -> rect4 geometry -> drawItem.
    { cmd: 'createBuffer', id: AREA_BUFFER, byteLength: 0 },
    {
      cmd: 'createGeometry',
      id: AREA_GEOMETRY,
      vertexBufferId: AREA_BUFFER,
      format: 'rect4',
      vertexCount: 1,
    },

    // Data->clip transform; sx/sy/tx/ty seeded from view.json by the replay controller.
    { cmd: 'createTransform', id: TRANSFORM },

    { cmd: 'createDrawItem', id: AREA_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: AREA_DRAWITEM, pipeline: 'instancedRect@1', geometryId: AREA_GEOMETRY },
    {
      cmd: 'setDrawItemStyle',
      drawItemId: AREA_DRAWITEM,
      r: 0.24, g: 0.86, b: 0.52, a: 0.65,
    },
    { cmd: 'attachTransform', drawItemId: AREA_DRAWITEM, transformId: TRANSFORM },
  ],
  // No uploads — data arrives via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth/X-anchor descriptor for the replay engine. instancedRect@1 over rect4:
 * the renderer draws geometry.vertexCount instances and the dataplane only grows
 * the buffer's bytes, so vertexCount = floor(byteLength / 16) is advanced as
 * records land (fresh-geometry rebind, ENC-558). xAnchor re-derives the X part
 * of the transform from the first replayed record's x0 (field @0).
 */
export const growth: GrowthSync = {
  bufferId: AREA_BUFFER,
  geometryId: AREA_GEOMETRY,
  drawItemId: AREA_DRAWITEM,
  layerId: LAYER,
  stride: 16, // rect4: [x0, y0, x1, y1] = 4×f32
  format: 'rect4',
  pipeline: 'instancedRect@1',
  transformId: TRANSFORM,
  xField: 0, // byte offset of x0 (recordIndex-0.5) within a rect4 record
};
