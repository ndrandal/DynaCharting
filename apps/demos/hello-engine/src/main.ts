import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

const canvas = document.getElementById("c") as HTMLCanvasElement;

const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);
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

// Optional toggles for manual testing:
(globalThis as any).__host = host;
