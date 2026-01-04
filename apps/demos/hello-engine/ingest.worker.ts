/// <reference lib="webworker" />

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

type StreamType = "lineSine" | "pointsCos" | "rectBars" | "candles";

type WorkerMsg =
  | {
      // Back-compat (current mode): emit all 4 streams
      type: "startRecipes";
      lineBufferId: number;
      rectBufferId: number;
      candleBufferId: number;
      pointsBufferId: number;
      tickMs?: number;
    }
  | {
      // New mode (D7.1): subscriptions
      type: "startStreams";
      tickMs?: number;
      streams: Array<{ stream: StreamType; bufferId: number }>;
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
  out[cursor] = OP_UPDATE_RANGE;
  u32(view, cursor + 1, bufferId);
  u32(view, cursor + 5, offsetBytes);
  u32(view, cursor + 9, payloadBytes.byteLength);
  out.set(payloadBytes, cursor + 13);
  return cursor + 13 + payloadBytes.byteLength;
}

// ---- stream payload builders (match your current shapes) ----
function buildLineSineBytes(t: number): Uint8Array {
  const lineVerts = 512; // must be even for LINES
  const line = new Float32Array(lineVerts * 2);
  for (let i = 0; i < lineVerts; i++) {
    const x = -1 + (2 * i) / Math.max(1, lineVerts - 1);
    const y = 0.35 * Math.sin(4 * x + t);
    line[i * 2 + 0] = x;
    line[i * 2 + 1] = y;
  }
  return new Uint8Array(line.buffer);
}

function buildPointsCosBytes(t: number): Uint8Array {
  const ptsCount = 256;
  const pts = new Float32Array(ptsCount * 2);
  for (let i = 0; i < ptsCount; i++) {
    const x = -1 + (2 * i) / Math.max(1, ptsCount - 1);
    const y = 0.15 * Math.cos(8 * x - t);
    pts[i * 2 + 0] = x;
    pts[i * 2 + 1] = y;
  }
  return new Uint8Array(pts.buffer);
}

function buildRectBarsBytes(t: number): Uint8Array {
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
  return new Uint8Array(rect.buffer);
}

function buildCandlesBytes(t: number): Uint8Array {
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
  return new Uint8Array(candle.buffer);
}

// ---- generic tick from subscriptions ----
function tickStreams(streams: Array<{ stream: StreamType; bufferId: number }>) {
  const t = performance.now() * 0.001;

  const payloads: Array<{ bufferId: number; bytes: Uint8Array }> = [];
  for (const s of streams) {
    const bufferId = s.bufferId >>> 0;
    if (!bufferId) continue;

    if (s.stream === "lineSine") payloads.push({ bufferId, bytes: buildLineSineBytes(t) });
    else if (s.stream === "pointsCos") payloads.push({ bufferId, bytes: buildPointsCosBytes(t) });
    else if (s.stream === "rectBars") payloads.push({ bufferId, bytes: buildRectBarsBytes(t) });
    else if (s.stream === "candles") payloads.push({ bufferId, bytes: buildCandlesBytes(t) });
  }

  let recordBytes = 0;
  for (const p of payloads) recordBytes += 13 + p.bytes.byteLength;

  const outBuf = new ArrayBuffer(recordBytes);
  const out = new Uint8Array(outBuf);
  const view = new DataView(outBuf);

  let c = 0;
  for (const p of payloads) c = writeUpdateRangeRecord(out, view, c, p.bufferId, 0, p.bytes);

  (self as any).postMessage({ type: "batch", buffer: outBuf }, [outBuf]);
}

function startInterval(tickMs: number, fn: () => void) {
  running = true;
  if (timer !== null) clearInterval(timer);
  timer = setInterval(() => {
    if (!running) return;
    fn();
  }, tickMs) as unknown as number;
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
    const streams = [
      { stream: "lineSine" as const, bufferId: msg.lineBufferId >>> 0 },
      { stream: "rectBars" as const, bufferId: msg.rectBufferId >>> 0 },
      { stream: "candles" as const, bufferId: msg.candleBufferId >>> 0 },
      { stream: "pointsCos" as const, bufferId: msg.pointsBufferId >>> 0 }
    ];

    const tickMs = Math.max(16, msg.tickMs ?? 33);
    startInterval(tickMs, () => tickStreams(streams));
    return;
  }

  if (msg.type === "startStreams") {
    const streams = (msg.streams ?? []).map((s) => ({
      stream: s.stream,
      bufferId: s.bufferId >>> 0
    }));

    const tickMs = Math.max(16, msg.tickMs ?? 33);
    startInterval(tickMs, () => tickStreams(streams));
    return;
  }
};
