import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

import { buildLineChartRecipe } from "./recipes/lineRecipe";
import type { RecipeBuildResult } from "./recipes/types";

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
    return;
  }
};

worker.onerror = (err) => {
  console.error("Worker error:", err);
};

function must(r: { ok: true } | { ok: false; error: string }) {
  if (!r.ok) throw new Error(r.error);
}

// -------------------- Recipe mount/unmount --------------------
let mounted: RecipeBuildResult | null = null;

function mountRecipe() {
  if (mounted) return;

  // Stable ID range for this recipe instance
  const recipe = buildLineChartRecipe({ idBase: 10000 });

  // Apply create commands
  for (const c of recipe.commands) must(host.applyControl(c));

  // Start worker streams based on subscriptions
  worker.postMessage({
    type: "startStreams",
    tickMs: 33,
    streams: recipe.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
  });

  mounted = recipe;
  console.log("Recipe mounted: line chart");
}

function unmountRecipe() {
  if (!mounted) return;

  // Stop worker first so no more updates hit deleted buffers
  worker.postMessage({ type: "stop" });

  // Apply dispose commands
  for (const c of mounted.dispose) must(host.applyControl(c));

  mounted = null;
  console.log("Recipe unmounted: IDs cleaned");
}

// Start mounted by default
mountRecipe();

// -------------------- D1.5 interaction: drag pan (transform-only) --------------------
// This assumes the line recipe uses T_VIEW = idBase + 2000.
let dragging = false;
let lastX = 0;
let lastY = 0;
let tx = 0, ty = 0, sx = 1, sy = 1;

const T_VIEW = 10000 + 2000;

canvas.addEventListener("mousedown", (e) => {
  dragging = true;
  lastX = e.clientX;
  lastY = e.clientY;
});

window.addEventListener("mouseup", () => { dragging = false; });

window.addEventListener("mousemove", (e) => {
  if (!dragging) return;
  if (!mounted) return;

  const rect = canvas.getBoundingClientRect();
  const dxPx = e.clientX - lastX;
  const dyPx = e.clientY - lastY;
  lastX = e.clientX;
  lastY = e.clientY;

  const dxClip = (dxPx / Math.max(1, rect.width)) * 2;
  const dyClip = -(dyPx / Math.max(1, rect.height)) * 2;

  tx += dxClip;
  ty += dyClip;

  host.applyControl({ cmd: "setTransform", id: T_VIEW, tx, ty, sx, sy });
});

// -------------------- Pick hook (kept) --------------------
canvas.addEventListener("click", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const hit = host.pick(x, y);
  const id = hit ? hit.drawItemId : null;
  (hud as any).setPick?.(id);
  console.log("pick:", hit);
});

// -------------------- Hotkeys --------------------
// M toggles mount/unmount (pass criteria proof)
window.addEventListener("keydown", (e) => {
  if (e.key === "m" || e.key === "M") {
    if (mounted) unmountRecipe();
    else mountRecipe();
  }

  if (e.key === "r" || e.key === "R") {
    tx = 0; ty = 0; sx = 1; sy = 1;
    if (mounted) host.applyControl({ cmd: "setTransform", id: T_VIEW, tx, ty, sx, sy });
  }
});

// Dev hooks
(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

console.log("D7.1 running: recipes only (press M to mount/unmount)");
