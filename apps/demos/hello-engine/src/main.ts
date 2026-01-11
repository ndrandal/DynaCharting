// apps/demos/hello-engine/src/main.ts
import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

import { compileScene } from "./compiler/compileScene";
import type { SceneSpecV0 } from "./compiler/SceneSpec";
import type { CompiledPlan } from "./compiler/SceneSpec";

const WORKER_URL = new URL("./ingest.worker.ts", import.meta.url);

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("Canvas #c not found");

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);
host.start();

// -------------------- Worker --------------------
const worker = new Worker(WORKER_URL, { type: "module" });

worker.onmessage = (e: MessageEvent<any>) => {
  const msg = e.data;
  if (!msg) return;
  if (msg.type === "batch" && msg.buffer instanceof ArrayBuffer) {
    host.enqueueData(msg.buffer);
  }
};

function must(r: { ok: true } | { ok: false; error: string }) {
  if (!r.ok) throw new Error(r.error);
}

// -------------------- D2: SceneSpecV0 (spaces + views + layers) --------------------
const spec: SceneSpecV0 = {
  version: 0,
  name: "d2-line+points",
  spaces: [
    { id: "clip", type: "clip" }
    // You can also try:
    // { id: "scr", type: "screen01" }
    // { id: "dom", type: "domain2d", domain: { xMin: -1, xMax: 1, yMin: -1, yMax: 1 } }
  ],
  views: [
    { id: "main", rect: { x: 0, y: 0, w: 1, h: 1, units: "relative" } }
  ],
  layers: [
    {
      id: "line",
      viewId: "main",
      spaceId: "clip",
      data: { kind: "workerStream", stream: "lineSine" },
      mark: { kind: "lineStrip", pipeline: "line2d@1" }
    },
    {
      id: "points",
      viewId: "main",
      spaceId: "clip",
      data: { kind: "workerStream", stream: "pointsCos" },
      mark: { kind: "points", pipeline: "points@1", pointSize: 2 }
    }
  ]
};

// -------------------- Plan mount/unmount --------------------
let mounted: CompiledPlan | null = null;

function mountPlan() {
  if (mounted) return;

  const plan = compileScene(spec, { idBase: 10000 });

  for (const c of plan.commands) must(host.applyControl(c));

  // One-time static batches (if any)
  for (const b of plan.initialBatches) host.enqueueData(b);

  // Start worker streams for subscriptions
  worker.postMessage({
    type: "startStreams",
    tickMs: 33,
    streams: plan.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
  });

  mounted = plan;

  // init view state (identity)
  mounted.runtime.setView(host.applyControl.bind(host), { tx: 0, ty: 0, sx: 1, sy: 1 });

  console.log("Mounted:", spec.name ?? "(unnamed)");
}

function unmountPlan() {
  if (!mounted) return;

  worker.postMessage({ type: "stop" });

  for (const c of mounted.dispose) {
    const r = host.applyControl(c);
    if (!r.ok) console.warn("dispose warn:", r.error, c);
  }

  mounted = null;
  console.log("Unmounted");
}

mountPlan();

// -------------------- Interaction: drag pan in scene-view space --------------------
let dragging = false;
let lastX = 0;
let lastY = 0;

let viewTx = 0;
let viewTy = 0;
let viewSx = 1;
let viewSy = 1;

canvas.addEventListener("mousedown", (e) => {
  dragging = true;
  lastX = e.clientX;
  lastY = e.clientY;
});

window.addEventListener("mouseup", () => (dragging = false));

window.addEventListener("mousemove", (e) => {
  if (!dragging || !mounted) return;

  const rect = canvas.getBoundingClientRect();
  const dxPx = e.clientX - lastX;
  const dyPx = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;

  // px -> clip delta
  const dxClip = (dxPx / Math.max(1, rect.width)) * 2;
  const dyClip = -(dyPx / Math.max(1, rect.height)) * 2;

  viewTx += dxClip;
  viewTy += dyClip;

  mounted.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
});

// -------------------- Hotkeys --------------------
window.addEventListener("keydown", (e) => {
  if (e.key === "m" || e.key === "M") {
    if (mounted) unmountPlan();
    else mountPlan();
  }

  if (e.key === "r" || e.key === "R") {
    viewTx = 0; viewTy = 0; viewSx = 1; viewSy = 1;
    if (mounted) mounted.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
  }
});

(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

console.log("D2 complete: spaces/views/layers compiler. Press M to mount/unmount, drag to pan.");
