/* apps/showcase/src/scene/candleManifest.ts
 *
 * The candle view SceneManifest (ENC-520, T0.5 vertical slice): ONE live
 * candlestick chart driven entirely by streamed data from embassy over the WS
 * data plane. The showcase owns this manifest (per CONTRACT-buffer-id.md);
 * embassy's own scene-init is ignored — the only thing both sides agree on is
 * buffer 10100 (candle6, 24B records: [x, open, high, low, close, halfWidth]).
 *
 * There are NO uploads here: the candle buffer (10100) is grown record-by-record
 * by embassy's compound OHLC join, arriving as APPEND records on the WS that
 * useAgentStream feeds straight into host.enqueueData.
 *
 * The transform (10050) maps data space (x = recordIndex; y = AAPL price) into
 * clip space. The constants below were tuned against the live mock-gma feed
 * (seed 1): the AAPL synthetic walk sits around ~$408–418; embassy's compound
 * OHLC join advances recordIndex ~13/sec (75ms cadence). A late-connecting
 * browser sees a contiguous run of records starting wherever recordIndex is at
 * connect time — so embassy is (re)started right before the browser connects to
 * keep the visible x-window near the values baked here. sx/tx map
 * x∈[X_MIN,X_MAX]→[-0.85,0.85]; sy/ty map a padded price range →[-0.85,0.85].
 * As more records stream the chart drifts right — acceptable for the
 * static-transform slice (live auto-ranging is a later phase). The proof is:
 * green/red candlesticks rendered from live streamed data.
 */

import type { SceneManifest } from './commands';

// --- structural IDs (shared with the instruction only via the buffer) ---
const PANE = 10000;
const LAYER = 10001;
const TRANSFORM = 10050;
const CANDLE_BUFFER = 10100; // candle6 24B — MUST match instruction bufferId
const CANDLE_GEOMETRY = 10200;
const CANDLE_DRAWITEM = 10300;

// --- tuned transform (see header; baked from a live-feed probe) ---
// y = price: a PADDED envelope around the observed live AAPL range (~408–418),
// baked here. sy/ty map price∈[Y_MIN,Y_MAX] → clipY[-0.85,0.85].
const Y_MIN = 405;
const Y_MAX = 421;
const SY = 1.7 / (Y_MAX - Y_MIN);
const TY = -0.85 - SY * Y_MIN;

// x = recordIndex, which embassy advances unboundedly (~13/sec) and which is
// already large by the time a browser connects. Rather than guess the absolute
// value, the transform's X part is set LIVE by useAgentStream: it reads the
// first streamed record's x and maps [firstX, firstX + X_WINDOW] → clipX
// [-0.85,0.85], so candles always frame from the left regardless of connect
// time. X_WINDOW ≈ the number of candles collected over the render window.
const X_WINDOW = 150;
// Initial (pre-data) X mapping; overwritten on the first record. Identity-ish so
// the lone bootstrap candle isn't wildly off-screen before the live anchor.
const SX0 = 1.7 / X_WINDOW;
const TX0 = -0.85;

export const CANDLE_MANIFEST: SceneManifest = {
  label: 'AAPL Candles (live)',
  commands: [
    // Pane + layer.
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Candle buffer (grown live by embassy) -> candle6 geometry -> drawItem.
    { cmd: 'createBuffer', id: CANDLE_BUFFER, byteLength: 0 },
    // vertexCount starts at 1 (the renderer draws geometry.vertexCount
    // instances; bindDrawItem rejects 0 for instanced pipelines). The live
    // data plane grows the buffer; useAgentStream syncs vertexCount up to the
    // real candle6 record count (bytes/24) as records arrive (CANDLE_GROWTH).
    {
      cmd: 'createGeometry',
      id: CANDLE_GEOMETRY,
      vertexBufferId: CANDLE_BUFFER,
      format: 'candle6',
      vertexCount: 1,
    },

    // Data->clip transform. Y is baked (price envelope); X is anchored live to
    // the first streamed recordIndex by useAgentStream (see CANDLE_GROWTH).
    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX0, sy: SY, tx: TX0, ty: TY },

    { cmd: 'createDrawItem', id: CANDLE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: CANDLE_DRAWITEM, pipeline: 'instancedCandle@1', geometryId: CANDLE_GEOMETRY },
    // instancedCandle@1 auto-colors from candle6's open vs close. colorUp*/
    // colorDown* are scalar fields on setDrawItemStyle (CommandProcessor.cpp).
    {
      cmd: 'setDrawItemStyle',
      drawItemId: CANDLE_DRAWITEM,
      colorUpR: 0.2, colorUpG: 0.75, colorUpB: 0.45, colorUpA: 1,
      colorDownR: 0.85, colorDownG: 0.3, colorDownB: 0.3, colorDownA: 1,
    },
    { cmd: 'attachTransform', drawItemId: CANDLE_DRAWITEM, transformId: TRANSFORM },
  ],
  uploads: [],
};

/**
 * Live-growth descriptor for the candle buffer. The renderer draws
 * geometry.vertexCount instances; the WS data plane only grows the buffer's
 * bytes, so the browser must advance vertexCount = floor(byteLength / stride)
 * as records arrive. useAgentStream(host, CANDLE_GROWTH) does this each batch.
 */
export const CANDLE_GROWTH = {
  bufferId: CANDLE_BUFFER,
  geometryId: CANDLE_GEOMETRY,
  drawItemId: CANDLE_DRAWITEM,
  layerId: LAYER,
  stride: 24, // candle6: [x, open, high, low, close, halfWidth] = 6×f32
  format: 'candle6' as const,
  pipeline: 'instancedCandle@1' as const,
  // Live X-anchoring: map [firstX, firstX + window] → clipX[clipMin,clipMax].
  transformId: TRANSFORM,
  xField: 0, // byte offset of x (recordIndex) within a candle6 record
  xWindow: X_WINDOW,
  clipMin: -0.85,
  clipMax: 0.85,
  // Baked Y mapping (price), preserved across the live setTransform.
  sy: SY,
  ty: TY,
} as const;
