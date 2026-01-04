import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

const WORKER_URL = new URL("./ingest.worker.ts", import.meta.url);

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("Canvas #c not found");

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);

// -------------------- Control plane demo scene for D2.2 --------------------
// Buffers:
//  1: line2d vertices (vec2)        stride 8
//  2: instancedRect instances (vec4) stride 16
//  3: instancedCandle instances (6 floats) stride 24
//  4: points vertices (vec2)        stride 8
const BUF_LINE = 1;
const BUF_RECT = 2;
const BUF_CANDLE = 3;
const BUF_POINTS = 4;

// Geometry IDs
const GEO_LINE = 10;
const GEO_RECT = 11;
const GEO_CANDLE = 12;
const GEO_POINTS = 13;

// DrawItem IDs
const DI_LINE = 100;
const DI_RECT = 101;
const DI_CANDLE = 102;
const DI_POINTS = 103;

// -------------------- D1.5: Transform resource --------------------
const T_VIEW = 2000;

function must(r: { ok: true } | { ok: false; error: string }) {
  if (!r.ok) throw new Error(r.error);
}

must(host.applyControl({ cmd: "createBuffer", id: BUF_LINE }));
must(host.applyControl({ cmd: "createBuffer", id: BUF_RECT }));
must(host.applyControl({ cmd: "createBuffer", id: BUF_CANDLE }));
must(host.applyControl({ cmd: "createBuffer", id: BUF_POINTS }));

// Vertex geometries (pos2_clip)
must(host.applyControl({ cmd: "createGeometry", id: GEO_LINE, vertexBufferId: BUF_LINE, format: "pos2_clip", strideBytes: 8 }));
must(host.applyControl({ cmd: "createGeometry", id: GEO_POINTS, vertexBufferId: BUF_POINTS, format: "pos2_clip", strideBytes: 8 }));

// Instanced geometries
must(host.applyControl({
  cmd: "createInstancedGeometry",
  id: GEO_RECT,
  instanceBufferId: BUF_RECT,
  instanceFormat: "rect4",
  instanceStrideBytes: 16
}));

must(host.applyControl({
  cmd: "createInstancedGeometry",
  id: GEO_CANDLE,
  instanceBufferId: BUF_CANDLE,
  instanceFormat: "candle6",
  instanceStrideBytes: 24
}));

// Draw items
must(host.applyControl({ cmd: "createDrawItem", id: DI_LINE, geometryId: GEO_LINE, pipeline: "line2d@1" }));
must(host.applyControl({ cmd: "createDrawItem", id: DI_RECT, geometryId: GEO_RECT, pipeline: "instancedRect@1" }));
must(host.applyControl({ cmd: "createDrawItem", id: DI_CANDLE, geometryId: GEO_CANDLE, pipeline: "instancedCandle@1" }));
must(host.applyControl({ cmd: "createDrawItem", id: DI_POINTS, geometryId: GEO_POINTS, pipeline: "points@1" }));

// -------------------- D1.5: create + attach a shared view transform --------------------
must(host.applyControl({ cmd: "createTransform", id: T_VIEW }));
must(host.applyControl({ cmd: "setTransform", id: T_VIEW, tx: 0, ty: 0, sx: 1, sy: 1 }));

must(host.applyControl({ cmd: "attachTransform", targetId: DI_LINE, transformId: T_VIEW }));
must(host.applyControl({ cmd: "attachTransform", targetId: DI_RECT, transformId: T_VIEW }));
must(host.applyControl({ cmd: "attachTransform", targetId: DI_CANDLE, transformId: T_VIEW }));
must(host.applyControl({ cmd: "attachTransform", targetId: DI_POINTS, transformId: T_VIEW }));

host.start();

// -------------------- Worker stream --------------------
const worker = new Worker(WORKER_URL, { type: "module" });

worker.onmessage = (e: MessageEvent<any>) => {
  const msg = e.data;
  if (!msg) return;

  if (msg.type === "batch" && msg.buffer instanceof ArrayBuffer) {
    host.enqueueData(msg.buffer);
    return;
  }
};

worker.onerror = (err) => {
  console.error("Worker error:", err);
};

worker.postMessage({
  type: "startRecipes",
  lineBufferId: BUF_LINE,
  rectBufferId: BUF_RECT,
  candleBufferId: BUF_CANDLE,
  pointsBufferId: BUF_POINTS,
  tickMs: 33
});

// -------------------- D1.5: Interaction — drag to pan (uniform-only) --------------------
let dragging = false;
let lastX = 0;
let lastY = 0;

// Keep transform params locally (no readback API needed yet)
let tx = 0, ty = 0, sx = 1, sy = 1;

canvas.addEventListener("mousedown", (e) => {
  dragging = true;
  lastX = e.clientX;
  lastY = e.clientY;
});

window.addEventListener("mouseup", () => { dragging = false; });

window.addEventListener("mousemove", (e) => {
  if (!dragging) return;

  const rect = canvas.getBoundingClientRect();
  const dxPx = e.clientX - lastX;
  const dyPx = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;

  // Pixels -> clip space deltas
  const dxClip = (dxPx / Math.max(1, rect.width)) * 2;
  const dyClip = -(dyPx / Math.max(1, rect.height)) * 2;

  tx += dxClip;
  ty += dyClip;

  // Pass criteria: only transform changes (no buffer updates)
  host.applyControl({ cmd: "setTransform", id: T_VIEW, tx, ty, sx, sy });
});

// Optional: scroll wheel zoom around center (still uniform-only)
// (Comment out if you want ONLY pan right now.)
canvas.addEventListener("wheel", (e) => {
  e.preventDefault();

  const zoom = Math.exp(-e.deltaY * 0.001); // smooth zoom
  sx *= zoom;
  sy *= zoom;

  host.applyControl({ cmd: "setTransform", id: T_VIEW, tx, ty, sx, sy });
}, { passive: false });

// -------------------- Interaction: click-to-pick --------------------
// pick currently only works for triSolid@1 in this EngineHost, so clicking won’t hit these.
// Leave the hook in place (still useful later).
canvas.addEventListener("click", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const hit = host.pick(x, y);
  const id = hit ? hit.drawItemId : null;
  (hud as any).setPick?.(id);

  console.log("pick:", hit);
});

// Debug toggles
window.addEventListener("keydown", (e) => {
  if (e.key === "b" || e.key === "B") {
    const s = host.getStats();
    host.setDebugToggles({ showBounds: !s.debug.showBounds });
  }
  if (e.key === "w" || e.key === "W") {
    const s = host.getStats();
    host.setDebugToggles({ wireframe: !s.debug.wireframe });
  }

  // quick reset view
  if (e.key === "r" || e.key === "R") {
    tx = 0; ty = 0; sx = 1; sy = 1;
    host.applyControl({ cmd: "setTransform", id: T_VIEW, tx, ty, sx, sy });
  }
});

// Dev hooks
(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

console.log("D2.2 + D1.5 demo running: drag pans via u_transform only (buffers unchanged)");
