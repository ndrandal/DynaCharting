/* apps/showcase/views/candle-overlays/manifest.ts
 *
 * Candles + volume sub-pane — AAPL (tier native, ≈ TradingView). The richest
 * native view: one JSON manifest declares multiple coordinated series across two
 * panes, all fed off the same captured embassy dataplane frames
 * (CONTRACT-view-catalog.md catalog shape — NO `uploads`):
 *
 *   • PRICE PANE (upper band): OHLC candles (candle6 / instancedCandle@1, buffer
 *     10100, compound OHLC join).
 *   • VOLUME PANE (lower band): cumulative volume bars (rect4 / instancedRect@1,
 *     buffer 10130, compound: x bracket via recordIndexPlusConst, y0 const 0,
 *     y1 = streamed volume) on its OWN pane + transform, because volume dwarfs
 *     price and needs an independent value→clip mapping.
 *
 * Modeled on embassy's candles-with-overlays-v1 recipe SHAPE but authored as a
 * showcase manifest (the showcase owns geometry; embassy is a generic pump).
 *
 * SMA(20) — CAPTURED BUT NOT DRAWN (two compounding frontier limits): a 20-period
 * SMA is subscribed + bound (buffer 10120) so the capture exercises the real
 * multi-buffer pipeline and the records are present, BUT no SMA draw item is
 * created, because (a) line2d@1 is WebGPU LineList (GL_LINES) — a one-point-per-
 * record append can't form a connected polyline, and (b) the replay engine's
 * GrowthSync advances only ONE buffer per view (the candle6 buffer here), so a
 * second growing series wouldn't track anyway. Drawing it would also blank the
 * scene (a LineList draw with an odd/seed vertexCount aborts the render pass).
 * The SMA line is the natural next step once the engine ships width-capable,
 * stream-connectable lines + the harness gains multi-series growth.
 *
 * VOLUME — captured + drawn, but NOT growing at replay for the same single-growth
 * reason: the volume rect4 buffer (10130) is applied and renders its seeded
 * instance, but its vertexCount does not advance during replay (only candle6
 * does). The full volume series is in records.json for a future multi-growth run.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs ---
const PRICE_PANE = 10000;
const PRICE_LAYER = 10001;
export const PRICE_TRANSFORM = 10050;
const VOL_PANE = 10002;
const VOL_LAYER = 10003;
const VOL_TRANSFORM = 10051;

const CANDLE_BUFFER = 10100; // candle6 24B  — MUST match instruction bufferId
const CANDLE_GEOMETRY = 10200;
const CANDLE_DRAWITEM = 10300;

// NOTE: the replay engine's growth rebind mints fresh candle geometry ids in
// CANDLE_GEOMETRY + 1 .. +98 (10201..10298) as the candle buffer grows. The SMA
// and volume geometry ids MUST sit OUTSIDE that band, or a rebind would collide
// (createGeometry ID_TAKEN) and stall the candle growth. Hence 10500/10600.
const SMA_BUFFER = 10120; // pos2_clip 8B (x, y)
const SMA_GEOMETRY = 10500;
const SMA_DRAWITEM = 10320;

const VOL_BUFFER = 10130; // rect4 16B (x0, y0, x1, y1)
const VOL_GEOMETRY = 10600;
const VOL_DRAWITEM = 10330;

export const manifest: SceneManifest = {
  label: 'Candles + Volume — AAPL',
  commands: [
    // --- Price pane (upper band): candles ---
    { cmd: 'createPane', id: PRICE_PANE },
    { cmd: 'setPaneRegion', id: PRICE_PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.20, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PRICE_PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: PRICE_LAYER, paneId: PRICE_PANE },

    // --- Volume pane (lower band): its own region + transform ---
    { cmd: 'createPane', id: VOL_PANE },
    { cmd: 'setPaneRegion', id: VOL_PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: -0.30 },
    { cmd: 'setPaneClearColor', id: VOL_PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: VOL_LAYER, paneId: VOL_PANE },

    // --- Buffers ---
    { cmd: 'createBuffer', id: CANDLE_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: SMA_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: VOL_BUFFER, byteLength: 0 },

    // --- Transforms ---
    // PRICE_TRANSFORM is seeded from view.json by the switching controller (it
    // bakes only growth.transformId) and X-anchored at replay. VOL_TRANSFORM is
    // NOT touched by the controller, so we bake its volume→clip mapping here in
    // the manifest (volume 0..~165k → the volume pane's clip band [-0.90,-0.42];
    // the X scale matches the price pane's record-index→clip so bars align).
    { cmd: 'createTransform', id: PRICE_TRANSFORM },
    { cmd: 'createTransform', id: VOL_TRANSFORM },
    { cmd: 'setTransform', id: VOL_TRANSFORM, sx: 0.011333333, sy: 0.000003333, tx: -0.85, ty: -0.9 },

    // --- Candles (price pane) ---
    {
      cmd: 'createGeometry', id: CANDLE_GEOMETRY,
      vertexBufferId: CANDLE_BUFFER, format: 'candle6', vertexCount: 1,
    },
    { cmd: 'createDrawItem', id: CANDLE_DRAWITEM, layerId: PRICE_LAYER },
    { cmd: 'bindDrawItem', drawItemId: CANDLE_DRAWITEM, pipeline: 'instancedCandle@1', geometryId: CANDLE_GEOMETRY },
    {
      cmd: 'setDrawItemStyle', drawItemId: CANDLE_DRAWITEM,
      colorUpR: 0.2, colorUpG: 0.75, colorUpB: 0.45, colorUpA: 1,
      colorDownR: 0.85, colorDownG: 0.3, colorDownB: 0.3, colorDownA: 1,
    },
    { cmd: 'attachTransform', drawItemId: CANDLE_DRAWITEM, transformId: PRICE_TRANSFORM },

    // --- Volume bars (volume pane, own transform) ---
    {
      cmd: 'createGeometry', id: VOL_GEOMETRY,
      vertexBufferId: VOL_BUFFER, format: 'rect4', vertexCount: 1,
    },
    { cmd: 'createDrawItem', id: VOL_DRAWITEM, layerId: VOL_LAYER },
    { cmd: 'bindDrawItem', drawItemId: VOL_DRAWITEM, pipeline: 'instancedRect@1', geometryId: VOL_GEOMETRY },
    {
      cmd: 'setDrawItemStyle', drawItemId: VOL_DRAWITEM,
      r: 0.32, g: 0.45, b: 0.78, a: 0.8,
    },
    { cmd: 'attachTransform', drawItemId: VOL_DRAWITEM, transformId: VOL_TRANSFORM },
  ],
  // No uploads — data arrives via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth descriptor: the candle6 buffer is the live-growing series the replay
 * engine advances (useReplay supports one growth target per view). xAnchor
 * re-derives the price transform's X from the first candle record. SMA (10120)
 * and volume (10130) buffers are captured + applied but do not grow at replay
 * until the harness supports multi-series growth (see header note).
 */
export const growth: GrowthSync = {
  bufferId: CANDLE_BUFFER,
  geometryId: CANDLE_GEOMETRY,
  drawItemId: CANDLE_DRAWITEM,
  layerId: PRICE_LAYER,
  stride: 24, // candle6: [x, open, high, low, close, halfWidth]
  format: 'candle6',
  pipeline: 'instancedCandle@1',
  transformId: PRICE_TRANSFORM,
  xField: 0,
};
