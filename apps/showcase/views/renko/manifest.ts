/* apps/showcase/views/renko/manifest.ts
 *
 * COMPOSED — "Renko / Point & Figure" fixed-brick chart, now a LIVE / STREAMING
 * view (Linear ENC-572). Previously this was a STATIC computed-geometry view:
 * the whole brick wall was tessellated at manifest-build time and baked into
 * pre-sized rect4 buffers via static `uploads`. It now APPENDS its bricks over a
 * ~20s replay timeline so the wall forms live as the simulated price walks
 * across brick thresholds.
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A Renko chart discards time: it advances one column only when price moves a
 * fixed BRICK SIZE, drawing a brick per step. The brick walk (brickSize =
 * priceRange / 14, closes walked boundary-by-boundary) is reproduced OFFLINE by
 * the generator (.gen.mjs) — but instead of one static upload, each brick is
 * emitted as one rect4 APPEND frame stamped with the timestamp of the AAPL close
 * that produced it (records.json). The replay engine (useReplay) streams those
 * APPEND frames into the data plane on that timeline; ENC-558's instancedRect
 * backend re-reads the growing buffer (driven by the `growth` descriptor below,
 * which advances geometry.vertexCount as records land), so bricks materialise
 * progressively — few early, many by t≈10s, the full wall by ~20s.
 *
 * ── Geometry (instancedRect@1 / rect4) ────────────────────────────────────
 * Each brick is one rect4 instance: x0,y0,x1,y1 in DATA space (column index ×
 * price). The brick buffer starts EMPTY (byteLength 0) and grows one 16B rect4
 * per appended frame; the geometry's instance count derives from the buffer
 * size at replay time. A single baked data→clip transform frames the FULL
 * column range and FULL price range up front, so the first bricks land
 * on-screen and the wall fills left-to-right without re-anchoring.
 *
 * ── Color ─────────────────────────────────────────────────────────────────
 * instancedRect draws every instance through ONE uniform color, and the replay
 * growth descriptor drives a SINGLE growing buffer/geometry/drawItem — so the
 * live wall uses one green accent rather than the old two-buffer green/red split
 * (a two-buffer stream would need two independent growth syncs). The chrome
 * legend still names both directions; the live emphasis is the bricks APPEARING.
 */

import type { SceneManifest } from '../../src/scene/commands';
import type { GrowthSync } from '../../src/engine/useReplay';
import aapl from '../../data/market/AAPL.json';

// --- structural IDs (local to this view; engine is scene-reset between views) ---
const PANE = 100;
const LAYER = 101;
const TRANSFORM = 150;

const BRICK_BUFFER = 601; // rect4 — grows one brick per replayed APPEND frame
const BRICK_GEOMETRY = 201;
const BRICK_DRAWITEM = 301;

const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1 (4×f32)
const BRICK_COUNT = 14; // priceRange / BRICK_COUNT = brickSize (matches .gen.mjs)

// --- build-time framing only: derive the data-space extents so the baked
// transform frames the FULL wall (the bricks themselves arrive via replay). The
// same close walk the generator uses, run here purely to size the transform. ---
function buildExtents(): { cols: number; lo: number; hi: number; brickSize: number } {
  const c = (aapl.updates as { field: string; value: number }[])
    .filter((u) => u.field === 'close')
    .map((u) => u.value);
  const lo = Math.min(...c);
  const hi = Math.max(...c);
  const brickSize = (hi - lo) / BRICK_COUNT;
  let level = c[0];
  let col = 0;
  for (const price of c) {
    while (price - level >= brickSize) {
      level += brickSize;
      col++;
    }
    while (level - price >= brickSize) {
      level -= brickSize;
      col++;
    }
  }
  return { cols: col, lo, hi, brickSize };
}

const { cols, lo, hi } = buildExtents();

// data→clip transform: x in [0,cols] → [-0.9,0.9]; price [lo,hi] → [-0.85,0.85].
// clip_x = sx*x + tx ; clip_y = sy*y + ty (recomputeMat3, Types.hpp). Baked for
// the FULL range so the first replayed bricks are already on-screen.
const SX = 1.8 / cols;
const TX = -0.9;
const SY = 1.7 / (hi - lo);
const TY = -0.85 - SY * lo;

export const manifest: SceneManifest = {
  label: 'Renko — fixed-brick (AAPL close, live)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Brick buffer starts EMPTY and grows one rect4 per replayed APPEND frame.
    { cmd: 'createBuffer', id: BRICK_BUFFER, byteLength: 0 },
    // vertexCount starts at 1 (instanced pipelines reject 0); the replay engine
    // advances it to the real brick count (bytes/16) as frames arrive.
    { cmd: 'createGeometry', id: BRICK_GEOMETRY, vertexBufferId: BRICK_BUFFER, format: 'rect4', vertexCount: 1 },

    { cmd: 'createTransform', id: TRANSFORM },
    { cmd: 'setTransform', id: TRANSFORM, sx: SX, sy: SY, tx: TX, ty: TY },

    // Bricks (green accent — instancedRect carries one uniform color).
    { cmd: 'createDrawItem', id: BRICK_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: BRICK_DRAWITEM, pipeline: 'instancedRect@1', geometryId: BRICK_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: BRICK_DRAWITEM, r: 0.20, g: 0.75, b: 0.45, a: 0.95 },
    { cmd: 'attachTransform', drawItemId: BRICK_DRAWITEM, transformId: TRANSFORM },
  ],
  // No `uploads` — the brick wall arrives live via useReplay over the data plane
  // (records.json, generated by .gen.mjs). CONTRACT-view-catalog.md.
};

/**
 * Growth descriptor for the replay engine. The renderer draws
 * geometry.vertexCount instances; the dataplane only grows the buffer's bytes,
 * so the replay engine advances vertexCount = floor(byteLength / 16) as the
 * rect4 bricks land (via fresh-geometry rebind — ENC-558 backend-cache
 * workaround). No X re-anchor: the transform is baked for the full column range
 * (xField is required by the type but unused here, as view.json sets no xAnchor).
 */
export const growth: GrowthSync = {
  bufferId: BRICK_BUFFER,
  geometryId: BRICK_GEOMETRY,
  drawItemId: BRICK_DRAWITEM,
  layerId: LAYER,
  stride: RECORD_BYTES, // rect4 = 16B
  format: 'rect4',
  pipeline: 'instancedRect@1',
  transformId: TRANSFORM,
  xField: 0, // x0 offset within a rect4 record (unused — no xAnchor)
};
