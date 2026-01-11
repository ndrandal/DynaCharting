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

// -------------------- D2 spec (example) --------------------
const spec: SceneSpecV0 = {
  version: 0,
  name: "d3-stable",
  spaces: [{ id: "clip", type: "clip" }],
  views: [{ id: "main", rect: { x: 0, y: 0, w: 1, h: 1, units: "relative" } }],
  layers: [
    { id: "line", viewId: "main", spaceId: "clip", data: { kind: "workerStream", stream: "lineSine" }, mark: { kind: "lineStrip" } },
    { id: "pts",  viewId: "main", spaceId: "clip", data: { kind: "workerStream", stream: "pointsCos" }, mark: { kind: "points" } }
  ]
};

type Mounted = {
  plan: ReturnType<typeof compileScene>["plan"];
  runtime: ReturnType<typeof compileScene>["runtime"];
};

let mounted: Mounted | null = null;

// View state
let dragging = false;
let lastX = 0, lastY = 0;
let viewTx = 0, viewTy = 0, viewSx = 1, viewSy = 1;

// Handlers (so we can remove them)
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

const onKeyDown = (e: KeyboardEvent) => {
  if (e.key === "m" || e.key === "M") {
    if (mounted) unmount();
    else mount();
  }
  if (e.key === "r" || e.key === "R") {
    viewTx = 0; viewTy = 0; viewSx = 1; viewSy = 1;
    mounted?.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });
  }
};

function mount() {
  if (mounted) return;

  const compiled = compileScene(spec, { idBase: 10000 });

  // Apply create commands with rollback on failure
  const r = compiled.plan.mount(host.applyControl.bind(host));
  must(r);

  // One-time static data
  for (const b of compiled.plan.initialBatches) host.enqueueData(b);

  // Start worker streams
  worker.postMessage({
    type: "startStreams",
    tickMs: 33,
    streams: compiled.plan.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
  });

  // Init view
  compiled.runtime.setView(host.applyControl.bind(host), { tx: viewTx, ty: viewTy, sx: viewSx, sy: viewSy });

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

// Attach listeners once (and remove on cleanup)
canvas.addEventListener("mousedown", onMouseDown);
window.addEventListener("mouseup", onMouseUp);
window.addEventListener("mousemove", onMouseMove);
window.addEventListener("keydown", onKeyDown);

// Auto-mount at start
mount();

// Final cleanup hook (dev and future service embedding)
function cleanup() {
  try { unmount(); } catch {}
  canvas.removeEventListener("mousedown", onMouseDown);
  window.removeEventListener("mouseup", onMouseUp);
  window.removeEventListener("mousemove", onMouseMove);
  window.removeEventListener("keydown", onKeyDown);

  // If you want to be strict about worker lifetime in dev:
  try { worker.terminate(); } catch {}

  try { host.shutdown(); } catch {}
  (globalThis as any).__host = null;
  (globalThis as any).__worker = null;
}

(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

// Vite HMR safe cleanup (prevents stacked listeners/workers)
if (import.meta.hot) {
  import.meta.hot.dispose(() => cleanup());
}

console.log("D3: PlanHandle mount/unmount + clean teardown. Press M to toggle.");
