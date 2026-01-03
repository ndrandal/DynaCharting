import { EngineHost } from "@repo/engine-host";
import { makeHud } from "./hud";

// ----------------- binary helpers (D3.1) -----------------

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

function buildBatch(records: Array<{
  op: number;
  bufferId: number;
  offsetBytes?: number;
  payload: Uint8Array;
}>): ArrayBuffer {
  let total = 0;
  for (const r of records) {
    total += 1 + 4 + 4 + 4 + r.payload.byteLength;
  }

  const ab = new ArrayBuffer(total);
  const dv = new DataView(ab);
  const u8 = new Uint8Array(ab);

  let p = 0;
  for (const r of records) {
    dv.setUint8(p, r.op); p += 1;
    dv.setUint32(p, r.bufferId >>> 0, true); p += 4;
    dv.setUint32(p, (r.offsetBytes ?? 0) >>> 0, true); p += 4;
    dv.setUint32(p, r.payload.byteLength >>> 0, true); p += 4;
    u8.set(r.payload, p); p += r.payload.byteLength;
  }
  return ab;
}

function f32Payload(arr: Float32Array): Uint8Array {
  return new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength);
}

// ----------------- demo -----------------

const canvas = document.getElementById("c") as HTMLCanvasElement;
const hud = makeHud();
const host = new EngineHost(hud);

host.init(canvas);

// Control plane (JSON)
host.applyControl({ cmd: "createBuffer", id: 1 });
host.applyControl({ cmd: "createGeometry", id: 10, vertexBufferId: 1, format: "pos2_clip" });
host.applyControl({ cmd: "createDrawItem", id: 42, geometryId: 10, pipeline: "flat" });

host.start();

// Start with one triangle (3 vertices in clip space)
const tri0 = new Float32Array([
  -0.6, -0.5,
   0.0,  0.6,
   0.6, -0.5
]);

host.applyDataBatch(buildBatch([
  { op: OP_APPEND, bufferId: 1, payload: f32Payload(tri0) }
]));

// Append another triangle every 700ms so geometry extends.
// Each append adds 3 more vertices => another triangle rendered.
let k = 0;
setInterval(() => {
  k++;

  // march triangles to the right, slightly smaller
  const x = -0.2 + 0.25 * (k % 5);
  const y = -0.3;
  const s = 0.25;

  const tri = new Float32Array([
    x - s, y - s,
    x,     y + s,
    x + s, y - s
  ]);

  const batch = buildBatch([
    { op: OP_APPEND, bufferId: 1, payload: f32Payload(tri) }
  ]);

  host.applyDataBatch(batch);
}, 700);

// Optional: demonstrate updateRange (wiggle the very first vertex)
// This proves the opcode exists, but pass criteria only needs append->extend.
setInterval(() => {
  const t = performance.now() * 0.001;
  const vx = -0.6 + Math.sin(t) * 0.05;
  const vy = -0.5 + Math.cos(t) * 0.05;
  const update = new Float32Array([vx, vy]);

  const batch = buildBatch([
    { op: OP_UPDATE_RANGE, bufferId: 1, offsetBytes: 0, payload: f32Payload(update) }
  ]);

  host.applyDataBatch(batch);
}, 60);

(globalThis as any).__host = host;
