/// <reference lib="webworker" />

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

type WorkerMsg =
  | {
      type: "startRecipes";
      // buffer ids
      lineBufferId: number;
      rectBufferId: number;
      candleBufferId: number;
      pointsBufferId: number;
      tickMs?: number;
    }
  | { type: "stop" };

let running = false;
let timer: number | null = null;

function u32(view: DataView, off: number, v: number) {
  view.setUint32(off, v >>> 0, true);
}

function writeUpdateRangeRecord(
  out: Uint8Array,
  view: DataView,
  cursor: number,
  bufferId: number,
  offsetBytes: number,
  payloadBytes: Uint8Array
): number {
  // header: u8 op + u32 bufferId + u32 offset + u32 len
  out[cursor] = OP_UPDATE_RANGE;
  u32(view, cursor + 1, bufferId);
  u32(view, cursor + 5, offsetBytes);
  u32(view, cursor + 9, payloadBytes.byteLength);
  out.set(payloadBytes, cursor + 13);
  return cursor + 13 + payloadBytes.byteLength;
}

function tick(cfg: {
  lineBufferId: number;
  rectBufferId: number;
  candleBufferId: number;
  pointsBufferId: number;
}) {
  const t = performance.now() * 0.001;

  // ---- line2d: segments (pairs) ----
  const lineVerts = 512; // must be even
  const line = new Float32Array(lineVerts * 2);
  for (let i = 0; i < lineVerts; i++) {
    const x = -1 + (2 * i) / Math.max(1, lineVerts - 1);
    const y = 0.35 * Math.sin(4 * x + t);
    line[i * 2 + 0] = x;
    line[i * 2 + 1] = y;
  }

  // ---- points: scatter ----
  const ptsCount = 256;
  const pts = new Float32Array(ptsCount * 2);
  for (let i = 0; i < ptsCount; i++) {
    const x = -1 + (2 * i) / Math.max(1, ptsCount - 1);
    const y = 0.15 * Math.cos(8 * x - t);
    pts[i * 2 + 0] = x;
    pts[i * 2 + 1] = y;
  }

  // ---- instancedRect: rect4 (x0,y0,x1,y1) ----
  const rectN = 80;
  const rect = new Float32Array(rectN * 4);
  for (let i = 0; i < rectN; i++) {
    const cx = -0.95 + (1.9 * i) / Math.max(1, rectN - 1);
    const w = 0.015;
    const h = 0.10 + 0.10 * (0.5 + 0.5 * Math.sin(t + i * 0.3));
    const x0 = cx - w;
    const x1 = cx + w;
    const y0 = -0.85;
    const y1 = y0 + h;
    rect[i * 4 + 0] = x0;
    rect[i * 4 + 1] = y0;
    rect[i * 4 + 2] = x1;
    rect[i * 4 + 3] = y1;
  }

  // ---- instancedCandle: candle6 (x,open,high,low,close,halfWidth) ----
  const candleN = 60;
  const candle = new Float32Array(candleN * 6);
  for (let i = 0; i < candleN; i++) {
    const x = -0.95 + (1.9 * i) / Math.max(1, candleN - 1);
    const base = -0.15 + 0.35 * Math.sin(t * 0.7 + i * 0.2);
    const open = base + 0.08 * Math.sin(t + i * 0.6);
    const close = base + 0.08 * Math.cos(t * 1.1 + i * 0.5);
    const high = Math.max(open, close) + 0.08 + 0.03 * Math.sin(t * 1.7 + i);
    const low = Math.min(open, close) - 0.08 - 0.03 * Math.cos(t * 1.3 + i);
    const hw = 0.012;

    const o = i * 6;
    candle[o + 0] = x;
    candle[o + 1] = open;
    candle[o + 2] = high;
    candle[o + 3] = low;
    candle[o + 4] = close;
    candle[o + 5] = hw;
  }

  const lineBytes = new Uint8Array(line.buffer);
  const ptsBytes = new Uint8Array(pts.buffer);
  const rectBytes = new Uint8Array(rect.buffer);
  const candleBytes = new Uint8Array(candle.buffer);

  const recordBytes =
    (13 + lineBytes.byteLength) +
    (13 + rectBytes.byteLength) +
    (13 + candleBytes.byteLength) +
    (13 + ptsBytes.byteLength);

  const outBuf = new ArrayBuffer(recordBytes);
  const out = new Uint8Array(outBuf);
  const view = new DataView(outBuf);

  let c = 0;
  c = writeUpdateRangeRecord(out, view, c, cfg.lineBufferId, 0, lineBytes);
  c = writeUpdateRangeRecord(out, view, c, cfg.rectBufferId, 0, rectBytes);
  c = writeUpdateRangeRecord(out, view, c, cfg.candleBufferId, 0, candleBytes);
  c = writeUpdateRangeRecord(out, view, c, cfg.pointsBufferId, 0, ptsBytes);

  (self as any).postMessage({ type: "batch", buffer: outBuf }, [outBuf]);
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

  if (msg.type === "startRecipes") {
    running = true;

    const cfg = {
      lineBufferId: msg.lineBufferId >>> 0,
      rectBufferId: msg.rectBufferId >>> 0,
      candleBufferId: msg.candleBufferId >>> 0,
      pointsBufferId: msg.pointsBufferId >>> 0
    };

    const tickMs = Math.max(16, msg.tickMs ?? 33);

    if (timer !== null) clearInterval(timer);
    timer = setInterval(() => {
      if (!running) return;
      tick(cfg);
    }, tickMs) as unknown as number;
  }
};
