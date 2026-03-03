// main-gallery.ts — Exotic Chart Gallery
//
// Six unusual chart types rendered in a 3x2 grid, all defined in one
// JSON document (GALLERY below).  Uses every major pipeline:
//   instancedCandle@1  — Heikin-Ashi
//   instancedRect@1    — Renko bricks + Equivolume boxes
//   line2d@1           — Kagi trend lines + grid/dividers
//   triSolid@1         — Order-book depth shading
//   points@1           — Cross-asset correlation scatter

import { EngineHost } from "@repo/engine-host";
import type { EngineStats } from "@repo/engine-host";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  TYPES
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

type C4 = [number, number, number, number];

interface ThemeDef {
  name: string;
  bg: C4;
  up: C4; down: C4;        // bull / bear
  accent1: C4; accent2: C4; // extra palette
  volume: C4;
  grid: C4;
  crosshair: C4;
  divider: C4;
  bidFill: C4; askFill: C4; // order book
  scatter1: C4; scatter2: C4;
  // CSS
  cssBg: string; fg: string; fgMuted: string;
  accent: string; accentBorder: string;
}

interface PanelDef {
  id: string;
  label: string;
  tx: number; ty: number; sx: number; sy: number;
}

interface BufferDef { id: number }
interface TransformDef { id: number; tx: number; ty: number; sx: number; sy: number }
interface GeoDef {
  id: number; kind: "vertex" | "instanced";
  bufferId: number;
  format: string; strideBytes: number;
}
interface DrawItemDef {
  id: number; geoId: number; pipeline: string;
  transformId: number; role: string;
  clipRect?: [number, number, number, number]; // [ndcX0, ndcY0, ndcX1, ndcY1]
}

interface Gallery {
  meta: { title: string; version: number };
  data: { seed: number; candleCount: number };
  panels: PanelDef[];
  scene: {
    transforms: TransformDef[];
    buffers: BufferDef[];
    geometries: GeoDef[];
    drawItems: DrawItemDef[];
  };
  themes: Record<string, ThemeDef>;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  GALLERY DOCUMENT — the single source of truth
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Layout: 3 rows x 2 columns.  Each panel maps data in [-0.9, 0.9] to its
// quadrant via a unique transform.
//
//  ┌────────────┬────────────┐
//  │ Heikin-Ashi│   Renko    │  row 0  (top)
//  ├────────────┼────────────┤
//  │   Kagi     │ Order Book │  row 1  (mid)
//  ├────────────┼────────────┤
//  │ Correlation│ Equivolume │  row 2  (bottom)
//  └────────────┴────────────┘

const R = 1 / 3;  // row height fraction
const C = 0.5;     // col width fraction

function panelTx(col: number, row: number): { tx: number; ty: number; sx: number; sy: number } {
  const sx = C * 0.82;                    // tighter X to leave axis margin
  const sy = R * 0.74;                    // tighter Y to leave axis margin
  const tx = -0.46 + col * 0.96;         // slight right offset for Y-axis room
  const ty = 1 - R - row * 2 * R - 0.02; // slight down offset for top label room
  return { tx, ty, sx, sy };
}

const p = [
  panelTx(0, 0), // T10: Heikin-Ashi (top-left)
  panelTx(1, 0), // T11: Renko (top-right)
  panelTx(0, 1), // T12: Kagi (mid-left)
  panelTx(1, 1), // T13: Order Book (mid-right)
  panelTx(0, 2), // T14: Correlation (bot-left)
  panelTx(1, 2), // T15: Equivolume (bot-right)
];

// Clip rectangles per panel [ndcX0, ndcY0, ndcX1, ndcY1]
const clipRects: [number, number, number, number][] = [
  [-1, 1 - 2*R,  0, 1       ], // top-left
  [ 0, 1 - 2*R,  1, 1       ], // top-right
  [-1, 1 - 4*R,  0, 1 - 2*R ], // mid-left
  [ 0, 1 - 4*R,  1, 1 - 2*R ], // mid-right
  [-1, -1,        0, 1 - 4*R ], // bot-left
  [ 0, -1,        1, 1 - 4*R ], // bot-right
];

// Map transform ID → panel index → clip rect
const txToClip: Record<number, [number, number, number, number]> = {};
for (let i = 0; i < 6; i++) txToClip[10 + i] = clipRects[i];

const GALLERY: Gallery = {
  meta: { title: "Exotic Chart Gallery", version: 1 },
  data: { seed: 42, candleCount: 80 },
  panels: [
    { id: "heikin",  label: "Heikin-Ashi",  ...p[0] },
    { id: "renko",   label: "Renko",        ...p[1] },
    { id: "kagi",    label: "Kagi",         ...p[2] },
    { id: "depth",   label: "Order Book",   ...p[3] },
    { id: "scatter", label: "Correlation",  ...p[4] },
    { id: "equivol", label: "Equivolume",   ...p[5] },
  ],
  scene: {
    transforms: [
      { id: 10, ...p[0] },
      { id: 11, ...p[1] },
      { id: 12, ...p[2] },
      { id: 13, ...p[3] },
      { id: 14, ...p[4] },
      { id: 15, ...p[5] },
      { id: 99, tx: 0, ty: 0, sx: 1, sy: 1 }, // overlay identity
    ],
    buffers: [
      // Panel data buffers
      { id: 100 }, // Heikin-Ashi candles
      { id: 110 }, // Heikin-Ashi grid
      { id: 200 }, // Renko bricks
      { id: 210 }, // Renko grid
      { id: 300 }, // Kagi lines
      { id: 310 }, // Kagi grid
      { id: 400 }, // Order book bid fill (triangles)
      { id: 401 }, // Order book ask fill (triangles)
      { id: 410 }, // Order book bid line
      { id: 411 }, // Order book ask line
      { id: 420 }, // Depth grid
      { id: 500 }, // Scatter points
      { id: 510 }, // Scatter grid
      { id: 600 }, // Equivolume boxes
      { id: 610 }, // Equivolume grid
      // Overlay
      { id: 900 }, // Divider lines (panel borders)
      { id: 910 }, // Crosshair V
      { id: 920 }, // Crosshair H
    ],
    geometries: [
      // Heikin-Ashi
      { id: 1000, kind: "instanced", bufferId: 100, format: "candle6", strideBytes: 24 },
      { id: 1001, kind: "vertex",    bufferId: 110, format: "pos2_clip", strideBytes: 8 },
      // Renko
      { id: 1100, kind: "instanced", bufferId: 200, format: "rect4", strideBytes: 16 },
      { id: 1101, kind: "vertex",    bufferId: 210, format: "pos2_clip", strideBytes: 8 },
      // Kagi
      { id: 1200, kind: "vertex",    bufferId: 300, format: "pos2_clip", strideBytes: 8 },
      { id: 1201, kind: "vertex",    bufferId: 310, format: "pos2_clip", strideBytes: 8 },
      // Depth — bid/ask filled areas
      { id: 1300, kind: "vertex",    bufferId: 400, format: "pos2_clip", strideBytes: 8 },
      { id: 1301, kind: "vertex",    bufferId: 401, format: "pos2_clip", strideBytes: 8 },
      { id: 1302, kind: "vertex",    bufferId: 410, format: "pos2_clip", strideBytes: 8 },
      { id: 1303, kind: "vertex",    bufferId: 411, format: "pos2_clip", strideBytes: 8 },
      { id: 1304, kind: "vertex",    bufferId: 420, format: "pos2_clip", strideBytes: 8 },
      // Scatter
      { id: 1400, kind: "vertex",    bufferId: 500, format: "pos2_clip", strideBytes: 8 },
      { id: 1401, kind: "vertex",    bufferId: 510, format: "pos2_clip", strideBytes: 8 },
      // Equivolume
      { id: 1500, kind: "instanced", bufferId: 600, format: "rect4", strideBytes: 16 },
      { id: 1501, kind: "vertex",    bufferId: 610, format: "pos2_clip", strideBytes: 8 },
      // Overlay
      { id: 9000, kind: "vertex",    bufferId: 900, format: "pos2_clip", strideBytes: 8 },
      { id: 9010, kind: "vertex",    bufferId: 910, format: "pos2_clip", strideBytes: 8 },
      { id: 9020, kind: "vertex",    bufferId: 920, format: "pos2_clip", strideBytes: 8 },
    ],
    drawItems: [
      // Draw order matters: grids first, then data, then overlays

      // Grids (behind data)
      { id: 5001, geoId: 1001, pipeline: "line2d@1",          transformId: 10, role: "grid" },
      { id: 5101, geoId: 1101, pipeline: "line2d@1",          transformId: 11, role: "grid" },
      { id: 5201, geoId: 1201, pipeline: "line2d@1",          transformId: 12, role: "grid" },
      { id: 5301, geoId: 1304, pipeline: "line2d@1",          transformId: 13, role: "grid" },
      { id: 5401, geoId: 1401, pipeline: "line2d@1",          transformId: 14, role: "grid" },
      { id: 5501, geoId: 1501, pipeline: "line2d@1",          transformId: 15, role: "grid" },

      // Data layers
      { id: 5000, geoId: 1000, pipeline: "instancedCandle@1", transformId: 10, role: "heikinAshi" },
      { id: 5100, geoId: 1100, pipeline: "instancedRect@1",   transformId: 11, role: "renko" },
      { id: 5200, geoId: 1200, pipeline: "line2d@1",          transformId: 12, role: "kagi" },
      { id: 5300, geoId: 1300, pipeline: "triSolid@1",        transformId: 13, role: "bidFill" },
      { id: 5310, geoId: 1301, pipeline: "triSolid@1",        transformId: 13, role: "askFill" },
      { id: 5320, geoId: 1302, pipeline: "line2d@1",          transformId: 13, role: "bidLine" },
      { id: 5330, geoId: 1303, pipeline: "line2d@1",          transformId: 13, role: "askLine" },
      { id: 5400, geoId: 1400, pipeline: "points@1",          transformId: 14, role: "scatter" },
      { id: 5500, geoId: 1500, pipeline: "instancedRect@1",   transformId: 15, role: "equivol" },

      // Overlays (on top)
      { id: 9100, geoId: 9000, pipeline: "line2d@1",          transformId: 99, role: "divider" },
      { id: 9110, geoId: 9010, pipeline: "line2d@1",          transformId: 99, role: "crosshairV" },
      { id: 9120, geoId: 9020, pipeline: "line2d@1",          transformId: 99, role: "crosshairH" },
    ],
  },

  // ── Themes ──────────────────────────────────────────────────────────
  themes: {
    midnight: {
      name: "Midnight",
      bg: [0.04, 0.05, 0.10, 1],
      up: [0.0, 0.82, 0.73, 1], down: [0.93, 0.26, 0.38, 1],
      accent1: [0.35, 0.60, 1.0, 0.85], accent2: [0.85, 0.45, 1.0, 0.85],
      volume: [0.15, 0.45, 0.75, 0.5],
      grid: [0.25, 0.35, 0.55, 0.18],
      crosshair: [0.5, 0.6, 0.8, 0.25],
      divider: [0.3, 0.4, 0.65, 0.30],
      bidFill: [0.0, 0.65, 0.55, 0.20], askFill: [0.75, 0.15, 0.25, 0.20],
      scatter1: [0.3, 0.7, 1.0, 0.7], scatter2: [1.0, 0.5, 0.3, 0.7],
      cssBg: "rgb(10,13,26)", fg: "#b8c4d8", fgMuted: "rgba(160,175,200,0.45)",
      accent: "rgba(0,210,186,0.30)", accentBorder: "rgba(0,210,186,0.50)",
    },
    neon: {
      name: "Neon",
      bg: [0.015, 0.015, 0.03, 1],
      up: [0.0, 1.0, 0.4, 1], down: [1.0, 0.0, 0.4, 1],
      accent1: [0.0, 0.8, 1.0, 0.9], accent2: [1.0, 0.0, 1.0, 0.9],
      volume: [0.0, 0.6, 1.0, 0.6],
      grid: [0.0, 0.6, 0.8, 0.15],
      crosshair: [0.0, 1.0, 1.0, 0.30],
      divider: [0.0, 0.7, 0.9, 0.25],
      bidFill: [0.0, 0.8, 0.3, 0.25], askFill: [0.8, 0.0, 0.3, 0.25],
      scatter1: [0.0, 1.0, 0.8, 0.8], scatter2: [1.0, 0.3, 1.0, 0.8],
      cssBg: "rgb(4,4,8)", fg: "#d0f0ff", fgMuted: "rgba(100,200,255,0.45)",
      accent: "rgba(0,255,100,0.25)", accentBorder: "rgba(0,255,100,0.45)",
    },
    bloomberg: {
      name: "Bloomberg",
      bg: [0.0, 0.0, 0.0, 1],
      up: [0.2, 0.8, 0.2, 1], down: [0.9, 0.3, 0.1, 1],
      accent1: [1.0, 0.65, 0.0, 0.9], accent2: [0.4, 0.9, 0.4, 0.9],
      volume: [1.0, 0.65, 0.0, 0.45],
      grid: [0.3, 0.3, 0.3, 0.22],
      crosshair: [1.0, 0.65, 0.0, 0.30],
      divider: [0.35, 0.35, 0.35, 0.35],
      bidFill: [0.15, 0.6, 0.15, 0.20], askFill: [0.7, 0.2, 0.05, 0.20],
      scatter1: [1.0, 0.7, 0.0, 0.75], scatter2: [0.3, 0.9, 0.3, 0.75],
      cssBg: "rgb(0,0,0)", fg: "#ff9900", fgMuted: "rgba(255,153,0,0.45)",
      accent: "rgba(255,153,0,0.25)", accentBorder: "rgba(255,153,0,0.55)",
    },
    pastel: {
      name: "Pastel",
      bg: [0.97, 0.95, 0.92, 1],
      up: [0.32, 0.68, 0.50, 1], down: [0.78, 0.35, 0.38, 1],
      accent1: [0.40, 0.55, 0.80, 0.85], accent2: [0.70, 0.45, 0.70, 0.85],
      volume: [0.55, 0.60, 0.75, 0.40],
      grid: [0.0, 0.0, 0.0, 0.12],
      crosshair: [0.3, 0.3, 0.3, 0.15],
      divider: [0.0, 0.0, 0.0, 0.08],
      bidFill: [0.25, 0.55, 0.40, 0.15], askFill: [0.65, 0.25, 0.28, 0.15],
      scatter1: [0.35, 0.55, 0.78, 0.7], scatter2: [0.78, 0.40, 0.55, 0.7],
      cssBg: "rgb(248,242,235)", fg: "#3a3530", fgMuted: "rgba(80,70,60,0.45)",
      accent: "rgba(82,174,130,0.30)", accentBorder: "rgba(82,174,130,0.50)",
    },
    dark: {
      name: "Dark",
      bg: [0.04, 0.05, 0.07, 1],
      up: [0.16, 0.69, 0.43, 1], down: [0.85, 0.15, 0.20, 1],
      accent1: [0.25, 0.52, 0.95, 0.85], accent2: [0.95, 0.55, 0.10, 0.85],
      volume: [0.30, 0.50, 0.85, 0.50],
      grid: [1, 1, 1, 0.12],
      crosshair: [1, 1, 1, 0.20],
      divider: [1, 1, 1, 0.10],
      bidFill: [0.12, 0.55, 0.35, 0.18], askFill: [0.68, 0.12, 0.15, 0.18],
      scatter1: [0.3, 0.6, 1.0, 0.7], scatter2: [1.0, 0.45, 0.15, 0.7],
      cssBg: "rgb(10,13,18)", fg: "#e8e8f0", fgMuted: "rgba(200,200,208,0.45)",
      accent: "rgba(59,130,246,0.30)", accentBorder: "rgba(59,130,246,0.50)",
    },
    light: {
      name: "Light",
      bg: [0.95, 0.96, 0.97, 1],
      up: [0.12, 0.58, 0.35, 1], down: [0.78, 0.18, 0.22, 1],
      accent1: [0.20, 0.45, 0.85, 0.85], accent2: [0.85, 0.40, 0.10, 0.85],
      volume: [0.40, 0.55, 0.80, 0.40],
      grid: [0, 0, 0, 0.14],
      crosshair: [0, 0, 0, 0.12],
      divider: [0, 0, 0, 0.08],
      bidFill: [0.1, 0.50, 0.30, 0.12], askFill: [0.65, 0.12, 0.15, 0.12],
      scatter1: [0.15, 0.40, 0.80, 0.7], scatter2: [0.80, 0.35, 0.10, 0.7],
      cssBg: "rgb(242,245,247)", fg: "#1a1a2e", fgMuted: "rgba(30,30,60,0.40)",
      accent: "rgba(59,130,246,0.25)", accentBorder: "rgba(59,130,246,0.45)",
    },
  },
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  ENGINE
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("No canvas");
const $ = (id: string) => document.getElementById(id)!;

const host = new EngineHost({
  setFps: (fps: number) => { $("fps").textContent = fps.toFixed(0); },
  setGl: () => {}, setMem: () => {},
  setStats: (s: EngineStats) => { $("draws").textContent = String(s.drawCalls); },
});
host.init(canvas);
host.start();

function must(r: { ok: true } | { ok: false; error: string }) { if (!r.ok) throw new Error(r.error); }

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  DATA GENERATION
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function prng(seed: number) {
  let s = seed;
  return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
}

type Candle = { x: number; o: number; h: number; l: number; c: number; hw: number; vol: number };

const N = GALLERY.data.candleCount;
const candles: Candle[] = [];
{
  const r = prng(GALLERY.data.seed);
  let price = 0;
  for (let i = 0; i < N; i++) {
    const x = -0.85 + (1.7 * i) / (N - 1);
    const drift = (r() * 2 - 1) * 0.04 - price * 0.01;
    const o = price, c = o + drift;
    const h = Math.max(o, c) + r() * 0.025;
    const l = Math.min(o, c) - r() * 0.025;
    const vol = 0.2 + r() * 0.8;
    candles.push({ x, o, h, l, c, hw: 0.75 / N, vol });
    price = c;
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  BINARY HELPERS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function sendF(bid: number, f: Float32Array) {
  const p = new Uint8Array(f.buffer, f.byteOffset, f.byteLength);
  const buf = new ArrayBuffer(13 + p.byteLength);
  const u = new Uint8Array(buf);
  const dv = new DataView(buf);
  u[0] = 2;
  dv.setUint32(1, bid, true);
  dv.setUint32(5, 0, true);
  dv.setUint32(9, p.byteLength, true);
  u.set(p, 13);
  host.enqueueData(buf);
}

function sendLine(bid: number, x0: number, y0: number, x1: number, y1: number) {
  sendF(bid, new Float32Array([x0, y0, x1, y1]));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SCENE BUILDER (from JSON)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function buildScene(doc: Gallery) {
  for (const t of doc.scene.transforms) {
    must(host.applyControl({ cmd: "createTransform", id: t.id }));
    must(host.applyControl({ cmd: "setTransform", id: t.id, tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy }));
  }
  for (const b of doc.scene.buffers) {
    must(host.applyControl({ cmd: "createBuffer", id: b.id }));
  }
  for (const g of doc.scene.geometries) {
    if (g.kind === "vertex") {
      must(host.applyControl({ cmd: "createGeometry", id: g.id, vertexBufferId: g.bufferId, format: g.format, strideBytes: g.strideBytes }));
    } else {
      must(host.applyControl({ cmd: "createInstancedGeometry", id: g.id, instanceBufferId: g.bufferId, instanceFormat: g.format, instanceStrideBytes: g.strideBytes }));
    }
  }
  for (const di of doc.scene.drawItems) {
    must(host.applyControl({ cmd: "createDrawItem", id: di.id, geometryId: di.geoId, pipeline: di.pipeline }));
    must(host.applyControl({ cmd: "attachTransform", targetId: di.id, transformId: di.transformId }));
    // Apply scissor clip rect from panel mapping
    const clip = txToClip[di.transformId];
    if (clip) {
      must(host.applyControl({ cmd: "setDrawItemClipRect", id: di.id, rect: clip }));
    }
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  DATA GENERATORS (one per chart type)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Helper: grid lines + axis spines + tick marks
const E = 0.88; // data edge (grid extent)
const TK = 0.04; // tick mark length
function makeGrid(bid: number, rows: number) {
  const segs: number[] = [];
  // Horizontal grid lines
  for (let i = 0; i < rows; i++) {
    const y = -E + (2 * E * i) / (rows - 1);
    segs.push(-E, y, E, y);
  }
  // Axis spines: left vertical + bottom horizontal (L-shape)
  segs.push(-E, -E, -E, E);   // Y-axis spine
  segs.push(-E, -E, E, -E);   // X-axis spine
  // Y-axis tick marks
  for (let i = 0; i < rows; i++) {
    const y = -E + (2 * E * i) / (rows - 1);
    segs.push(-E - TK, y, -E, y);
  }
  // X-axis tick marks (same count as rows for symmetry)
  for (let i = 0; i < rows; i++) {
    const x = -E + (2 * E * i) / (rows - 1);
    segs.push(x, -E - TK, x, -E);
  }
  sendF(bid, new Float32Array(segs));
}

// ── 1. Heikin-Ashi ──────────────────────────────────────────────────────
function pushHeikinAshi() {
  const ha: { o: number; h: number; l: number; c: number }[] = [];
  for (let i = 0; i < N; i++) {
    const cd = candles[i];
    const c = (cd.o + cd.h + cd.l + cd.c) / 4;
    const o = i === 0 ? cd.o : (ha[i-1].o + ha[i-1].c) / 2;
    const h = Math.max(cd.h, o, c);
    const l = Math.min(cd.l, o, c);
    ha.push({ o, h, l, c });
  }
  // Find Y range for normalization
  let minY = Infinity, maxY = -Infinity;
  for (const h of ha) { minY = Math.min(minY, h.l); maxY = Math.max(maxY, h.h); }
  const yR = maxY - minY || 1;
  const normY = (v: number) => -E + ((v - minY) / yR) * 2 * E;

  const f = new Float32Array(N * 6);
  for (let i = 0; i < N; i++) {
    f[i*6]   = candles[i].x;
    f[i*6+1] = normY(ha[i].o);
    f[i*6+2] = normY(ha[i].h);
    f[i*6+3] = normY(ha[i].l);
    f[i*6+4] = normY(ha[i].c);
    f[i*6+5] = candles[i].hw;
  }
  sendF(100, f);
  makeGrid(110, 6);
}

// ── 2. Renko ────────────────────────────────────────────────────────────
function pushRenko() {
  const brickSize = 0.04;
  const bricks: { x: number; y0: number; y1: number; up: boolean }[] = [];
  let base = candles[0].c;
  let brickX = -0.85;
  const step = 1.7 / N;

  for (let i = 1; i < N; i++) {
    const c = candles[i].c;
    while (c >= base + brickSize) {
      bricks.push({ x: brickX, y0: base, y1: base + brickSize, up: true });
      base += brickSize;
      brickX += step;
    }
    while (c <= base - brickSize) {
      base -= brickSize;
      bricks.push({ x: brickX, y0: base, y1: base + brickSize, up: false });
      brickX += step;
    }
  }

  // Normalize to [-E, E]
  if (bricks.length === 0) return;
  let minY = Infinity, maxY = -Infinity, maxX = -Infinity;
  for (const b of bricks) { minY = Math.min(minY, b.y0); maxY = Math.max(maxY, b.y1); maxX = Math.max(maxX, b.x); }
  const yRange = maxY - minY || 1;
  const xRange = (maxX - (-0.85)) || 1;
  const hw = (2 * E / bricks.length) * 0.4;

  const f = new Float32Array(bricks.length * 4);
  for (let i = 0; i < bricks.length; i++) {
    const b = bricks[i];
    const cx = -E + ((b.x - (-0.85)) / xRange) * 2 * E;
    const ny0 = -E + ((b.y0 - minY) / yRange) * 2 * E;
    const ny1 = -E + ((b.y1 - minY) / yRange) * 2 * E;
    f[i*4]   = cx - hw;
    f[i*4+1] = ny0;
    f[i*4+2] = cx + hw;
    f[i*4+3] = ny1;
  }
  sendF(200, f);

  (pushRenko as any)._bricks = bricks;
  makeGrid(210, 6);
}

// ── 3. Kagi ─────────────────────────────────────────────────────────────
function pushKagi() {
  const reversal = 0.04;
  const pts: { x: number; y: number }[] = [{ x: candles[0].x, y: candles[0].c }];
  let dir = 0; // 0=none, 1=up, -1=down
  let lastY = candles[0].c;
  let col = 0;

  for (let i = 1; i < N; i++) {
    const c = candles[i].c;
    const diff = c - lastY;

    if (dir === 0) {
      dir = diff > 0 ? 1 : -1;
      lastY = c;
      pts.push({ x: candles[i].x, y: c });
    } else if ((dir === 1 && diff < -reversal) || (dir === -1 && diff > reversal)) {
      // Reversal: horizontal then vertical
      col++;
      const nx = -0.85 + (1.7 * col) / (N * 0.6);
      pts.push({ x: nx, y: lastY }); // horizontal
      pts.push({ x: nx, y: c });     // vertical
      dir = diff > 0 ? 1 : -1;
      lastY = c;
    } else if ((dir === 1 && diff > 0) || (dir === -1 && diff < 0)) {
      // Continue in same direction
      pts[pts.length - 1].y = c;
      lastY = c;
    }
  }

  // Normalize Y
  let minY = Infinity, maxY = -Infinity;
  for (const p of pts) { minY = Math.min(minY, p.y); maxY = Math.max(maxY, p.y); }
  const yR = maxY - minY || 1;
  let minX = Infinity, maxX = -Infinity;
  for (const p of pts) { minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x); }
  const xR = maxX - minX || 1;

  // Build line segments (normalized to [-E, E])
  const segs: number[] = [];
  for (let i = 1; i < pts.length; i++) {
    segs.push(
      -E + ((pts[i-1].x - minX) / xR) * 2 * E,
      -E + ((pts[i-1].y - minY) / yR) * 2 * E,
      -E + ((pts[i].x - minX) / xR) * 2 * E,
      -E + ((pts[i].y - minY) / yR) * 2 * E,
    );
  }
  sendF(300, segs.length > 0 ? new Float32Array(segs) : new Float32Array([0,0,0,0]));
  makeGrid(310, 6);
}

// ── 4. Order Book Depth ─────────────────────────────────────────────────
function pushOrderBook() {
  const rng = prng(77);
  const levels = 30;

  // Generate bid/ask levels
  const mid = 0.0;
  const bidPrices: number[] = [], askPrices: number[] = [];
  const bidSizes: number[] = [],  askSizes: number[] = [];
  let cumBid = 0, cumAsk = 0;

  for (let i = 0; i < levels; i++) {
    const spread = 0.02 + i * 0.025;
    bidPrices.push(mid - spread);
    askPrices.push(mid + spread);
    cumBid += 0.1 + rng() * 0.5 + i * 0.03;
    cumAsk += 0.1 + rng() * 0.5 + i * 0.03;
    bidSizes.push(cumBid);
    askSizes.push(cumAsk);
  }

  // Normalize
  const maxSize = Math.max(bidSizes[levels-1], askSizes[levels-1]);
  const minP = bidPrices[levels-1], maxP = askPrices[levels-1];
  const pRange = maxP - minP || 1;

  function normP(v: number) { return -E + ((v - minP) / pRange) * 2 * E; }
  function normS(v: number) { return -E + (v / maxSize) * 2 * E; }

  // Bid filled area (triangles: strip converted to triangles)
  const bidTris: number[] = [];
  const bidLine: number[] = [];
  const baseY = -E;
  for (let i = 0; i < levels; i++) {
    const px = normP(bidPrices[i]);
    const sy = normS(bidSizes[i]);
    if (i > 0) {
      const ppx = normP(bidPrices[i-1]);
      const psy = normS(bidSizes[i-1]);
      // Two triangles for the step
      bidTris.push(ppx, baseY, ppx, psy, px, psy);
      bidTris.push(ppx, baseY, px, psy, px, baseY);
      bidLine.push(ppx, psy, px, psy);
    }
    if (i < levels - 1) {
      bidLine.push(px, normS(bidSizes[i]), px, normS(bidSizes[i+1]));
    }
  }

  // Ask filled area
  const askTris: number[] = [];
  const askLine: number[] = [];
  for (let i = 0; i < levels; i++) {
    const px = normP(askPrices[i]);
    const sy = normS(askSizes[i]);
    if (i > 0) {
      const ppx = normP(askPrices[i-1]);
      const psy = normS(askSizes[i-1]);
      askTris.push(ppx, baseY, ppx, psy, px, psy);
      askTris.push(ppx, baseY, px, psy, px, baseY);
      askLine.push(ppx, psy, px, psy);
    }
    if (i < levels - 1) {
      askLine.push(px, normS(askSizes[i]), px, normS(askSizes[i+1]));
    }
  }

  sendF(400, new Float32Array(bidTris));
  sendF(401, new Float32Array(askTris));
  sendF(410, bidLine.length > 0 ? new Float32Array(bidLine) : new Float32Array([0,0,0,0]));
  sendF(411, askLine.length > 0 ? new Float32Array(askLine) : new Float32Array([0,0,0,0]));
  makeGrid(420, 6);
}

// ── 5. Correlation Scatter ──────────────────────────────────────────────
function pushScatter() {
  // Two correlated random walks → scatter of returns
  const rng = prng(99);
  const nPts = 200;
  const pts = new Float32Array(nPts * 2);

  for (let i = 0; i < nPts; i++) {
    // Box-Muller for correlated normals (rho ≈ 0.6)
    const u1 = Math.max(1e-10, rng()), u2 = rng();
    const z0 = Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
    const z1 = Math.sqrt(-2 * Math.log(u1)) * Math.sin(2 * Math.PI * u2);
    const rho = 0.6;
    const x = z0 * 0.35;
    const y = (rho * z0 + Math.sqrt(1 - rho * rho) * z1) * 0.35;
    pts[i*2]   = Math.max(-E, Math.min(E, x));
    pts[i*2+1] = Math.max(-E, Math.min(E, y));
  }
  sendF(500, pts);

  // Grid: crosshairs at origin + axes
  makeGrid(510, 5);
}

// ── 6. Equivolume ───────────────────────────────────────────────────────
function pushEquivolume() {
  // Width proportional to volume, no gaps, colored by direction
  let totalVol = 0;
  for (const c of candles) totalVol += c.vol;

  // Normalize prices
  let minP = Infinity, maxP = -Infinity;
  for (const c of candles) { minP = Math.min(minP, c.l); maxP = Math.max(maxP, c.h); }
  const pR = maxP - minP || 1;

  const rects = new Float32Array(N * 4);
  let cx = -E;
  for (let i = 0; i < N; i++) {
    const c = candles[i];
    const w = (c.vol / totalVol) * 2 * E;
    const y0 = -E + ((Math.min(c.o, c.c) - minP) / pR) * 2 * E;
    const y1 = -E + ((Math.max(c.o, c.c) - minP) / pR) * 2 * E;
    rects[i*4]   = cx;
    rects[i*4+1] = y0;
    rects[i*4+2] = cx + w * 0.92;
    rects[i*4+3] = Math.max(y0 + 0.005, y1);
    cx += w;
  }
  sendF(600, rects);
  makeGrid(610, 6);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  DIVIDER LINES (panel borders)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function pushDividers() {
  const lines: number[] = [];
  // Vertical center
  lines.push(0, -1, 0, 1);
  // Horizontal thirds
  const y1 = 1 - 2 * R;      // ≈ 0.333
  const y2 = 1 - 4 * R;      // ≈ -0.333
  lines.push(-1, y1, 1, y1);
  lines.push(-1, y2, 1, y2);
  sendF(900, new Float32Array(lines));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  THEME APPLICATOR
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

let currentTheme = "midnight";

function applyTheme(key: string) {
  const t = GALLERY.themes[key];
  if (!t) return;
  currentTheme = key;

  const roleMap: Record<string, () => void> = {
    heikinAshi: () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5000, colorUp: t.up, colorDown: t.down })),
    renko:      () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5100, color: t.up })),
    kagi:       () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5200, color: t.accent1 })),
    bidFill:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5300, color: t.bidFill })),
    askFill:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5310, color: t.askFill })),
    bidLine:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5320, color: t.up })),
    askLine:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5330, color: t.down })),
    scatter:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5400, color: t.scatter1 })),
    equivol:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 5500, color: t.accent1 })),
    grid:       () => {
      for (const di of GALLERY.scene.drawItems) {
        if (di.role === "grid") must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: t.grid }));
      }
    },
    divider:    () => must(host.applyControl({ cmd: "setDrawItemColor", id: 9100, color: t.divider })),
    crosshairV: () => must(host.applyControl({ cmd: "setDrawItemColor", id: 9110, color: t.crosshair })),
    crosshairH: () => must(host.applyControl({ cmd: "setDrawItemColor", id: 9120, color: t.crosshair })),
  };

  for (const [, apply] of Object.entries(roleMap)) apply();

  // Background
  host.applyControl({ cmd: "setClearColor", r: t.bg[0], g: t.bg[1], b: t.bg[2], a: t.bg[3] });

  // CSS
  const root = document.documentElement;
  root.style.setProperty("--bg", t.cssBg);
  root.style.setProperty("--fg", t.fg);
  root.style.setProperty("--fg-muted", t.fgMuted);
  root.style.setProperty("--accent", t.accent);
  root.style.setProperty("--accent-border", t.accentBorder);

  // Buttons
  document.querySelectorAll<HTMLButtonElement>(".tb[data-theme]").forEach(btn => {
    btn.classList.toggle("active", btn.dataset.theme === key);
  });
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  INTERACTION
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function c2ndc(cx: number, cy: number) {
  const r = canvas.getBoundingClientRect();
  return {
    nx: ((cx - r.left) / Math.max(1, r.width)) * 2 - 1,
    ny: 1 - ((cy - r.top) / Math.max(1, r.height)) * 2,
  };
}

// Figure out which panel [0..5] the cursor is in
function panelAt(nx: number, ny: number): number {
  const col = nx < 0 ? 0 : 1;
  let row: number;
  if (ny > 1 - 2 * R) row = 0;
  else if (ny > 1 - 4 * R) row = 1;
  else row = 2;
  return row * 2 + col;
}

// Store per-panel zoom offsets
const panelZoom = new Float32Array(6); // extra sx multiplier per panel
panelZoom.fill(1);
const panelTxOff = new Float32Array(6);
const panelTyOff = new Float32Array(6);

function syncPanelTransform(idx: number) {
  const base = GALLERY.scene.transforms[idx];
  const z = panelZoom[idx];
  host.applyControl({
    cmd: "setTransform", id: base.id,
    tx: base.tx + panelTxOff[idx],
    ty: base.ty + panelTyOff[idx],
    sx: base.sx * z,
    sy: base.sy * z,
  });
}

let dragging = false;
let lastX = 0, lastY = 0;
let activePanel = 0;

canvas.addEventListener("mousedown", (e) => {
  const { nx, ny } = c2ndc(e.clientX, e.clientY);
  activePanel = panelAt(nx, ny);
  dragging = true;
  lastX = e.clientX; lastY = e.clientY;
  canvas.style.cursor = "grabbing";
});

window.addEventListener("mouseup", () => {
  dragging = false;
  canvas.style.cursor = "grab";
});

window.addEventListener("mousemove", (e) => {
  const { nx, ny } = c2ndc(e.clientX, e.clientY);

  // Crosshair
  sendLine(910, nx, -1, nx, 1);
  sendLine(920, -1, ny, 1, ny);

  if (!dragging) return;
  const rect = canvas.getBoundingClientRect();
  const dx = (e.clientX - lastX) / Math.max(1, rect.width) * 2;
  const dy = -(e.clientY - lastY) / Math.max(1, rect.height) * 2;
  lastX = e.clientX; lastY = e.clientY;

  panelTxOff[activePanel] += dx;
  panelTyOff[activePanel] += dy;
  syncPanelTransform(activePanel);
});

canvas.addEventListener("wheel", (e) => {
  e.preventDefault();
  const { nx, ny } = c2ndc(e.clientX, e.clientY);
  const idx = panelAt(nx, ny);
  const z = e.deltaY < 0 ? 1.12 : 1 / 1.12;
  panelZoom[idx] = Math.max(0.2, Math.min(8, panelZoom[idx] * z));
  syncPanelTransform(idx);
}, { passive: false });

canvas.addEventListener("dblclick", (e) => {
  const { nx, ny } = c2ndc(e.clientX, e.clientY);
  const idx = panelAt(nx, ny);
  panelZoom[idx] = 1;
  panelTxOff[idx] = 0;
  panelTyOff[idx] = 0;
  syncPanelTransform(idx);
});

// Theme keyboard shortcuts (1-6)
const themeKeys = Object.keys(GALLERY.themes);
window.addEventListener("keydown", (e) => {
  const idx = parseInt(e.key) - 1;
  if (idx >= 0 && idx < themeKeys.length) applyTheme(themeKeys[idx]);
  if (e.key === "r" || e.key === "R") {
    for (let i = 0; i < 6; i++) { panelZoom[i] = 1; panelTxOff[i] = 0; panelTyOff[i] = 0; syncPanelTransform(i); }
  }
});

// Theme buttons
document.querySelectorAll<HTMLButtonElement>(".tb[data-theme]").forEach(btn => {
  btn.addEventListener("click", () => {
    const key = btn.dataset.theme;
    if (key) applyTheme(key);
  });
});

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  INIT
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

buildScene(GALLERY);

pushHeikinAshi();
pushRenko();
pushKagi();
pushOrderBook();
pushScatter();
pushEquivolume();
pushDividers();

applyTheme("midnight");
canvas.style.cursor = "grab";

console.log(
  `Exotic Chart Gallery: ${N} candles → 6 chart types, ${themeKeys.length} themes\n` +
  `  Heikin-Ashi (instancedCandle@1)\n` +
  `  Renko (instancedRect@1)\n` +
  `  Kagi (line2d@1)\n` +
  `  Order Book Depth (triSolid@1)\n` +
  `  Correlation Scatter (points@1)\n` +
  `  Equivolume (instancedRect@1)`
);

(globalThis as any).__host = host;
(globalThis as any).__gallery = GALLERY;
