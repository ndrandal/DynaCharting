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
 * MULTI-BUFFER GROWTH (ENC-568): candles (10100) AND volume (10130) now grow
 * LIVE at replay. The replay engine advances every series' geometry vertexCount
 * as its buffer streams in (see `growthSeries` below) and the instanced backends
 * re-read the grown buffers (ENC-558). So the volume sub-pane bars now populate
 * across the tape instead of showing a single seeded instance.
 *
 * SMA(20) — NOW LIVE + DRAWN (ENC-585): the 20-period SMA overlay (buffer 10120)
 * streams live alongside the candles/volume and is drawn as a thick, connected,
 * anti-aliased polyline via lineAA@1. Its geometry format is `rect4` (16B per
 * instance = one [x0,y0,x1,y1] clip-space segment); a connected N-point SMA is
 * authored as N OVERLAPPING segments (segment i = [pt[i-1], pt[i]]) that APPEND
 * one-per-timestep in lockstep with the candle/volume APPENDs — see
 * records.gen.mjs. The lineAA Dawn backend re-gathers + re-counts instances
 * (instanceCount = bytes/16) on every buffer version bump (foundation #38/
 * ENC-569), so the streamed segments grow the drawn line live, exactly like the
 * instanced candle/volume buffers. The SMA values ride PRICE_TRANSFORM (lineAA
 * applies the DrawItem mat3 to both endpoints) so the line overlays the candles.
 *
 * (Was line2d@1 — a WebGPU LineList drawing DISCONNECTED vertex pairs from a
 * one-point-per-record append, which rendered thin/broken; replaced by lineAA@1.
 * See ENC-587 for the line-pipeline fix lineage.)
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync, GrowthSeries } from '../../src/engine/useReplay';

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

// NOTE: ENC-568 advances each series' geometry vertexCount in place
// (setGeometryVertexCount) instead of the old fresh-id rebind, so geometry ids
// no longer churn. The SMA/volume ids are kept well-separated for clarity.
const SMA_BUFFER = 10120; // rect4 16B (x0, y0, x1, y1) — connected line segments
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

    // --- SMA(20) overlay (price pane): thick AA connected polyline, lineAA@1 ---
    // rect4 geometry (16B/instance = one [x0,y0,x1,y1] segment); the records
    // stream one OVERLAPPING segment per timestep so the polyline is connected
    // and grows live (the lineAA backend re-counts instanceCount = bytes/16 on
    // each buffer version bump). Shares PRICE_TRANSFORM so it overlays the
    // candles. Colour: warm amber, ~2px — clearly reads over the OHLC bars.
    {
      cmd: 'createGeometry', id: SMA_GEOMETRY,
      vertexBufferId: SMA_BUFFER, format: 'rect4', vertexCount: 1,
    },
    { cmd: 'createDrawItem', id: SMA_DRAWITEM, layerId: PRICE_LAYER },
    { cmd: 'bindDrawItem', drawItemId: SMA_DRAWITEM, pipeline: 'lineAA@1', geometryId: SMA_GEOMETRY },
    {
      cmd: 'setDrawItemStyle', drawItemId: SMA_DRAWITEM,
      r: 0.95, g: 0.78, b: 0.3, a: 1, lineWidth: 2,
    },
    { cmd: 'attachTransform', drawItemId: SMA_DRAWITEM, transformId: PRICE_TRANSFORM },

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
 * PRIMARY growth descriptor: the candle6 buffer drives the X-anchor (its first
 * record's recordIndex re-frames PRICE_TRANSFORM's X) and the chrome overlay.
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

/**
 * MULTI-BUFFER GROWTH (ENC-568 / ENC-585): every live-growing geometry the
 * replay engine advances as its buffer streams in. All THREE series now grow
 * live — candles (candle6), volume (rect4), AND the SMA(20) overlay (rect4
 * segments, lineAA@1). The replay tallies each buffer's record count and bumps
 * the matching geometry's vertexCount; the instanced backends re-read the grown
 * candle/volume buffers (ENC-558) and the lineAA backend re-gathers + re-counts
 * instances (bytes/16) on each version bump (ENC-569), so the SMA polyline grows
 * one segment per timestep in lockstep with the bars.
 */
export const growthSeries: GrowthSeries[] = [
  { bufferId: CANDLE_BUFFER, geometryId: CANDLE_GEOMETRY, stride: 24 }, // candle6
  { bufferId: SMA_BUFFER, geometryId: SMA_GEOMETRY, stride: 16 }, // rect4 (lineAA segments)
  { bufferId: VOL_BUFFER, geometryId: VOL_GEOMETRY, stride: 16 }, // rect4
];
