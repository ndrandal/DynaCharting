/// <reference lib="webworker" />

// Generates fake high-rate data and sends binary data-plane batches as Transferables.
// Record format must match EngineHost's data-plane parser:
// [u8 op][u32 bufferId][u32 offsetOrUnused][u32 byteLen][payload bytes...]

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

type WorkerMsg =
  | { type: "start"; bufferId: number; pointsPerSec?: number; batchPoints?: number }
  | { type: "stop" };

let running = false;
let timer: number | null = null;

function u32(view: DataView, off: number, v: number) {
  view.setUint32(off, v >>> 0, true);
}

function makeAppendBatch(bufferId: number, floats: Float32Array): ArrayBuffer {
  const payload = new Uint8Array(floats.buffer, floats.byteOffset, floats.byteLength);
  const headerBytes = 1 + 4 + 4 + 4; // op + bufferId + offset + byteLen
  const out = new ArrayBuffer(headerBytes + payload.byteLength);
  const view = new DataView(out);
  const bytes = new Uint8Array(out);

  bytes[0] = OP_APPEND;
  u32(view, 1, bufferId);
  u32(view, 5, 0); // unused for append
  u32(view, 9, payload.byteLength);

  bytes.set(payload, headerBytes);
  return out;
}

function tick(bufferId: number, batchPoints: number) {
  // Each point is vec2 position in clip-space (x,y)
  const floats = new Float32Array(batchPoints * 2);

  // Simple moving wave; deterministic enough for testing
  const t = performance.now() * 0.001;
  for (let i = 0; i < batchPoints; i++) {
    const x = -1 + (2 * i) / Math.max(1, batchPoints - 1);
    const y = 0.25 * Math.sin(6 * x + t);
    floats[i * 2 + 0] = x;
    floats[i * 2 + 1] = y;
  }

  const batch = makeAppendBatch(bufferId, floats);

  // Transfer ownership of the ArrayBuffer (no copy)
  (self as any).postMessage({ type: "data", batch }, [batch]);
}

self.onmessage = (ev: MessageEvent<WorkerMsg>) => {
  const msg = ev.data;

  if (msg.type === "stop") {
    running = false;
    if (timer !== null) {
      clearInterval(timer);
      timer = null;
    }
    return;
  }

  if (msg.type === "start") {
    const bufferId = msg.bufferId >>> 0;
    const pointsPerSec = Math.max(1, msg.pointsPerSec ?? 10_000);
    const batchPoints = Math.max(32, msg.batchPoints ?? 512);

    // interval so that batchPoints * (1000/interval) ~= pointsPerSec
    const intervalMs = Math.max(5, Math.floor((1000 * batchPoints) / pointsPerSec));

    running = true;
    if (timer !== null) clearInterval(timer);

    timer = setInterval(() => {
      if (!running) return;
      tick(bufferId, batchPoints);
    }, intervalMs) as unknown as number;
  }
};
