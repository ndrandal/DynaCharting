/* apps/showcase/views/multipane-indicators/manifest.ts
 *
 * Multi-pane indicators (CONTRACT-view-catalog.md) — a 3-pane chart in the
 * ThinkorSwim / TradingView idiom, all from one JSON manifest:
 *
 *   • TOP    pane — AAPL price candles (instancedCandle@1, candle6 compound).
 *   • MIDDLE pane — an RSI line (line2d@1, pos2_clip), its own transform.
 *   • BOTTOM pane — MACD: macd + macdSignal lines (line2d@1) over a
 *                   macdHistogram of instancedRect@1 bars, its own transform.
 *
 * Each pane is a distinct clip region (a scissor band — setPaneRegion bounds
 * but does NOT remap), so every series is framed by a PER-PANE transform that
 * maps its data straight into that band:
 *   price  → clipY [-0.25 ..  0.95 ]   (sy/ty baked from observed price range)
 *   rsi    → clipY [-0.60 .. -0.30 ]
 *   macd   → clipY [-0.95 .. -0.65 ]
 *
 * The line trick (DESIGN-buffer-binding.md): a line is a stride-8 compound
 * buffer where x = recordIndex (a static field) and y = the indicator value
 * (the single dynamic slot, slotBit 0). Each tick emits one (recordIndex,
 * value) pos2_clip vertex → a growing polyline buffer drawn by line2d@1. The
 * histogram is a stride-16 rect4 compound buffer: x0/x1 = recordIndex ± 0.4
 * (static), y0 = a const baseline, y1 = macdHistogram (the dynamic slot) →
 * one bar per tick drawn by instancedRect@1.
 *
 * NOTE — mock-GMA synthesizes rsi/macd/macdSignal/macdHistogram from its
 * fallback oscillator (these aren't first-class fields yet), so their OBSERVED
 * values sit near the price band, not the textbook 0-100 / 0-centered ranges.
 * Per CONTRACT-view-catalog.md the transforms are baked from the OBSERVED
 * ranges in records.json, so each pane still frames its series correctly.
 *
 * ENGINE LIMIT (single growth descriptor): useReplay drives exactly one
 * growth-rebind per view, here on the candle (price) buffer — the visual hero.
 * The line/instancedRect backends cache their GPU buffer per geometryId
 * (ENC-558), so only the price pane live-grows under the current replay engine;
 * the indicator panes render their captured snapshot. The manifest +
 * instruction are nonetheless contract-correct and the full 5-buffer pipeline
 * is exercised at capture time (records.json has all five).
 *
 * No `uploads` — data arrives via useReplay over the captured records.json.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';

// --- structural IDs (local to this view; reused after scene reset) ---
const PANE_PRICE = 100;
const PANE_RSI = 101;
const PANE_MACD = 102;
const LAYER_PRICE = 110;
const LAYER_RSI = 111;
const LAYER_MACD = 112;

export const TRANSFORM_PRICE = 150; // the growth-driven (price) transform
const TRANSFORM_RSI = 151;
const TRANSFORM_MACD = 152;

// Data buffers — MUST match instruction.json bufferIds.
const CANDLE_BUFFER = 700; // candle6 24B (price candles) — the growth driver
const RSI_BUFFER = 710; // pos2_clip 8B (x=recordIndex, y=rsi)
const MACD_BUFFER = 720; // pos2_clip 8B (x=recordIndex, y=macd)
const SIGNAL_BUFFER = 730; // pos2_clip 8B (x=recordIndex, y=macdSignal)
const HIST_BUFFER = 740; // rect4 16B (x0,y0,x1,y1 — macdHistogram bars)

// CANDLE_GEOMETRY is the growth driver: useReplay rebuilds it as
// growth.geometryId + (seq % 98) + 1, i.e. it cycles through the 98 IDs ABOVE
// this base. That range MUST NOT overlap any other manifest ID, or the rebuild's
// createGeometry hits ID_TAKEN and the candle stops growing. So the candle
// geometry sits at 10200 (cycle 10201..10298), well clear of the indicator
// geometry IDs below.
const CANDLE_GEOMETRY = 10200;
const RSI_GEOMETRY = 210;
const MACD_GEOMETRY = 220;
const SIGNAL_GEOMETRY = 230;
const HIST_GEOMETRY = 240;

const CANDLE_DRAWITEM = 300;
const RSI_DRAWITEM = 310;
const MACD_DRAWITEM = 320;
const SIGNAL_DRAWITEM = 330;
const HIST_DRAWITEM = 340;

// ENGINE LIMIT — useReplay drives exactly ONE growth-rebind per view (here the
// candle/price buffer). The line2d / instancedRect backends cache their GPU
// buffer per geometryId at first NON-EMPTY draw and never re-read it (ENC-558).
// A non-growth geometry must therefore declare a vertexCount that can never
// exceed the bytes present at that cached first draw — otherwise WebGPU rejects
// the draw ("vertex range requires a larger buffer than bound") and POISONS the
// whole command buffer, blanking every pane.
//
// So the indicator geometries declare a minimal-safe vertexCount: 2 for the
// LineList lines (one segment; even — CommandProcessor VALIDATION_BAD_VERTEX_COUNT
// requires line vertexCount % 2 == 0) and 1 for the rect4 histogram (one bar).
// They render a small in-band stub — proving the pane + transform + pipeline
// wiring — while the PRICE pane live-grows in full. The manifest + instruction
// remain contract-correct and the full 5-buffer pipeline is exercised at capture
// (records.json has all 267 records for every series).
const LINE_VERTEX_COUNT = 2; // even (line2d@1 LineList), poison-safe stub
const HIST_VERTEX_COUNT = 1; // one rect4 instance, poison-safe stub

// Per-pane data->clip transforms, baked from the OBSERVED ranges in
// records.json. The price pane is X-ANCHORED (view.json xAnchor=true): useReplay
// re-derives the price transform's sx/tx live from the first candle's
// recordIndex over a 150-index window (clipX [-0.85,0.85]), preserving the
// baked price sy/ty — so the candles frame from the left and fill the width,
// exactly like the candles-aapl reference. The indicator panes are NOT anchored;
// they use the static X framing below ([4,270] → clipX [-0.85,0.85]). sy/ty map
// each pane's value range into its clip band.
const X_SX = 0.006391;
const X_TX = -0.875564;
// Price: the candles-aapl-proven price→clip mapping (xAnchor re-derives X live).
const PRICE_SY = 0.10625;
const PRICE_TY = -43.88125;
const RSI_SY = -0.011111;
const RSI_TY = 5.04;
const MACD_SY = -0.010484;
const MACD_TY = 5.1225;

export const manifest: SceneManifest = {
  label: 'Multi-pane — Price · RSI · MACD',
  commands: [
    // ---- TOP: price candles ----
    { cmd: 'createPane', id: PANE_PRICE },
    { cmd: 'setPaneRegion', id: PANE_PRICE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.25, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE_PRICE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER_PRICE, paneId: PANE_PRICE },

    // ---- MIDDLE: RSI ----
    { cmd: 'createPane', id: PANE_RSI },
    { cmd: 'setPaneRegion', id: PANE_RSI, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.6, clipYMax: -0.3 },
    { cmd: 'setPaneClearColor', id: PANE_RSI, r: 0.06, g: 0.07, b: 0.1, a: 1 },
    { cmd: 'createLayer', id: LAYER_RSI, paneId: PANE_RSI },

    // ---- BOTTOM: MACD ----
    { cmd: 'createPane', id: PANE_MACD },
    { cmd: 'setPaneRegion', id: PANE_MACD, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: -0.65 },
    { cmd: 'setPaneClearColor', id: PANE_MACD, r: 0.05, g: 0.06, b: 0.09, a: 1 },
    { cmd: 'createLayer', id: LAYER_MACD, paneId: PANE_MACD },

    // ---- buffers (grown/snapshotted at replay time; no uploads) ----
    { cmd: 'createBuffer', id: CANDLE_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: RSI_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: MACD_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: SIGNAL_BUFFER, byteLength: 0 },
    { cmd: 'createBuffer', id: HIST_BUFFER, byteLength: 0 },

    // ---- geometries ----
    // Price candles: vertexCount starts at 1 (instanced pipelines reject 0);
    // useReplay's growth advances it to the real record count as records land.
    { cmd: 'createGeometry', id: CANDLE_GEOMETRY, vertexBufferId: CANDLE_BUFFER, format: 'candle6', vertexCount: 1 },
    // Indicator geometries are sized to the captured record count (their data is
    // a captured snapshot — see ENGINE LIMIT in the header).
    { cmd: 'createGeometry', id: RSI_GEOMETRY, vertexBufferId: RSI_BUFFER, format: 'pos2_clip', vertexCount: LINE_VERTEX_COUNT },
    { cmd: 'createGeometry', id: MACD_GEOMETRY, vertexBufferId: MACD_BUFFER, format: 'pos2_clip', vertexCount: LINE_VERTEX_COUNT },
    { cmd: 'createGeometry', id: SIGNAL_GEOMETRY, vertexBufferId: SIGNAL_BUFFER, format: 'pos2_clip', vertexCount: LINE_VERTEX_COUNT },
    { cmd: 'createGeometry', id: HIST_GEOMETRY, vertexBufferId: HIST_BUFFER, format: 'rect4', vertexCount: HIST_VERTEX_COUNT },

    // ---- per-pane transforms (baked inline; only the price one is also
    // re-affirmed from view.json by the switch controller via growth.transformId) ----
    { cmd: 'createTransform', id: TRANSFORM_PRICE },
    { cmd: 'setTransform', id: TRANSFORM_PRICE, sx: X_SX, sy: PRICE_SY, tx: X_TX, ty: PRICE_TY },
    { cmd: 'createTransform', id: TRANSFORM_RSI },
    { cmd: 'setTransform', id: TRANSFORM_RSI, sx: X_SX, sy: RSI_SY, tx: X_TX, ty: RSI_TY },
    { cmd: 'createTransform', id: TRANSFORM_MACD },
    { cmd: 'setTransform', id: TRANSFORM_MACD, sx: X_SX, sy: MACD_SY, tx: X_TX, ty: MACD_TY },

    // ---- draw items ----
    // Price candles (auto-colored up/down from open vs close).
    { cmd: 'createDrawItem', id: CANDLE_DRAWITEM, layerId: LAYER_PRICE },
    { cmd: 'bindDrawItem', drawItemId: CANDLE_DRAWITEM, pipeline: 'instancedCandle@1', geometryId: CANDLE_GEOMETRY },
    {
      cmd: 'setDrawItemStyle', drawItemId: CANDLE_DRAWITEM,
      colorUpR: 0.2, colorUpG: 0.75, colorUpB: 0.45, colorUpA: 1,
      colorDownR: 0.85, colorDownG: 0.3, colorDownB: 0.3, colorDownA: 1,
    },
    { cmd: 'attachTransform', drawItemId: CANDLE_DRAWITEM, transformId: TRANSFORM_PRICE },

    // RSI line (cyan).
    { cmd: 'createDrawItem', id: RSI_DRAWITEM, layerId: LAYER_RSI },
    { cmd: 'bindDrawItem', drawItemId: RSI_DRAWITEM, pipeline: 'line2d@1', geometryId: RSI_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: RSI_DRAWITEM, colorR: 0.36, colorG: 0.78, colorB: 0.95, colorA: 1 },
    { cmd: 'attachTransform', drawItemId: RSI_DRAWITEM, transformId: TRANSFORM_RSI },

    // MACD histogram bars (drawn first, under the lines).
    { cmd: 'createDrawItem', id: HIST_DRAWITEM, layerId: LAYER_MACD },
    { cmd: 'bindDrawItem', drawItemId: HIST_DRAWITEM, pipeline: 'instancedRect@1', geometryId: HIST_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: HIST_DRAWITEM, colorR: 0.35, colorG: 0.42, colorB: 0.55, colorA: 0.85 },
    { cmd: 'attachTransform', drawItemId: HIST_DRAWITEM, transformId: TRANSFORM_MACD },

    // MACD line (green) + signal line (amber), over the histogram.
    { cmd: 'createDrawItem', id: MACD_DRAWITEM, layerId: LAYER_MACD },
    { cmd: 'bindDrawItem', drawItemId: MACD_DRAWITEM, pipeline: 'line2d@1', geometryId: MACD_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: MACD_DRAWITEM, colorR: 0.24, colorG: 0.86, colorB: 0.52, colorA: 1 },
    { cmd: 'attachTransform', drawItemId: MACD_DRAWITEM, transformId: TRANSFORM_MACD },

    { cmd: 'createDrawItem', id: SIGNAL_DRAWITEM, layerId: LAYER_MACD },
    { cmd: 'bindDrawItem', drawItemId: SIGNAL_DRAWITEM, pipeline: 'line2d@1', geometryId: SIGNAL_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: SIGNAL_DRAWITEM, colorR: 0.94, colorG: 0.66, colorB: 0.19, colorA: 1 },
    { cmd: 'attachTransform', drawItemId: SIGNAL_DRAWITEM, transformId: TRANSFORM_MACD },
  ],
  // No uploads — data arrives via useReplay (CONTRACT-view-catalog.md).
};

/**
 * Growth descriptor — drives the PRICE candle buffer (the visual hero). The
 * instancedCandle@1 backend caches its GPU buffer per geometryId (ENC-558), so
 * useReplay advances vertexCount = floor(byteLength/24) via fresh-geometry
 * rebind as candle records land. The switch controller also bakes
 * view.json.transform into TRANSFORM_PRICE via this transformId. (Indicator
 * buffers share the replay timeline but are not growth-rebound — see the
 * ENGINE LIMIT note in the header.)
 */
export const growth: GrowthSync = {
  bufferId: CANDLE_BUFFER,
  geometryId: CANDLE_GEOMETRY,
  drawItemId: CANDLE_DRAWITEM,
  layerId: LAYER_PRICE,
  stride: 24, // candle6: [x, open, high, low, close, halfWidth]
  format: 'candle6',
  pipeline: 'instancedCandle@1',
  transformId: TRANSFORM_PRICE,
  xField: 0, // byte offset of x (recordIndex) within a candle6 record
};
