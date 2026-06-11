/* apps/showcase/views/footprint/manifest.ts
 *
 * COMPOSED / HARD view — "order-flow footprint" (bid×ask volume per price×time
 * cell), rendered by the SCALAR-FAN / fixed-mode technique (Linear ENC-535 /
 * T4.3). This is the frontier case: see the "FRONTIER LIMIT" note below and
 * explainer.md — a true 2D footprint GRID with per-cell magnitude is NOT
 * expressible under the current model; this ships the closest faithful
 * approximation and documents the wall.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A footprint chart is a grid of cells over (price × time); each cell shows the
 * bid and ask volume traded at that price in that time bucket. We drive it with
 * a 160-way scalar-fan: each cell's bid and ask volume is its own
 * `footprint.{bid,ask}.r.c` subscription bound `{mode:"fixed", offset}`, writing
 * via UPDATE_RANGE (op 2) into one of two pre-sized rect4 buffers (bid / ask)
 * read as live snapshots — the same fixed-mode mechanic as the depth ladder,
 * scaled to a 10×8 cell grid per side.
 *
 * ── FRONTIER LIMIT (the wall this view maps) ──────────────────────────────
 * instancedRect draws every instance through ONE shared per-draw-item transform
 * and ONE uniform color, and a fixed binding writes ONE raw float (the absolute
 * value, ~410 at capture) to ONE rect corner. With a single global linear
 * transform, a value-driven corner lands at clip(sx·V+tx) — the SAME clip
 * position for every cell, independent of the cell's baked baseline. So the only
 * per-cell quantity that survives is the bar LENGTH (value − bakedBaseline,
 * anchored at the baked baseline); the value-driven edge cannot be tiled along
 * its own growth axis. ⇒ A 2D grid with per-cell magnitude is unreachable: you
 * can tile cells along the axis PERPENDICULAR to growth, but not along it.
 * Therefore this view FLATTENS the 10×8 grid into 80 stacked horizontal volume
 * bars per side (price·time ordered, level = row·COLS + col), bid (green) left
 * of the mid, ask (red) right — an honest "footprint as a stacked bid/ask volume
 * profile". The true 2D-grid footprint with independent per-cell heat is the
 * documented frontier: it needs per-instance color + per-instance offset (custom
 * WGSL / a packed multi-field instance format), the exact "live-GPU" wall the
 * frontier map names.
 *
 * No `growth`: fixed buffers do not grow; the replay just streams UPDATE_RANGE.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const BID_TRANSFORM = 150; // mirrored → bars grow left of the mid
const ASK_TRANSFORM = 151; // bars grow right of the mid

const BID_BUFFER = 601; // rect4 fixed snapshot, ROWS*COLS cells × 16B
const ASK_BUFFER = 602;
const BID_GEOMETRY = 201;
const ASK_GEOMETRY = 202;
const BID_DRAWITEM = 301;
const ASK_DRAWITEM = 302;

const ROWS = 10; // price rows
const COLS = 8; // time columns
const CELLS = ROWS * COLS; // 80 levels per side
const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1

// At capture (--seed 1) the synthetic footprint.* fields oscillate through
// ~[400,425]; baseline just below the floor so every bar has positive length.
const BASE_X = 398;
const SPAN_Y = 1.84; // total clip height the 80 levels fill (clip [-0.92, 0.92])

/**
 * Static rect corners (one UPDATE_RANGE upload). Each cell (r,c) is one stacked
 * horizontal bar at level = r·COLS + c; x0 = BASE_X baseline (bar length =
 * liveValue − BASE_X), y0/y1 = the level's thin band. A small inter-row gap
 * (every COLS bars) groups the time-columns of one price row, so the price·time
 * ordering stays legible in the flattened stack.
 */
function staticCorners(bufferId: number): BufferUpload {
  const floats: number[] = [];
  for (let r = 0; r < ROWS; r++) {
    for (let c = 0; c < COLS; c++) {
      const level = r * COLS + c;
      const y0 = level + 0.1;
      const y1 = level + 0.9;
      floats.push(BASE_X, y0, BASE_X, y1); // x0, y0, x1(seed), y1
    }
  }
  return { bufferId, op: 'updateRange', offsetBytes: 0, floats };
}

export const manifest: SceneManifest = {
  label: 'Footprint — bid×ask volume (order-flow)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    { cmd: 'createBuffer', id: BID_BUFFER, byteLength: CELLS * RECORD_BYTES },
    { cmd: 'createBuffer', id: ASK_BUFFER, byteLength: CELLS * RECORD_BYTES },
    { cmd: 'createGeometry', id: BID_GEOMETRY, vertexBufferId: BID_BUFFER, format: 'rect4', vertexCount: CELLS },
    { cmd: 'createGeometry', id: ASK_GEOMETRY, vertexBufferId: ASK_BUFFER, format: 'rect4', vertexCount: CELLS },

    // clip_x: BASE_X(398)→0, ~426→~0.9 ⇒ sx ≈ 0.0321, tx ≈ -12.78
    // clip_y: 80 levels over [-0.92,0.92] ⇒ sy = SPAN_Y/CELLS ≈ 0.023, ty = -0.92
    { cmd: 'createTransform', id: ASK_TRANSFORM },
    { cmd: 'setTransform', id: ASK_TRANSFORM, sx: 0.0321, sy: SPAN_Y / CELLS, tx: -12.78, ty: -0.92 },
    { cmd: 'createTransform', id: BID_TRANSFORM },
    { cmd: 'setTransform', id: BID_TRANSFORM, sx: -0.0321, sy: SPAN_Y / CELLS, tx: 12.78, ty: -0.92 },

    { cmd: 'createDrawItem', id: ASK_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: ASK_DRAWITEM, pipeline: 'instancedRect@1', geometryId: ASK_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: ASK_DRAWITEM, r: 0.85, g: 0.30, b: 0.30, a: 0.9 },
    { cmd: 'attachTransform', drawItemId: ASK_DRAWITEM, transformId: ASK_TRANSFORM },

    { cmd: 'createDrawItem', id: BID_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: BID_DRAWITEM, pipeline: 'instancedRect@1', geometryId: BID_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: BID_DRAWITEM, r: 0.20, g: 0.75, b: 0.45, a: 0.9 },
    { cmd: 'attachTransform', drawItemId: BID_DRAWITEM, transformId: BID_TRANSFORM },
  ],
  // Baked baselines; the fixed-mode UPDATE_RANGE frames overwrite only x1
  // (offset cell·16+8) of each cell every tick → the live bid/ask volume stack.
  uploads: [staticCorners(BID_BUFFER), staticCorners(ASK_BUFFER)],
};
