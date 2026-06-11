/* apps/showcase/views/renko/manifest.ts
 *
 * COMPOSED — "Renko / Point & Figure" fixed-brick chart, a STATIC
 * COMPUTED-GEOMETRY view (Linear ENC-536 / T4.4). Unlike the streaming catalog
 * views, the chart shape here is TESSELLATED AT MANIFEST-BUILD TIME from a
 * dataset and embedded as static manifest `uploads`; there is no capture,
 * no replay, no embassy — records.json is an empty stub.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A Renko chart discards time: it advances one column only when price moves a
 * fixed BRICK SIZE, drawing a green up-brick or a red down-brick per step. We
 * compute the brick sequence from AAPL's close series at module-eval time:
 * brickSize = priceRange / 14; walk the closes, emitting a brick each time the
 * running level crosses a brick boundary. Each brick is one axis-aligned rect.
 *
 * ── Geometry (instancedRect@1 / rect4) ────────────────────────────────────
 * Each brick is one rect4 instance: x0,y0,x1,y1 in DATA space (column index ×
 * price). The whole brick wall is baked once into a pre-sized rect4 buffer via
 * the manifest `uploads` (a single UPDATE_RANGE). instancedRect draws every
 * instance through ONE uniform color, so up- and down-bricks are split across
 * TWO draw items (green / red), each reading its own static buffer. A single
 * data→clip transform per item bakes the column/price framing into clip space.
 *
 * No `growth`, no streaming: the geometry is fully resolved at build time.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';
import aapl from '../../data/market/AAPL.json';

// --- structural IDs (local to this view; engine is scene-reset between views) ---
const PANE = 100;
const LAYER = 101;
const TRANSFORM = 150;

const UP_BUFFER = 601; // rect4 static — up-bricks (green)
const DOWN_BUFFER = 602; // rect4 static — down-bricks (red)
const UP_GEOMETRY = 201;
const DOWN_GEOMETRY = 202;
const UP_DRAWITEM = 301;
const DOWN_DRAWITEM = 302;

const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1 (4×f32)

// --- build-time tessellation: closes → fixed-brick rects -------------------
interface Brick {
  col: number; // column index along the time-less x axis
  lo: number; // brick bottom price
  hi: number; // brick top price
  up: boolean; // up-brick (close rising) vs down-brick
}

function closes(): number[] {
  return (aapl.updates as { field: string; value: number }[])
    .filter((u) => u.field === 'close')
    .map((u) => u.value);
}

/**
 * Walk the close series, emitting one fixed-size brick per boundary crossing.
 * The running `level` tracks the price floor/ceiling of the last brick; each
 * full brickSize move appends a brick and advances the column. (A single-brick
 * threshold — not the 2× reversal rule — keeps every move visible.)
 */
function buildBricks(): { bricks: Brick[]; brickSize: number; cols: number; lo: number; hi: number } {
  const c = closes();
  const lo = Math.min(...c);
  const hi = Math.max(...c);
  const brickSize = (hi - lo) / 14;
  const bricks: Brick[] = [];
  let level = c[0];
  let col = 0;
  for (const price of c) {
    while (price - level >= brickSize) {
      bricks.push({ col, lo: level, hi: level + brickSize, up: true });
      level += brickSize;
      col++;
    }
    while (level - price >= brickSize) {
      bricks.push({ col, lo: level - brickSize, hi: level, up: false });
      level -= brickSize;
      col++;
    }
  }
  return { bricks, brickSize, cols: col, lo, hi };
}

const { bricks, brickSize, cols, lo, hi } = buildBricks();
const UP_BRICKS = bricks.filter((b) => b.up);
const DOWN_BRICKS = bricks.filter((b) => !b.up);

/** Pack one side's bricks as rect4 floats (x0,y0,x1,y1) with a small inset gap. */
function rectsFor(side: Brick[], bufferId: number): BufferUpload {
  const floats: number[] = [];
  const gap = 0.08; // fraction of a column/brick to leave as a gutter
  for (const b of side) {
    const x0 = b.col + gap;
    const x1 = b.col + 1 - gap;
    floats.push(x0, b.lo + brickSize * gap, x1, b.hi - brickSize * gap);
  }
  return { bufferId, op: 'updateRange', offsetBytes: 0, floats };
}

// data→clip transform: x in [0,cols] → [-0.9,0.9]; price [lo,hi] → [-0.85,0.85].
// clip_x = sx*x + tx ; clip_y = sy*y + ty (recomputeMat3, Types.hpp).
const SX = 1.8 / cols;
const TX = -0.9;
const SY = 1.7 / (hi - lo);
const TY = -0.85 - SY * lo;

export const manifest: SceneManifest = {
  label: 'Renko — fixed-brick (AAPL close)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Pre-sized static rect4 buffers (one per direction).
    { cmd: 'createBuffer', id: UP_BUFFER, byteLength: UP_BRICKS.length * RECORD_BYTES },
    { cmd: 'createBuffer', id: DOWN_BUFFER, byteLength: DOWN_BRICKS.length * RECORD_BYTES },
    { cmd: 'createGeometry', id: UP_GEOMETRY, vertexBufferId: UP_BUFFER, format: 'rect4', vertexCount: UP_BRICKS.length },
    { cmd: 'createGeometry', id: DOWN_GEOMETRY, vertexBufferId: DOWN_BUFFER, format: 'rect4', vertexCount: DOWN_BRICKS.length },

    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    // Up-bricks (green).
    { cmd: 'createDrawItem', id: UP_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: UP_DRAWITEM, pipeline: 'instancedRect@1', geometryId: UP_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: UP_DRAWITEM, r: 0.20, g: 0.75, b: 0.45, a: 0.95 },
    { cmd: 'attachTransform', drawItemId: UP_DRAWITEM, transformId: TRANSFORM },

    // Down-bricks (red).
    { cmd: 'createDrawItem', id: DOWN_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: DOWN_DRAWITEM, pipeline: 'instancedRect@1', geometryId: DOWN_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: DOWN_DRAWITEM, r: 0.85, g: 0.30, b: 0.30, a: 0.95 },
    { cmd: 'attachTransform', drawItemId: DOWN_DRAWITEM, transformId: TRANSFORM },
  ],
  // The whole brick wall, tessellated at build time and baked as static rects.
  uploads: [rectsFor(UP_BRICKS, UP_BUFFER), rectsFor(DOWN_BRICKS, DOWN_BUFFER)],
};
