/* apps/showcase/views/sankey/records.gen.mjs  (ENC-581)
 *
 * Generator for the sankey view's LIVE replay records (zero-dep node).
 *
 * ── What changed (STATIC → LIVE) ──────────────────────────────────────────
 * The fund-flow Sankey used to bake ONE static ribbon/node tessellation as a
 * pair of triGradient@1 uploads — the diagram never moved. This generator makes
 * it LIVE via GEOMETRY-FRAME REPLAY: it re-computes the flow matrix at N
 * timesteps, re-tessellates the node bars + flow ribbons each timestep, and
 * emits an UPDATE_RANGE record per timestep that OVERWRITES the ribbon (and
 * node) vertex buffer with that frame's geometry. The replay engine
 * (useReplay → enqueueData) streams those frames in place; ENC-569's triGradient
 * backend re-reads + redraws the UPDATE_RANGE'd vertex buffer every frame, so the
 * ribbons pulse and re-route live as the flows drift.
 *
 * ── MECHANISM ─────────────────────────────────────────────────────────────
 * Each symbol's flow into the three buckets is the baked base matrix modulated by
 * a smooth, per-cell phase-offset oscillation (deterministic, no RNG). The node
 * heights, the ribbon widths, and the ribbon routing (stacking order along each
 * node edge) all follow from the matrix, so re-tessellating per frame makes the
 * whole Sankey breathe. The VERTEX COUNT IS CONSTANT across frames (the same 12
 * flow quads + 7 node quads every timestep — only their corner positions move),
 * so the pre-sized buffers are stable and every frame is a full-buffer
 * UPDATE_RANGE at offset 0.
 *
 * The geometry math here is the SINGLE SOURCE OF TRUTH and is mirrored exactly by
 * manifest.ts at phase 0 (frame 0) for the at-rest seed.
 *
 * USAGE: node apps/showcase/views/sankey/records.gen.mjs
 *   (overwrites ./records.json next to this file with the geometry-frame timeline.)
 */

import { writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const HERE = dirname(fileURLToPath(import.meta.url));
const RECORDS_PATH = join(HERE, 'records.json');

// ── record format (mirrors src/scene/commands.ts encodeUpload) ──────────────
const OP_UPDATE_RANGE = 2;
const HEADER_SIZE = 13; // [1B op][4B bufferId LE][4B offset LE][4B payloadBytes LE]
const RIBBON_BUFFER = 601; // pos2_color4 triangle list (ribbons)
const NODE_BUFFER = 602; // pos2_color4 triangle list (node bars)

// ── replay timeline ─────────────────────────────────────────────────────────
const FRAME_COUNT = 40;
const DURATION_MS = 20000;
const CADENCE_MS = Math.round(DURATION_MS / FRAME_COUNT);

// ── flow model (mirrors manifest.ts) ────────────────────────────────────────
const SOURCES = ['AAPL', 'MSFT', 'NVDA', 'TSLA'];
const DESTS = ['Inflow', 'Rotation', 'Outflow'];

// Baked base flow matrix (rows = source sectors, cols = [Inflow, Rotation, Outflow]).
const BASE_FLOW = [
  [12502.0, 37506.1, 61814.8], // AAPL
  [38941.1, 56800.4, 18933.5], // MSFT
  [19720.4, 59161.1, 36250.5], // NVDA
  [19657.4, 58972.3, 38300.3], // TSLA
];

// Per-source ribbon colors (RGBA), tinted by source. One hue per symbol.
const SRC_COLOR = [
  [0.27, 0.62, 0.95, 0.85], // AAPL — blue
  [0.35, 0.8, 0.55, 0.85], // MSFT — green
  [0.95, 0.72, 0.3, 0.85], // NVDA — amber
  [0.85, 0.4, 0.7, 0.85], // TSLA — magenta
];
const NODE_COLOR = [0.78, 0.82, 0.9, 1.0];

// Oscillation: each (source,bucket) cell breathes by ±AMP about its base value
// with a per-cell phase offset so buckets re-weight against each other (the flow
// re-routes), not just pulse in unison. phase advances a full 2π over the
// timeline so frame N wraps cleanly onto frame 0 (seamless loop).
const AMP = 0.45;

/** The flow matrix at normalized phase p∈[0,1) — base modulated per cell. */
function flowAt(p) {
  const out = [];
  for (let i = 0; i < SOURCES.length; i++) {
    const row = [];
    for (let j = 0; j < DESTS.length; j++) {
      // Per-cell phase offset spreads the oscillation across the matrix.
      const off = (i * DESTS.length + j) * ((2 * Math.PI) / (SOURCES.length * DESTS.length));
      const m = 1 + AMP * Math.sin(2 * Math.PI * p + off);
      row.push(BASE_FLOW[i][j] * m);
    }
    out.push(row);
  }
  return out;
}

// ── layout (clip space) — mirrors manifest.ts ───────────────────────────────
const X_SRC_L = -0.86;
const X_SRC_R = -0.74;
const X_DST_L = 0.74;
const X_DST_R = 0.86;
const Y_TOP = 0.82;
const Y_BOT = -0.82;
const NODE_GAP = 0.06;
const RIBBON_GAP = 0.0;

/** Push one quad (4 corners → 2 triangles → 6 vertices) in pos2_color4. */
function pushQuad(out, x0, y0, x1, y1, x2, y2, x3, y3, col) {
  const [r, g, b, a] = col;
  const v = (x, y) => out.push(x, y, r, g, b, a);
  v(x0, y0); v(x1, y1); v(x2, y2); // tri 1
  v(x0, y0); v(x2, y2); v(x3, y3); // tri 2
}

/**
 * Tessellate the whole Sankey for one flow matrix. Returns { ribbon, node }
 * pos2_color4 float arrays. Layout is identical to manifest.ts: node heights ∝
 * total flow (normalized to fill the band), ribbons leave/enter stacked along
 * each node edge in (source-major, dest) order.
 */
function tessellate(FLOW) {
  const grandTotal = FLOW.flat().reduce((a, b) => a + b, 0);
  const srcTotal = FLOW.map((row) => row.reduce((a, b) => a + b, 0));
  const dstTotal = DESTS.map((_, j) => FLOW.reduce((a, row) => a + row[j], 0));

  const srcBand = Y_TOP - Y_BOT - NODE_GAP * (SOURCES.length - 1);
  const dstBand = Y_TOP - Y_BOT - NODE_GAP * (DESTS.length - 1);
  const srcScale = srcBand / grandTotal;
  const dstScale = dstBand / grandTotal;

  // node bars
  const node = [];
  const srcY = [];
  let cursor = Y_TOP;
  for (let i = 0; i < SOURCES.length; i++) {
    const h = srcTotal[i] * srcScale;
    const top = cursor;
    const bot = cursor - h;
    srcY.push({ top, bot });
    pushQuad(node, X_SRC_L, top, X_SRC_R, top, X_SRC_R, bot, X_SRC_L, bot, NODE_COLOR);
    cursor = bot - NODE_GAP;
  }
  const dstY = [];
  cursor = Y_TOP;
  for (let j = 0; j < DESTS.length; j++) {
    const h = dstTotal[j] * dstScale;
    const top = cursor;
    const bot = cursor - h;
    dstY.push({ top, bot });
    pushQuad(node, X_DST_L, top, X_DST_R, top, X_DST_R, bot, X_DST_L, bot, NODE_COLOR);
    cursor = bot - NODE_GAP;
  }

  // ribbons (source-major, dest-minor) — constant 12 quads (all cells > 0).
  const ribbon = [];
  const srcCursor = srcY.map((n) => n.top);
  const dstCursor = dstY.map((n) => n.top);
  for (let i = 0; i < SOURCES.length; i++) {
    for (let j = 0; j < DESTS.length; j++) {
      const f = FLOW[i][j];
      const hSrc = f * srcScale;
      const hDst = f * dstScale;
      const sTop = srcCursor[i];
      const sBot = sTop - hSrc;
      const dTop = dstCursor[j];
      const dBot = dTop - hDst;
      pushQuad(
        ribbon,
        X_SRC_R, sTop, X_DST_L, dTop, // top edge (src→dst)
        X_DST_L, dBot, X_SRC_R, sBot, // bottom edge (dst→src)
        SRC_COLOR[i],
      );
      srcCursor[i] = sBot - RIBBON_GAP;
      dstCursor[j] = dBot - RIBBON_GAP;
    }
  }
  return { ribbon, node };
}

/** Encode one full-buffer UPDATE_RANGE record (offset 0) for `bufferId`. */
function encodeUpdateRange(bufferId, floats) {
  const buf = Buffer.alloc(HEADER_SIZE + floats.length * 4);
  buf.writeUInt8(OP_UPDATE_RANGE, 0);
  buf.writeUInt32LE(bufferId, 1);
  buf.writeUInt32LE(0, 5); // offset 0 — overwrite the whole buffer
  buf.writeUInt32LE(floats.length * 4, 9);
  let o = HEADER_SIZE;
  for (const f of floats) {
    buf.writeFloatLE(f, o);
    o += 4;
  }
  return buf.toString('base64');
}

function main() {
  // Sanity: vertex count must be constant across all frames so the buffers stay
  // stable (full-buffer UPDATE_RANGE). Verify the first and a mid frame agree.
  const f0 = tessellate(flowAt(0));
  const fMid = tessellate(flowAt(0.37));
  if (f0.ribbon.length !== fMid.ribbon.length || f0.node.length !== fMid.node.length) {
    throw new Error('vertex count drifted across frames — buffer would not be stable');
  }

  const frames = [];
  for (let k = 0; k < FRAME_COUNT; k++) {
    const p = k / FRAME_COUNT; // 0 .. <1 (frame N wraps onto frame 0)
    const { ribbon, node } = tessellate(flowAt(p));
    const t = Math.round((k / FRAME_COUNT) * DURATION_MS);
    // One batch per frame carrying BOTH buffer overwrites (ribbons + node bars),
    // so the node heights track the flows in lock-step with the ribbon widths.
    const ribbonRec = Buffer.from(encodeUpdateRange(RIBBON_BUFFER, ribbon), 'base64');
    const nodeRec = Buffer.from(encodeUpdateRange(NODE_BUFFER, node), 'base64');
    const batch = Buffer.concat([ribbonRec, nodeRec]).toString('base64');
    frames.push({ t, b64: batch });
  }

  const out = {
    meta: {
      viewId: 'sankey',
      durationMs: DURATION_MS,
      frameCount: frames.length,
      cadenceMs: CADENCE_MS,
    },
    frames,
  };

  writeFileSync(RECORDS_PATH, JSON.stringify(out));
  console.log(
    `sankey records: ${frames.length} geometry frames, ` +
      `${f0.ribbon.length / 6} ribbon verts + ${f0.node.length / 6} node verts/frame, ` +
      `t∈[0, ${frames[frames.length - 1].t}]ms, cadence=${CADENCE_MS}ms → buffers ${RIBBON_BUFFER}/${NODE_BUFFER}`,
  );
}

main();
