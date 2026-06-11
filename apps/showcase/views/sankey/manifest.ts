/* apps/showcase/views/sankey/manifest.ts
 *
 * COMPOSED / STATIC view — "fund-flow Sankey" (Linear ENC-537 / T4.5).
 *
 * ── What's going on (the technique) ───────────────────────────────────────
 * A Sankey diagram is a set of weighted FLOW RIBBONS between two columns of
 * nodes. There is no native "ribbon" primitive, so the diagram is COMPUTED at
 * manifest-build time into a triangle list and drawn by the generic colored-
 * triangle pipeline `triGradient@1` (vertex format pos2_color4: x,y,r,g,b,a per
 * vertex). Every ribbon is a quad (= 2 triangles = 6 vertices) whose color is
 * carried per-vertex (constant across the ribbon, tinted by the SOURCE node).
 *
 * The flow matrix is SYNTHESIZED from the market datasets (precomputed by
 * .build-composed-static.mjs): each of the four symbols is a source "sector"
 * whose total traded volume is split into three destination buckets
 * (Inflow / Rotation / Outflow) by its net price drift over the tape. The
 * absolute numbers are not a real capital-flow claim — they are a deterministic,
 * dataset-derived demonstration of the ribbon-tessellation capability.
 *
 * ── Geometry ──────────────────────────────────────────────────────────────
 * Two node columns: 4 source nodes (left), 3 destination nodes (right). A node
 * is a vertical bar whose height ∝ its total flow; ribbons leave/enter a node
 * stacked by sub-flow. Each ribbon is drawn as a STRAIGHT quad from the source
 * node's right edge to the destination node's left edge (see the bezier note).
 * Coordinates are authored directly in CLIP space ([-1,1]); no transform needed.
 *
 * ── Bezier tessellation (APPROXIMATED) ────────────────────────────────────
 * Reference Sankeys curve their ribbons with a horizontal cubic Bézier. Here the
 * ribbons are STRAIGHT quads (a single quad per flow), NOT bezier-tessellated.
 * This is an explicit, documented approximation: a true bezier ribbon would
 * tessellate each flow into ~16 quad segments along a cubic whose control points
 * pull horizontally from the endpoints. The straight form proves the same
 * capability (arbitrary colored-triangle geometry from a flow matrix) with a
 * fraction of the vertices; curving it is purely more tessellation in this same
 * build step. The node bars + straight ribbons read clearly as a Sankey.
 */

import type { SceneManifest, BufferUpload } from '../../src/scene/commands';

const PANE = 100;
const LAYER = 101;
const RIBBON_BUFFER = 601; // pos2_color4 triangle list (ribbons)
const NODE_BUFFER = 602; // pos2_color4 triangle list (node bars)
const RIBBON_GEOMETRY = 201;
const NODE_GEOMETRY = 202;
const RIBBON_DRAWITEM = 301;
const NODE_DRAWITEM = 302;

// ── SYNTHESIZED flow matrix (baked by .build-composed-static.mjs) ───────────
// rows = source sectors (symbols); cols = [Inflow, Rotation, Outflow].
const SOURCES = ['AAPL', 'MSFT', 'NVDA', 'TSLA'];
const DESTS = ['Inflow', 'Rotation', 'Outflow'];
const FLOW: number[][] = [
  [12502.0, 37506.1, 61814.8], // AAPL
  [38941.1, 56800.4, 18933.5], // MSFT
  [19720.4, 59161.1, 36250.5], // NVDA
  [19657.4, 58972.3, 38300.3], // TSLA
];

// Per-source ribbon colors (RGBA), tinted by source. One hue per symbol.
const SRC_COLOR: [number, number, number, number][] = [
  [0.27, 0.62, 0.95, 0.85], // AAPL — blue
  [0.35, 0.80, 0.55, 0.85], // MSFT — green
  [0.95, 0.72, 0.30, 0.85], // NVDA — amber
  [0.85, 0.40, 0.70, 0.85], // TSLA — magenta
];
const NODE_COLOR: [number, number, number, number] = [0.78, 0.82, 0.90, 1.0];

// ── layout (clip space) ─────────────────────────────────────────────────────
const X_SRC_L = -0.86, X_SRC_R = -0.74; // source node bar x-extent
const X_DST_L = 0.74, X_DST_R = 0.86; // destination node bar x-extent
const Y_TOP = 0.82, Y_BOT = -0.82; // usable vertical band
const NODE_GAP = 0.06; // vertical gap between stacked nodes
const RIBBON_GAP = 0.0; // ribbons packed flush within a node

// Total flow (for normalizing node heights to fill the band).
const grandTotal = FLOW.flat().reduce((a, b) => a + b, 0);
// Source totals and destination totals.
const srcTotal = FLOW.map((row) => row.reduce((a, b) => a + b, 0));
const dstTotal = DESTS.map((_, j) => FLOW.reduce((a, row) => a + row[j], 0));

// Map a flow magnitude to a vertical pixel-height in clip units. We allocate the
// full band height minus inter-node gaps proportionally to flow.
const srcBand = (Y_TOP - Y_BOT) - NODE_GAP * (SOURCES.length - 1);
const dstBand = (Y_TOP - Y_BOT) - NODE_GAP * (DESTS.length - 1);
const srcScale = srcBand / grandTotal; // clip units per flow-unit (sources)
const dstScale = dstBand / grandTotal; // clip units per flow-unit (dests)

// Push one quad (4 corners → 2 triangles → 6 vertices) in pos2_color4.
function pushQuad(
  out: number[],
  // corners given as (x0,y0)=top-left, (x1,y1)=top-right, (x2,y2)=bottom-right, (x3,y3)=bottom-left
  x0: number, y0: number, x1: number, y1: number,
  x2: number, y2: number, x3: number, y3: number,
  col: [number, number, number, number],
): void {
  const [r, g, b, a] = col;
  const v = (x: number, y: number) => out.push(x, y, r, g, b, a);
  v(x0, y0); v(x1, y1); v(x2, y2); // tri 1
  v(x0, y0); v(x2, y2); v(x3, y3); // tri 2
}

// ── build node bars ─────────────────────────────────────────────────────────
const nodeFloats: number[] = [];
// source nodes stacked top→bottom on the left
const srcY: { top: number; bot: number }[] = [];
let cursor = Y_TOP;
for (let i = 0; i < SOURCES.length; i++) {
  const h = srcTotal[i] * srcScale;
  const top = cursor, bot = cursor - h;
  srcY.push({ top, bot });
  pushQuad(nodeFloats, X_SRC_L, top, X_SRC_R, top, X_SRC_R, bot, X_SRC_L, bot, NODE_COLOR);
  cursor = bot - NODE_GAP;
}
// destination nodes stacked top→bottom on the right
const dstY: { top: number; bot: number }[] = [];
cursor = Y_TOP;
for (let j = 0; j < DESTS.length; j++) {
  const h = dstTotal[j] * dstScale;
  const top = cursor, bot = cursor - h;
  dstY.push({ top, bot });
  pushQuad(nodeFloats, X_DST_L, top, X_DST_R, top, X_DST_R, bot, X_DST_L, bot, NODE_COLOR);
  cursor = bot - NODE_GAP;
}

// ── build flow ribbons ──────────────────────────────────────────────────────
// Per source we walk its sub-flows top→bottom along the source node's right
// edge; per destination we consume incoming flow top→bottom along its left edge.
const ribbonFloats: number[] = [];
const srcCursor = srcY.map((n) => n.top); // running y at each source's right edge
const dstCursor = dstY.map((n) => n.top); // running y at each dest's left edge
for (let i = 0; i < SOURCES.length; i++) {
  for (let j = 0; j < DESTS.length; j++) {
    const f = FLOW[i][j];
    if (f <= 0) continue;
    const hSrc = f * srcScale;
    const hDst = f * dstScale;
    const sTop = srcCursor[i], sBot = sTop - hSrc;
    const dTop = dstCursor[j], dBot = dTop - hDst;
    // STRAIGHT ribbon quad: source right edge → dest left edge.
    pushQuad(
      ribbonFloats,
      X_SRC_R, sTop, X_DST_L, dTop, // top edge (src→dst)
      X_DST_L, dBot, X_SRC_R, sBot, // bottom edge (dst→src)
      SRC_COLOR[i],
    );
    srcCursor[i] = sBot - RIBBON_GAP;
    dstCursor[j] = dBot - RIBBON_GAP;
  }
}

const RIBBON_VERTS = ribbonFloats.length / 6; // 6 floats per pos2_color4 vertex
const NODE_VERTS = nodeFloats.length / 6;

const ribbonUpload: BufferUpload = { bufferId: RIBBON_BUFFER, op: 'append', floats: ribbonFloats };
const nodeUpload: BufferUpload = { bufferId: NODE_BUFFER, op: 'append', floats: nodeFloats };

export const manifest: SceneManifest = {
  label: 'Fund-Flow Sankey — ribbon tessellation',
  commands: [
    { cmd: 'createPane', id: PANE },
    { cmd: 'setPaneRegion', id: PANE, clipXMin: -0.98, clipXMax: 0.98, clipYMin: -0.98, clipYMax: 0.98 },
    { cmd: 'setPaneClearColor', id: PANE, r: 0.05, g: 0.05, b: 0.08, a: 1 },
    { cmd: 'createLayer', id: LAYER, paneId: PANE },

    // Ribbons first (under the node bars).
    { cmd: 'createBuffer', id: RIBBON_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: RIBBON_GEOMETRY, vertexBufferId: RIBBON_BUFFER, format: 'pos2_color4', vertexCount: RIBBON_VERTS },
    { cmd: 'createDrawItem', id: RIBBON_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: RIBBON_DRAWITEM, pipeline: 'triGradient@1', geometryId: RIBBON_GEOMETRY },

    // Node bars on top.
    { cmd: 'createBuffer', id: NODE_BUFFER, byteLength: 0 },
    { cmd: 'createGeometry', id: NODE_GEOMETRY, vertexBufferId: NODE_BUFFER, format: 'pos2_color4', vertexCount: NODE_VERTS },
    { cmd: 'createDrawItem', id: NODE_DRAWITEM, layerId: LAYER },
    { cmd: 'bindDrawItem', drawItemId: NODE_DRAWITEM, pipeline: 'triGradient@1', geometryId: NODE_GEOMETRY },
  ],
  // Static geometry baked once at manifest time (no replay).
  uploads: [ribbonUpload, nodeUpload],
};
