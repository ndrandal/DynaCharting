import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

// If your worker file is named differently, update this path.
const WORKER_URL = new URL("./ingest.worker.ts", import.meta.url);

const canvas = document.getElementById("c") as HTMLCanvasElement;
if (!canvas) throw new Error("Canvas #c not found");

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);

// ------------------------------------------------------------
// Control plane setup (JSON-adjacent)
// One buffer (id=1) feeds one geometry (id=10) and one draw item (id=42).
// The worker will append vertices into buffer 1.
// ------------------------------------------------------------
{
  const r1 = host.applyControl({ cmd: "createBuffer", id: 1 });
  if (!r1.ok) throw new Error(r1.error);

  const r2 = host.applyControl({ cmd: "createGeometry", id: 10, vertexBufferId: 1, format: "pos2_clip" });
  if (!r2.ok) throw new Error(r2.error);

  const r3 = host.applyControl({ cmd: "createDrawItem", id: 42, geometryId: 10, pipeline: "flat" });
  if (!r3.ok) throw new Error(r3.error);
}

host.start();

// ------------------------------------------------------------
// Worker: high-rate fake data (Transferables)
// Main thread MUST NOT parse data here.
// ------------------------------------------------------------
const worker = new Worker(WORKER_URL, { type: "module" });

worker.onmessage = (e: MessageEvent<any>) => {
  const msg = e.data;
  if (!msg) return;

  // Expected: { type:"batch", buffer:ArrayBuffer }
  if (msg.type === "batch" && msg.buffer instanceof ArrayBuffer) {
    // HOT LOOP RULE: do not parse. Just enqueue.
    host.enqueueData(msg.buffer);
    return;
  }

  if (msg.type === "log") {
    console.log("[worker]", msg.message);
    return;
  }
};

worker.onerror = (err) => {
  console.error("Worker error:", err);
};

// Start fake stream: 10k vertices/sec, ~10ms batches
worker.postMessage({
  type: "start",
  bufferId: 1,
  vertsPerSec: 10_000,
  batchMs: 10
});

// ------------------------------------------------------------
// Interaction: click-to-pick (D1.4 stays alive)
// ------------------------------------------------------------
canvas.addEventListener("click", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const hit = host.pick(x, y);
  const id = hit ? hit.drawItemId : null;

  // If your HUD implements setPick, show it
  (hud as any).setPick?.(id);

  console.log("pick:", hit);
});

// ------------------------------------------------------------
// Debug toggles (optional, but useful while iterating)
// B = bounds toggle, W = wireframe toggle
// ------------------------------------------------------------
window.addEventListener("keydown", (e) => {
  if (e.key === "b" || e.key === "B") {
    const s = host.getStats();
    host.setDebugToggles({ showBounds: !s.debug.showBounds });
  }
  if (e.key === "w" || e.key === "W") {
    const s = host.getStats();
    host.setDebugToggles({ wireframe: !s.debug.wireframe });
  }
});

// ------------------------------------------------------------
// Dev hooks
// ------------------------------------------------------------
(globalThis as any).__host = host;
(globalThis as any).__worker = worker;

console.log("Demo running. Try clicking geometry for pick(), press B/W for debug toggles.");
