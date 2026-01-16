// apps/demos/hello-engine/src/main.ts
import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

import { compileScene } from "./compiler/compileScene";
import type { SceneSpecV0 } from "./compiler/SceneSpec";

const WORKER_URL = new URL("./ingest.worker.ts", import.meta.url);

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("Canvas #c not found");

const hud = makeHud();
const host = new EngineHost(hud);
host.init(canvas);
host.start();

const worker = new Worker(WORKER_URL, { type: "module" });

worker.onmessage = (e: MessageEvent<any>) => {
  const msg = e.data;
  if (msg?.type === "batch" && msg.buffer instanceof ArrayBuffer) {
    host.enqueueData(msg.buffer);
  }
};

function must(r: { ok: true } | { ok: false; error: string }) {
  if (!r.ok) throw new Error(r.error);
}

// -------------------- D5 spec: domain space + axis overlay --------------------
const spec: SceneSpecV0 = {
  version: 0,
  name: "d5-domain+axes",

  spaces: [
    { id: "domain", type: "domain2d", domain: { xMin: 0, xMax: 100, yMin: -1.2, yMax: 1.2 } },
    { id: "screen", type: "screen01" }
  ],

  views: [
    { id: "main", rect: { x: 0.06, y: 0.06, w: 0.90, h: 0.88, units: "relative" } }
  ],

  layers: [
    // Data in domain coords
    { id: "line", viewId: "main", spaceId: "domain", data: { kind: "workerStream", stream: "lineSine" },  mark: { kind: "lineStrip" } },
    { id: "pts",  viewId: "main", spaceId: "domain", data: { kind: "workerStream", stream: "pointsCos" }, mark: { kind: "points" } },

    // Axes overlay in screen space (0..1)
    { id: "xAxis", viewId: "main", spaceId: "screen", data: { kind: "none" }, mark: { kind: "axis2d", axis: { side: "bottom", ticks: 7, tickLen: 0.035, inset: 0.02 } } },
    { id: "yAxis", viewId: "main", spaceId: "screen", data: { kind: "none" }, mark: { kind: "axis2d", axis: { side: "left",   ticks: 7, tickLen: 0.035, inset: 0.02 } } },
  ]
};

type Mounted = {
  plan: ReturnType<typeof compileScene>["plan"];
  runtime: ReturnType<typeof compileScene>["runtime"];
};

let mounted: Mounted | null = null;

// View state (sceneView transform)
let dragging = false;
let lastX = 0, lastY = 0;
let viewTx = 0, viewTy = 0, viewSx = 1, viewSy = 1;

// -------------------- interaction --------------------
const onMouseDown = (e: MouseEvent) => {
  dragging = true;
  lastX = e.clientX;
  lastY = e.clientY;
};

const onMouseUp = () => (dragging = false);

const onMouseMove = (e: MouseEvent) => {
  if (!dragging || !mounted) return;

  const rect = canvas.getBoundingClientRect();
  const dxPx = e.clientX - lastX;
  const dyPx = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;

  const dxClip = (dxPx / Math.max(1, rect.width)) * 2;
  const dyClip = -(dyPx / Math.max(1, rect.height)) * 2;

  viewTx += dxClip;
  viewTy += dyClip;

  mounted.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
};

const onWheel = (e: WheelEvent) => {
  if (!mounted) return;
  e.preventDefault();

  const zoom = Math.exp(-e.deltaY * 0.001);
  viewSx *= zoom;
  viewSy *= zoom;

  mounted.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
};

const onKeyDown = (e: KeyboardEvent) => {
  if (e.key === "m" || e.key === "M") {
    if (mounted) unmount();
    else mount();
  }

  if (e.key === "r" || e.key === "R") {
    viewTx = 0; viewTy = 0; viewSx = 1; viewSy = 1;
    mounted?.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
  }

  // D5 demo: change domain at runtime
  if (e.key === "1") {
    mounted?.runtime.setDomain(host.applyControl.bind(host), "domain", { xMin: 0, xMax: 100, yMin: -1.2, yMax: 1.2 });
    console.log("Domain preset 1");
  }
  if (e.key === "2") {
    mounted?.runtime.setDomain(host.applyControl.bind(host), "domain", { xMin: 0, xMax: 50, yMin: -0.6, yMax: 0.6 });
    console.log("Domain preset 2");
  }
  if (e.key === "3") {
    mounted?.runtime.setDomain(host.applyControl.bind(host), "domain", { xMin: 25, xMax: 75, yMin: -1.0, yMax: 1.0 });
    console.log("Domain preset 3");
  }
};

function mount() {
  if (mounted) return;

  const compiled = compileScene(spec, { idBase: 10000 });

  // Apply create commands with rollback on failure
  const r = compiled.plan.mount(host.applyControl.bind(host));
  must(r);

  // One-time static data (axes are generated here too)
  for (const b of compiled.plan.initialBatches) host.enqueueData(b);

  // Start worker streams
  worker.postMessage({
    type: "startStreams",
    tickMs: 33,
    streams: compiled.plan.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
  });

  // Init view + domain
  compiled.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
  compiled.runtime.setDomain(host.applyControl.bind(host), "domain", { xMin: 0, xMax: 100, yMin: -1.2, yMax: 1.2 });

  mounted = { plan: compiled.plan, runtime: compiled.runtime };
  console.log("Mounted plan:", spec.name ?? "(unnamed)");
}

function unmount() {
  if (!mounted) return;

  // stop streams
  worker.postMessage({ type: "stop" });

  // dispose engine objects safely
  mounted.plan.unmount(host.applyControl.bind(host));
  mounted = null;

  console.log("Unmounted");
}

// Attach listeners once
canvas.addEventListener("mousedown", onMouseDown);
window.addEventListener("mouseup", onMouseUp);
window.addEventListener("mousemove", onMouseMove);
canvas.addEventListener("wheel", onWheel, { passive: false });
window.addEventListener("keydown", onKeyDown);

// Auto-mount at start
mount();

// Final cleanup hook
function cleanup() {
  try { unmount(); } catch {}
  canvas.removeEventListener("mousedown", onMouseDown);
  window.removeEventListener("mouseup", onMouseUp);
  window.removeEventListener("mousemove", onMouseMove);
  canvas.removeEventListener("wheel", onWheel as any);
  window.removeEventListener("keydown", onKeyDown);

  try { worker.terminate(); } catch {}
  try { host.shutdown(); } catch {}

  (globalThis as any).__host = null;
  (globalThis as any).__worker = null;
}

(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

if (import.meta.hot) {
  import.meta.hot.dispose(() => cleanup());
}

console.log("D5: domain2d + screen01 axes overlay + runtime setDomain(). Keys: M toggle, R reset, 1/2/3 change domain.");
