// main-themes.ts — Single-file-driven theme showcase
//
// The entire chart is declared in one JSON document (CHART_DOC below).
// A small interpreter creates all engine resources from it, then applies
// theme colors in real-time via setDrawItemColor.

import { EngineHost } from "@repo/engine-host";
import type { EngineStats } from "@repo/engine-host";

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 1. CHART DOCUMENT — single source of truth
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

type Color4 = [number, number, number, number];

interface ThemeDef {
  name: string;
  background: Color4;
  candleUp: Color4;
  candleDown: Color4;
  volume: Color4;
  gridColor: Color4;
  gridLineWidth: number;
  gridDashLength: number;
  gridGapLength: number;
  gridOpacity: number;
  crosshairColor: Color4;
  dividerColor: Color4;
  panelBg: string;       // CSS for overlay panels
  fg: string;            // CSS for text
  fgMuted: string;       // CSS for muted text
  accent: string;        // CSS for active button
  accentBorder: string;  // CSS for active button border
}

interface ChartDocument {
  meta: { title: string; version: number };
  data: { candleCount: number; seed: number };
  scene: {
    transforms: { id: number; tx: number; ty: number; sx: number; sy: number }[];
    buffers: { id: number }[];
    geometries: (
      | { id: number; kind: "vertex"; vertexBufferId: number; format: string; strideBytes: number }
      | { id: number; kind: "instanced"; instanceBufferId: number; instanceFormat: string; instanceStrideBytes: number }
    )[];
    drawItems: {
      id: number;
      geometryId: number;
      pipeline: string;
      transformId: number;
      role: string; // semantic role for theme mapping
    }[];
  };
  themes: Record<string, ThemeDef>;
}

const CHART_DOC: ChartDocument = {
  meta: { title: "DynaCharting Theme Showcase", version: 1 },
  data: { candleCount: 120, seed: 42 },
  scene: {
    transforms: [
      { id: 1, tx: 0, ty: 0.50, sx: 1, sy: 0.44 },   // price panel
      { id: 2, tx: 0, ty: -0.52, sx: 1, sy: 0.38 },   // volume panel
      { id: 3, tx: 0, ty: 0, sx: 1, sy: 1 },           // overlay (identity)
    ],
    buffers: [
      { id: 100 },   // candle data
      { id: 110 },   // volume data
      { id: 120 },   // horizontal grid lines (price panel)
      { id: 130 },   // horizontal grid lines (volume panel)
      { id: 200 },   // divider line
      { id: 210 },   // crosshair vertical
      { id: 220 },   // crosshair horizontal
    ],
    geometries: [
      { id: 1000, kind: "instanced", instanceBufferId: 100, instanceFormat: "candle6", instanceStrideBytes: 24 },
      { id: 1010, kind: "instanced", instanceBufferId: 110, instanceFormat: "rect4", instanceStrideBytes: 16 },
      { id: 1020, kind: "vertex", vertexBufferId: 120, format: "pos2_clip", strideBytes: 8 },
      { id: 1030, kind: "vertex", vertexBufferId: 130, format: "pos2_clip", strideBytes: 8 },
      { id: 2000, kind: "vertex", vertexBufferId: 200, format: "pos2_clip", strideBytes: 8 },
      { id: 2010, kind: "vertex", vertexBufferId: 210, format: "pos2_clip", strideBytes: 8 },
      { id: 2020, kind: "vertex", vertexBufferId: 220, format: "pos2_clip", strideBytes: 8 },
    ],
    drawItems: [
      { id: 5000, geometryId: 1020, pipeline: "line2d@1", transformId: 1, role: "gridPrice" },
      { id: 5010, geometryId: 1030, pipeline: "line2d@1", transformId: 2, role: "gridVolume" },
      { id: 5100, geometryId: 1010, pipeline: "instancedRect@1", transformId: 2, role: "volume" },
      { id: 5200, geometryId: 1000, pipeline: "instancedCandle@1", transformId: 1, role: "candle" },
      { id: 6000, geometryId: 2000, pipeline: "line2d@1", transformId: 3, role: "divider" },
      { id: 6010, geometryId: 2010, pipeline: "line2d@1", transformId: 3, role: "crosshairV" },
      { id: 6020, geometryId: 2020, pipeline: "line2d@1", transformId: 3, role: "crosshairH" },
    ],
  },
  themes: {
    dark: {
      name: "Dark",
      background: [0.04, 0.05, 0.08, 1],
      candleUp: [0.16, 0.69, 0.43, 1],
      candleDown: [0.85, 0.15, 0.20, 1],
      volume: [0.30, 0.50, 0.85, 0.55],
      gridColor: [1, 1, 1, 0.07],
      gridLineWidth: 1, gridDashLength: 0, gridGapLength: 0, gridOpacity: 1.0,
      crosshairColor: [1, 1, 1, 0.25],
      dividerColor: [1, 1, 1, 0.12],
      panelBg: "rgba(8,10,18,0.75)", fg: "#e8e8f0", fgMuted: "rgba(200,200,208,0.5)",
      accent: "rgba(59,130,246,0.4)", accentBorder: "rgba(59,130,246,0.6)",
    },
    midnight: {
      name: "Midnight",
      background: [0.05, 0.06, 0.12, 1],
      candleUp: [0.0, 0.82, 0.73, 1],
      candleDown: [0.93, 0.26, 0.38, 1],
      volume: [0.15, 0.45, 0.75, 0.5],
      gridColor: [0.3, 0.4, 0.6, 0.12],
      gridLineWidth: 1, gridDashLength: 0, gridGapLength: 0, gridOpacity: 1.0,
      crosshairColor: [0.5, 0.6, 0.8, 0.3],
      dividerColor: [0.3, 0.4, 0.6, 0.25],
      panelBg: "rgba(12,14,28,0.8)", fg: "#b8c4d8", fgMuted: "rgba(160,175,200,0.5)",
      accent: "rgba(0,210,186,0.35)", accentBorder: "rgba(0,210,186,0.55)",
    },
    neon: {
      name: "Neon",
      background: [0.02, 0.02, 0.04, 1],
      candleUp: [0.0, 1.0, 0.4, 1],
      candleDown: [1.0, 0.0, 0.4, 1],
      volume: [0.0, 0.6, 1.0, 0.6],
      gridColor: [0.0, 0.8, 1.0, 0.15],
      gridLineWidth: 1, gridDashLength: 6, gridGapLength: 4, gridOpacity: 0.8,
      crosshairColor: [0.0, 1.0, 1.0, 0.35],
      dividerColor: [0.0, 0.8, 1.0, 0.3],
      panelBg: "rgba(4,4,12,0.85)", fg: "#d0f0ff", fgMuted: "rgba(100,200,255,0.5)",
      accent: "rgba(0,255,100,0.3)", accentBorder: "rgba(0,255,100,0.5)",
    },
    pastel: {
      name: "Pastel",
      background: [0.98, 0.96, 0.93, 1],
      candleUp: [0.32, 0.72, 0.53, 1],
      candleDown: [0.82, 0.35, 0.35, 1],
      volume: [0.55, 0.65, 0.80, 0.45],
      gridColor: [0.0, 0.0, 0.0, 0.06],
      gridLineWidth: 1, gridDashLength: 0, gridGapLength: 0, gridOpacity: 0.5,
      crosshairColor: [0.3, 0.3, 0.3, 0.2],
      dividerColor: [0.0, 0.0, 0.0, 0.1],
      panelBg: "rgba(240,232,220,0.85)", fg: "#3a3530", fgMuted: "rgba(80,70,60,0.5)",
      accent: "rgba(82,184,136,0.35)", accentBorder: "rgba(82,184,136,0.55)",
    },
    bloomberg: {
      name: "Bloomberg",
      background: [0.0, 0.0, 0.0, 1],
      candleUp: [0.2, 0.8, 0.2, 1],
      candleDown: [0.9, 0.3, 0.1, 1],
      volume: [1.0, 0.65, 0.0, 0.5],
      gridColor: [0.3, 0.3, 0.3, 0.25],
      gridLineWidth: 1, gridDashLength: 4, gridGapLength: 3, gridOpacity: 1.0,
      crosshairColor: [1.0, 0.65, 0.0, 0.35],
      dividerColor: [0.4, 0.4, 0.4, 0.3],
      panelBg: "rgba(0,0,0,0.85)", fg: "#ff9900", fgMuted: "rgba(255,153,0,0.5)",
      accent: "rgba(255,153,0,0.3)", accentBorder: "rgba(255,153,0,0.6)",
    },
    light: {
      name: "Light",
      background: [0.96, 0.97, 0.98, 1],
      candleUp: [0.12, 0.60, 0.35, 1],
      candleDown: [0.80, 0.18, 0.22, 1],
      volume: [0.40, 0.55, 0.80, 0.45],
      gridColor: [0.0, 0.0, 0.0, 0.08],
      gridLineWidth: 1, gridDashLength: 0, gridGapLength: 0, gridOpacity: 1.0,
      crosshairColor: [0.0, 0.0, 0.0, 0.15],
      dividerColor: [0.0, 0.0, 0.0, 0.1],
      panelBg: "rgba(240,242,245,0.85)", fg: "#1a1a2e", fgMuted: "rgba(30,30,60,0.45)",
      accent: "rgba(59,130,246,0.3)", accentBorder: "rgba(59,130,246,0.5)",
    },
  },
};

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 2. DOM REFERENCES
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("No canvas #c");
const $ = (id: string) => document.getElementById(id)!;

const fpsEl = $("fps"), frameEl = $("framems"), drawEl = $("drawcalls"), barsEl = $("bars");
const valO = $("val-o"), valH = $("val-h"), valL = $("val-l"), valC = $("val-c");
const themeLabel = $("theme-label");
const themeBtns = document.querySelectorAll<HTMLButtonElement>(".theme-btn");

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 3. ENGINE INIT
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const hud = {
  setFps: (fps: number) => { fpsEl.textContent = fps.toFixed(1); },
  setGl: () => {}, setMem: () => {},
  setStats: (s: EngineStats) => {
    frameEl.textContent = s.frameMs.toFixed(2);
    drawEl.textContent = String(s.drawCalls);
  },
};
const host = new EngineHost(hud);
host.init(canvas);
host.start();

function must(r: { ok: true } | { ok: false; error: string }) { if (!r.ok) throw new Error(r.error); }

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 4. DATA GENERATION — deterministic PRNG candles
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function prng(seed: number) {
  let s = seed;
  return () => { s = (s * 16807) % 2147483647; return (s - 1) / 2147483646; };
}

type Candle = { x: number; o: number; h: number; l: number; c: number; hw: number };

const N = CHART_DOC.data.candleCount;
const candles: Candle[] = [];
{
  const r = prng(CHART_DOC.data.seed);
  let price = 0;
  for (let i = 0; i < N; i++) {
    const x = -0.95 + (1.9 * i) / (N - 1);
    const drift = (r() * 2 - 1) * 0.035 - price * 0.012;
    const o = price, c = o + drift;
    const h = Math.max(o, c) + r() * 0.02;
    const l = Math.min(o, c) - r() * 0.02;
    candles.push({ x, o, h, l, c, hw: 0.8 / N });
    price = c;
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 5. BINARY HELPERS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function sendF(bid: number, f: Float32Array) {
  const p = new Uint8Array(f.buffer, f.byteOffset, f.byteLength);
  const buf = new ArrayBuffer(13 + p.byteLength);
  const u = new Uint8Array(buf);
  const dv = new DataView(buf);
  u[0] = 2; // updateRange
  dv.setUint32(1, bid, true);
  dv.setUint32(5, 0, true);
  dv.setUint32(9, p.byteLength, true);
  u.set(p, 13);
  host.enqueueData(buf);
}

const EMPTY = new Float32Array([0, 0, 0, 0]);

function sendLine(bid: number, x0: number, y0: number, x1: number, y1: number) {
  sendF(bid, new Float32Array([x0, y0, x1, y1]));
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 6. SCENE BUILDER — interprets CHART_DOC.scene
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function buildScene(doc: ChartDocument) {
  // Transforms
  for (const t of doc.scene.transforms) {
    must(host.applyControl({ cmd: "createTransform", id: t.id }));
    must(host.applyControl({ cmd: "setTransform", id: t.id, tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy }));
  }

  // Buffers
  for (const b of doc.scene.buffers) {
    must(host.applyControl({ cmd: "createBuffer", id: b.id }));
  }

  // Geometries
  for (const g of doc.scene.geometries) {
    if (g.kind === "vertex") {
      must(host.applyControl({ cmd: "createGeometry", id: g.id, vertexBufferId: g.vertexBufferId, format: g.format, strideBytes: g.strideBytes }));
    } else {
      must(host.applyControl({ cmd: "createInstancedGeometry", id: g.id, instanceBufferId: g.instanceBufferId, instanceFormat: g.instanceFormat, instanceStrideBytes: g.instanceStrideBytes }));
    }
  }

  // DrawItems
  for (const di of doc.scene.drawItems) {
    must(host.applyControl({ cmd: "createDrawItem", id: di.id, geometryId: di.geometryId, pipeline: di.pipeline }));
    must(host.applyControl({ cmd: "attachTransform", targetId: di.id, transformId: di.transformId }));
  }
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 7. THEME APPLICATOR — maps theme colors to draw items by role
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

let currentTheme: string = "dark";

function applyTheme(themeKey: string) {
  const theme = CHART_DOC.themes[themeKey];
  if (!theme) return;
  currentTheme = themeKey;

  // Map roles to colors
  for (const di of CHART_DOC.scene.drawItems) {
    switch (di.role) {
      case "candle":
        must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, colorUp: theme.candleUp, colorDown: theme.candleDown }));
        break;
      case "volume":
        must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: theme.volume }));
        break;
      case "gridPrice":
      case "gridVolume": {
        const a = theme.gridColor[3] * theme.gridOpacity;
        must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: [theme.gridColor[0], theme.gridColor[1], theme.gridColor[2], a] }));
        break;
      }
      case "divider":
        must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: theme.dividerColor }));
        break;
      case "crosshairV":
      case "crosshairH":
        must(host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: theme.crosshairColor }));
        break;
    }
  }

  // GL clear color (WebGL background)
  host.applyControl({
    cmd: "setClearColor",
    r: theme.background[0], g: theme.background[1],
    b: theme.background[2], a: theme.background[3],
  });

  // CSS custom properties for UI overlays
  const root = document.documentElement;
  const bg = theme.background;
  root.style.setProperty("--bg", `rgb(${(bg[0]*255)|0},${(bg[1]*255)|0},${(bg[2]*255)|0})`);
  root.style.setProperty("--fg", theme.fg);
  root.style.setProperty("--fg-muted", theme.fgMuted);
  root.style.setProperty("--panel-bg", theme.panelBg);
  root.style.setProperty("--accent", theme.accent);
  root.style.setProperty("--accent-border", theme.accentBorder);

  // Update label
  themeLabel.textContent = `Theme: ${theme.name}`;

  // Update button states
  themeBtns.forEach(btn => {
    btn.classList.toggle("active", btn.dataset.theme === themeKey);
  });

  // Re-push grid lines with current theme (grid data depends on theme dash settings)
  pushGridLines();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 8. DATA PUSHERS
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

// Transform state
let sx = 1, tx = 0;
let syP = CHART_DOC.scene.transforms[0].sy, tyP = CHART_DOC.scene.transforms[0].ty;
let syV = CHART_DOC.scene.transforms[1].sy, tyV = CHART_DOC.scene.transforms[1].ty;
const T_PR = CHART_DOC.scene.transforms[0].id;
const T_VL = CHART_DOC.scene.transforms[1].id;
const INIT = { sx: 1, tx: 0, syP, tyP, syV, tyV };

function syncTx() {
  host.applyControl({ cmd: "setTransform", id: T_PR, tx, ty: tyP, sx, sy: syP });
  host.applyControl({ cmd: "setTransform", id: T_VL, tx, ty: tyV, sx, sy: syV });
}

function pushCandles() {
  const f = new Float32Array(N * 6);
  for (let i = 0; i < N; i++) {
    const c = candles[i], o = i * 6;
    f[o] = c.x; f[o+1] = c.o; f[o+2] = c.h;
    f[o+3] = c.l; f[o+4] = c.c; f[o+5] = c.hw;
  }
  sendF(100, f);
  barsEl.textContent = String(N);
}

function pushVolume() {
  const f = new Float32Array(N * 4);
  for (let i = 0; i < N; i++) {
    const c = candles[i];
    const body = Math.abs(c.c - c.o);
    const h = 0.05 + body * 3;
    f[i*4] = c.x - c.hw; f[i*4+1] = -1;
    f[i*4+2] = c.x + c.hw; f[i*4+3] = -1 + h;
  }
  sendF(110, f);
}

function pushGridLines() {
  // Price panel grid: 7 horizontal lines spanning data range
  const gridCount = 7;
  const priceGrid = new Float32Array(gridCount * 4);
  for (let i = 0; i < gridCount; i++) {
    const y = -0.9 + (1.8 * i) / (gridCount - 1);
    priceGrid[i*4] = -1; priceGrid[i*4+1] = y;
    priceGrid[i*4+2] = 1;  priceGrid[i*4+3] = y;
  }
  sendF(120, priceGrid);

  // Volume panel grid: 4 horizontal lines
  const volGridCount = 4;
  const volGrid = new Float32Array(volGridCount * 4);
  for (let i = 0; i < volGridCount; i++) {
    const y = -0.9 + (1.8 * i) / (volGridCount - 1);
    volGrid[i*4] = -1; volGrid[i*4+1] = y;
    volGrid[i*4+2] = 1;  volGrid[i*4+3] = y;
  }
  sendF(130, volGrid);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 9. INTERACTION (pan / zoom / crosshair / OHLC)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

function c2ndc(cx: number, cy: number) {
  const r = canvas.getBoundingClientRect();
  return {
    nx: ((cx - r.left) / Math.max(1, r.width)) * 2 - 1,
    ny: 1 - ((cy - r.top) / Math.max(1, r.height)) * 2,
  };
}
function isTop(ny: number) { return ny >= 0; }

function updateOhlc(nx: number) {
  const dx = (nx - tx) / Math.max(0.001, sx);
  let best = 0, bd = Infinity;
  for (let i = 0; i < N; i++) {
    const d = Math.abs(candles[i].x - dx);
    if (d < bd) { bd = d; best = i; }
  }
  const c = candles[best];
  valO.textContent = c.o.toFixed(4);
  valH.textContent = c.h.toFixed(4);
  valL.textContent = c.l.toFixed(4);
  valC.textContent = c.c.toFixed(4);
}

let dragging = false;
let lastX = 0, lastY = 0;

canvas.addEventListener("mousedown", (e) => {
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
  sendLine(210, nx, -1, nx, 1);
  sendLine(220, -1, ny, 1, ny);
  updateOhlc(nx);

  if (!dragging) return;

  const rect = canvas.getBoundingClientRect();
  const dxPx = e.clientX - lastX, dyPx = e.clientY - lastY;
  lastX = e.clientX; lastY = e.clientY;
  tx += (dxPx / Math.max(1, rect.width)) * 2;
  if (isTop(ny)) tyP += -(dyPx / Math.max(1, rect.height)) * 2;
  else tyV += -(dyPx / Math.max(1, rect.height)) * 2;
  syncTx();
});

canvas.addEventListener("wheel", (e) => {
  e.preventDefault();
  const { ny } = c2ndc(e.clientX, e.clientY);
  const z = e.deltaY < 0 ? 1.1 : 1 / 1.1;
  sx = Math.max(0.1, Math.min(50, sx * z));
  if (isTop(ny)) syP = Math.max(0.1, Math.min(50, syP * z));
  else syV = Math.max(0.1, Math.min(50, syV * z));
  syncTx();
}, { passive: false });

canvas.addEventListener("dblclick", () => {
  sx = INIT.sx; tx = INIT.tx;
  syP = INIT.syP; tyP = INIT.tyP;
  syV = INIT.syV; tyV = INIT.tyV;
  syncTx();
});

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 10. THEME SWITCHING (buttons + keyboard)
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const themeKeys = Object.keys(CHART_DOC.themes);

themeBtns.forEach(btn => {
  btn.addEventListener("click", () => {
    const key = btn.dataset.theme;
    if (key && CHART_DOC.themes[key]) applyTheme(key);
  });
});

window.addEventListener("keydown", (e) => {
  const idx = parseInt(e.key) - 1;
  if (idx >= 0 && idx < themeKeys.length) {
    applyTheme(themeKeys[idx]);
  }
  if (e.key === "r" || e.key === "R") {
    canvas.dispatchEvent(new MouseEvent("dblclick"));
  }
});

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// 11. INIT — build scene from document, push data, apply default theme
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

buildScene(CHART_DOC);
pushCandles();
pushVolume();
sendLine(200, -1, 0, 1, 0); // divider
applyTheme("dark");
canvas.style.cursor = "grab";

console.log(
  `DynaCharting Theme Showcase: ${N} candles, ${themeKeys.length} themes (${themeKeys.join(", ")})`
);

(globalThis as any).__host = host;
(globalThis as any).__doc = CHART_DOC;
