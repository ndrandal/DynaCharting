/* apps/showcase/views/depth-ladder/manifest.ts
 *
 * COMPOSED view — "Bookmap / DOM" order-book depth ladder, rendered by the
 * SCALAR-FAN / fixed-mode technique (Linear ENC-535 / T4.3, the signature
 * vector-rendering capability).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * An order book is a *wide current-state vector*: ~20 price levels per side,
 * each with a live size that is overwritten every tick (it does not grow over
 * time like a candle series). The append/compound write modes can't express
 * this (compound caps at 8 join slots), so the depth ladder is driven by the
 * THIRD embassy write mode: **fixed** (DESIGN-buffer-binding.md). Each level is
 * its OWN subscription bound `{mode:"fixed", bufferId, offset}` that writes its
 * value via UPDATE_RANGE (op 2) at a binding-declared byte offset into a
 * PRE-SIZED buffer. The geometry reads the whole buffer as a live snapshot,
 * overwritten in place each frame. 20 subs per side = the "scalar-fan".
 *
 * ── Geometry layout (instancedRect@1 / rect4) ─────────────────────────────
 * Each level is a horizontal bar = one rect4 instance (x0,y0,x1,y1 = 16B). The
 * bar's STATIC corners (the baseline x0, the band y0/y1) are baked once via the
 * manifest `uploads` (a one-time UPDATE_RANGE of the rect corners). The DYNAMIC
 * size drives the far x-corner (x1) at byte offset k*16+8 — that is the single
 * float each fixed binding overwrites per tick. Bid and ask share the identical
 * rightward layout; the bid draw item carries a mirrored (negative-sx)
 * transform so its bars grow LEFT of the mid, ask bars grow RIGHT — the classic
 * Bookmap/DOM split-ladder. Colors are per-draw-item (instancedRect uses one
 * uniform color): bid = green, ask = red.
 *
 * No `growth` export: fixed-mode buffers do NOT grow (vertexCount is fixed at
 * LEVELS), so the replay engine just streams the UPDATE_RANGE frames in place.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

// --- structural IDs (local to this view; engine is scene-reset between views) ---
const PANE = 100;
const LAYER = 101;
const BID_TRANSFORM = 150; // mirrored (negative sx/sy) → bars grow left+down
const ASK_TRANSFORM = 151; // bars grow right+up

const BID_BUFFER = 601; // rect4 fixed snapshot, 20 levels × 16B
const ASK_BUFFER = 602;
const BID_GEOMETRY = 201;
const ASK_GEOMETRY = 202;
const BID_DRAWITEM = 301;
const ASK_DRAWITEM = 302;

const LEVELS = 20;
const RECORD_BYTES = 16; // rect4: x0,y0,x1,y1 (4×f32)

// Data-space framing constants. The mock GMA synthesizes the depth.*.size
// fields as a deterministic oscillator around the symbol mid; at capture
// (--seed 1) that mid walks through ~[400,425], and all 20 levels share a
// near-identical value each tick (the per-field phase varies only with the
// field-name length), so the ladder "breathes" together over the 20s replay —
// the live-snapshot proof. We bake the bar baseline just below the value floor
// so every bar has positive, on-screen length and the per-tick swing is
// amplified across the clip width.
const BASE_X = 398; // baseline x0 (≈ value floor → clip center after transform)

/**
 * Build the static rect corners for one side as a single UPDATE_RANGE upload.
 * Per level k: x0=BASE_X (baseline), y0/y1 = the level's vertical band, and
 * x1 seeded to BASE_X (zero-length until the first fixed tick overwrites it).
 * Levels stack away from the mid: k=0 nearest the center, k=19 outermost.
 */
function staticCorners(bufferId: number): BufferUpload {
  const floats: number[] = [];
  for (let k = 0; k < LEVELS; k++) {
    const y0 = k + 1 + 0.12; // small gap between bars
    const y1 = k + 1 + 0.88;
    floats.push(BASE_X, y0, BASE_X, y1); // x0, y0, x1(seed=base), y1
  }
  return { bufferId, op: 'updateRange', offsetBytes: 0, floats };
}

export const manifest: SceneManifest = {
  label: 'Depth Ladder — order book (Bookmap/DOM)',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.95, clipXMax: 0.95, clipYMin: -0.95, clipYMax: 0.95 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Pre-sized fixed-mode buffers (byteLength = LEVELS × recordBytes). The
    // UPDATE_RANGE offsets the fixed bindings write into MUST be valid here.
    { cmd: 'createBuffer', id: BID_BUFFER, byteLength: LEVELS * RECORD_BYTES },
    { cmd: 'createBuffer', id: ASK_BUFFER, byteLength: LEVELS * RECORD_BYTES },

    { cmd: 'createGeometry', id: BID_GEOMETRY, vertexBufferId: BID_BUFFER, format: 'rect4', vertexCount: LEVELS },
    { cmd: 'createGeometry', id: ASK_GEOMETRY, vertexBufferId: ASK_BUFFER, format: 'rect4', vertexCount: LEVELS },

    // Transforms: ask grows RIGHT and stacks ABOVE the mid, bid grows LEFT and
    // stacks BELOW the mid (the classic Bookmap/DOM split). clip_x = sx*x + tx,
    // clip_y = sy*y + ty (recomputeMat3, Types.hpp); the instancedRect shader
    // then negates clip_y for the framebuffer, so a NEGATIVE sy puts ascending
    // levels toward the TOP of the screen and a POSITIVE sy toward the BOTTOM.
    //   x: BASE_X(398)→0, ~426→~0.90  ⇒ |sx| ≈ 0.0321, |tx| ≈ 12.78
    //   y: level 1→±0.05, level 20→±0.90 ⇒ |sy| ≈ 0.0438, ty = 0
    // Ask: sx>0 (grow right), sy<0 (above). Bid: sx<0 (grow left), sy>0 (below).
    { cmd: 'createTransform', id: ASK_TRANSFORM },
    { cmd: 'setTransform', id: ASK_TRANSFORM, sx: 0.0321, sy: -0.0438, tx: -12.78, ty: 0 },
    { cmd: 'createTransform', id: BID_TRANSFORM },
    { cmd: 'setTransform', id: BID_TRANSFORM, sx: -0.0321, sy: 0.0438, tx: 12.78, ty: 0 },

    // Ask draw item (red, above mid).
    { cmd: 'createDrawItem', id: ASK_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: ASK_DRAWITEM, pipeline: 'instancedRect@1', geometryId: ASK_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: ASK_DRAWITEM, r: 0.85, g: 0.30, b: 0.30, a: 0.92 },
    { cmd: 'attachTransform', drawItemId: ASK_DRAWITEM, transformId: ASK_TRANSFORM },

    // Bid draw item (green, below mid).
    { cmd: 'createDrawItem', id: BID_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: BID_DRAWITEM, pipeline: 'instancedRect@1', geometryId: BID_GEOMETRY },
    { cmd: 'setDrawItemStyle', drawItemId: BID_DRAWITEM, r: 0.20, g: 0.75, b: 0.45, a: 0.92 },
    { cmd: 'attachTransform', drawItemId: BID_DRAWITEM, transformId: BID_TRANSFORM },
  ],
  // Static rect corners baked once. The fixed-mode UPDATE_RANGE frames (replayed
  // from records.json) then overwrite only the x1 corner (offset k*16+8) of each
  // level every tick — the live depth snapshot.
  uploads: [staticCorners(BID_BUFFER), staticCorners(ASK_BUFFER)],
};
