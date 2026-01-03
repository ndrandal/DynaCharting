import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

const canvas = document.getElementById("c") as HTMLCanvasElement;

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);

// NEW: register a pickable triangle (clip/NDC coords in [-1..1])
const TRI_ID = 42;
host.setPickableTriangle(
  TRI_ID,
  new Float32Array([
    -0.6, -0.5,
     0.6, -0.5,
     0.0,  0.6
  ])
);

host.start();

// Proof: uploaded bytes changes when buffers update.
// Every 500ms we upload a different sized buffer.
let flip = false;
setInterval(() => {
  flip = !flip;
  const n = flip ? 256 : 2048; // floats
  const arr = new Float32Array(n);
  for (let i = 0; i < arr.length; i++) arr[i] = Math.random();
  host.uploadBuffer(1, arr); // same buffer id, new content
}, 500);

// NEW: click -> pick
canvas.addEventListener("click", (e) => {
  const rect = canvas.getBoundingClientRect();
  const x = e.clientX - rect.left;
  const y = e.clientY - rect.top;

  const hit = host.pick(x, y);
  const id = hit ? hit.drawItemId : null;

  hud.setPick(id);
  console.log("pick:", hit);
});

// Optional toggles for manual testing:
(globalThis as any).__host = host;
