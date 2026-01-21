/// <reference lib="webworker" />

// -------------------- Data plane record format --------------------
// u8  op (1 append, 2 updateRange)
// u32 bufferId
// u32 offsetBytes (for updateRange; 0 for append)
// u32 payloadBytes
// payload
import type { StreamType, PolicyMode } from "@repo/protocol"

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

// -------------------- Stream + policy --------------------
// -------------------- Stream + policy --------------------


let policyMode: PolicyMode = "raw";

// -------------------- Worker protocol --------------------
type WorkerMsg =
  | {
      // Back-compat: emit all 4 fake streams at once
      type: "startRecipes";
      lineBufferId: number;
      rectBufferId: number;
      candleBufferId: number;
      pointsBufferId: number;
      tickMs?: number;
    }
  | {
      // D7.1/D8.1: recipe-defined subscriptions (fake mode)
      type: "startStreams";
      tickMs?: number;
      streams: Array<{ stream: StreamType; bufferId: number }>;
    }
  | {
      // D5.2: connect to real server WS
      type: "connectWs";
      url: string;
      protocols?: string | string[];
      auth?: any;      // optional auth payload, sent as JSON after open
      tickMs?: number; // used only for JSON-record batching
    }
  | { type: "disconnectWs" }
  | { type: "stop" }
  | { type: "updatePolicy"; mode: PolicyMode };

type OutMsg =
  | { type: "batch"; buffer: ArrayBuffer }
  | { type: "wsStatus"; state: "connecting" | "open" | "closed" | "error"; detail?: any }
  | { type: "wsStats"; rxBytes: number; rxMsgs: number; dropped: number };

// -------------------- timers --------------------
let fakeRunning = false;
let fakeTimer: number | null = null;

// WS state
let ws: WebSocket | null = null;
let wsRxBytes = 0;
let wsRxMsgs = 0;
let wsDropped = 0;

// JSON record mode accumulation (server sends JSON records)
type JsonRecord =
  | { op: "append"; bufferId: number; payload: Uint8Array }
  | { op: "updateRange"; bufferId: number; offsetBytes: number; payload: Uint8Array };

let jsonQueue: JsonRecord[] = [];
let jsonFlushTimer: number | null = null;
let jsonFlushMs = 16;

// -------------------- post helpers --------------------
function post(msg: OutMsg, transfer?: Transferable[]) {
  (self as any).postMessage(msg, transfer ?? []);
}

function stopFake() {
  fakeRunning = false;
  if (fakeTimer !== null) {
    clearInterval(fakeTimer);
    fakeTimer = null;
  }
}

function stopJsonFlush() {
  if (jsonFlushTimer !== null) {
    clearInterval(jsonFlushTimer);
    jsonFlushTimer = null;
  }
}

function stopAll() {
  stopFake();
  stopJsonFlush();
}

// -------------------- record writers --------------------
function u32(view: DataView, off: number, v: number) {
  view.setUint32(off, v >>> 0, true);
}

function writeRecord(
  out: Uint8Array,
  view: DataView,
  cursor: number,
  op: number,
  bufferId: number,
  offsetBytes: number,
  payload: Uint8Array
): number {
  out[cursor] = op;
  u32(view, cursor + 1, bufferId >>> 0);
  u32(view, cursor + 5, offsetBytes >>> 0);
  u32(view, cursor + 9, payload.byteLength >>> 0);
  out.set(payload, cursor + 13);
  return cursor + 13 + payload.byteLength;
}

function buildBatchFromJson(records: JsonRecord[]): ArrayBuffer | null {
  if (records.length === 0) return null;

  let total = 0;
  for (const r of records) total += 13 + r.payload.byteLength;

  const buf = new ArrayBuffer(total);
  const out = new Uint8Array(buf);
  const view = new DataView(buf);

  let c = 0;
  for (const r of records) {
    const op = r.op === "append" ? OP_APPEND : OP_UPDATE_RANGE;
    const off = r.op === "append" ? 0 : (r.offsetBytes >>> 0);
    c = writeRecord(out, view, c, op, r.bufferId, off, r.payload);
  }

  return buf;
}

function startJsonFlushLoop(tickMs: number) {
  jsonFlushMs = Math.max(8, tickMs | 0);

  stopJsonFlush();
  jsonFlushTimer = setInterval(() => {
    if (!ws) return;
    if (jsonQueue.length === 0) return;

    const batch = buildBatchFromJson(jsonQueue);
    jsonQueue = [];

    if (batch) post({ type: "batch", buffer: batch }, [batch]);
  }, jsonFlushMs) as unknown as number;
}

// -------------------- fake streams --------------------
function density(): { lineVerts: number; ptsCount: number; rectN: number; candleN: number } {
  // IMPORTANT: lineVerts must be even if you render as LINES
  if (policyMode === "agg") {
    return { lineVerts: 128, ptsCount: 64, rectN: 40, candleN: 30 };
  }
  return { lineVerts: 1024, ptsCount: 512, rectN: 120, candleN: 90 };
}

function buildLineSineBytes(t: number, lineVerts: number): Uint8Array {
  const v = (lineVerts % 2 === 0) ? lineVerts : (lineVerts + 1);

  // domain space: x in [0..100]
  const x0 = 0;
  const x1 = 100;

  const line = new Float32Array(v * 2);
  for (let i = 0; i < v; i++) {
    const u = i / Math.max(1, v - 1);
    const x = x0 + (x1 - x0) * u;

    // domain y in [-1..1]
    const y = 0.8 * Math.sin(0.12 * x + t);

    line[i * 2 + 0] = x;
    line[i * 2 + 1] = y;
  }

  return new Uint8Array(line.buffer);
}


function buildPointsCosBytes(t: number, ptsCount: number): Uint8Array {
  const x0 = 0;
  const x1 = 100;

  const pts = new Float32Array(ptsCount * 2);
  for (let i = 0; i < ptsCount; i++) {
    const u = i / Math.max(1, ptsCount - 1);
    const x = x0 + (x1 - x0) * u;

    const y = 0.35 * Math.cos(0.22 * x - t);

    pts[i * 2 + 0] = x;
    pts[i * 2 + 1] = y;
  }
  return new Uint8Array(pts.buffer);
}


function buildRectBarsBytes(t: number, rectN: number): Uint8Array {
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

function buildCandlesBytes(t: number, candleN: number): Uint8Array {
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

function tickFake(streams: Array<{ stream: StreamType; bufferId: number }>) {
  const t = performance.now() * 0.001;
  const d = density();

  const payloads: Array<{ bufferId: number; bytes: Uint8Array }> = [];

  for (const s of streams) {
    const bufferId = s.bufferId >>> 0;
    if (!bufferId) continue;

    if (s.stream === "lineSine") payloads.push({ bufferId, bytes: buildLineSineBytes(t, d.lineVerts) });
    else if (s.stream === "pointsCos") payloads.push({ bufferId, bytes: buildPointsCosBytes(t, d.ptsCount) });
    else if (s.stream === "rectBars") payloads.push({ bufferId, bytes: buildRectBarsBytes(t, d.rectN) });
    else if (s.stream === "candles") payloads.push({ bufferId, bytes: buildCandlesBytes(t, d.candleN) });
  }

  let total = 0;
  for (const p of payloads) total += 13 + p.bytes.byteLength;

  const outBuf = new ArrayBuffer(total);
  const out = new Uint8Array(outBuf);
  const view = new DataView(outBuf);

  let c = 0;
  for (const p of payloads) {
    c = writeRecord(out, view, c, OP_UPDATE_RANGE, p.bufferId, 0, p.bytes);
  }

  post({ type: "batch", buffer: outBuf }, [outBuf]);
}

function startFake(tickMs: number, streams: Array<{ stream: StreamType; bufferId: number }>) {
  stopAll();
  disconnectWs(); // fake and ws modes are mutually exclusive

  fakeRunning = true;
  const ms = Math.max(16, tickMs | 0);

  fakeTimer = setInterval(() => {
    if (!fakeRunning) return;
    tickFake(streams);
  }, ms) as unknown as number;
}

// -------------------- WebSocket integration --------------------
function disconnectWs() {
  if (ws) {
    try { ws.close(); } catch {}
  }
  ws = null;
}

function handleWsMessage(data: any) {
  // Preferred: server sends our batch as binary
  if (data instanceof ArrayBuffer) {
    wsRxBytes += data.byteLength;
    wsRxMsgs += 1;
    post({ type: "batch", buffer: data }, [data]);
    return;
  }

  // Common: Blob
  if (typeof Blob !== "undefined" && data instanceof Blob) {
    data.arrayBuffer().then((ab) => {
      wsRxBytes += ab.byteLength;
      wsRxMsgs += 1;
      post({ type: "batch", buffer: ab }, [ab]);
    }).catch(() => { wsDropped += 1; });
    return;
  }

  // Fallback: JSON string -> queue records -> periodic flush
  if (typeof data === "string") {
    wsRxBytes += data.length;
    wsRxMsgs += 1;

    let obj: any;
    try { obj = JSON.parse(data); }
    catch { wsDropped += 1; return; }

    const records = Array.isArray(obj?.records) ? obj.records : [obj];

    for (const r of records) {
      const type = r?.type;
      const bufferId = (r?.bufferId >>> 0) || 0;
      if (!bufferId) { wsDropped += 1; continue; }

      let payload: Uint8Array | null = null;

      if (typeof r?.payloadBase64 === "string") {
        payload = base64ToU8(r.payloadBase64);
      } else if (Array.isArray(r?.payloadBytes)) {
        payload = new Uint8Array(r.payloadBytes);
      }

      if (!payload) { wsDropped += 1; continue; }

      if (type === "append") {
        jsonQueue.push({ op: "append", bufferId, payload });
      } else if (type === "updateRange") {
        const offsetBytes = (r?.offsetBytes >>> 0) || 0;
        jsonQueue.push({ op: "updateRange", bufferId, offsetBytes, payload });
      } else {
        wsDropped += 1;
      }
    }

    return;
  }

  wsDropped += 1;
}

function base64ToU8(b64: string): Uint8Array {
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i) & 255;
  return out;
}

function connectWs(url: string, protocols?: string | string[], auth?: any, flushMs?: number) {
  stopAll();
  disconnectWs();

  wsRxBytes = 0;
  wsRxMsgs = 0;
  wsDropped = 0;
  jsonQueue = [];

  try {
    post({ type: "wsStatus", state: "connecting", detail: url });

    ws = protocols ? new WebSocket(url, protocols) : new WebSocket(url);
    ws.binaryType = "arraybuffer";

    ws.onopen = () => {
      post({ type: "wsStatus", state: "open" });

      // Optional auth
      if (auth !== undefined) {
        try { ws?.send(JSON.stringify({ type: "auth", ...auth })); } catch {}
      }

      // Tell server current policy mode (if it cares)
      try { ws?.send(JSON.stringify({ type: "subscriptionPolicy", mode: policyMode })); } catch {}

      // If server uses JSON-record mode, we flush to binary batches on a timer.
      startJsonFlushLoop(flushMs ?? 16);
    };

    ws.onmessage = (ev) => {
      handleWsMessage(ev.data);

      // periodic stats
      if ((wsRxMsgs % 60) === 0) {
        post({ type: "wsStats", rxBytes: wsRxBytes, rxMsgs: wsRxMsgs, dropped: wsDropped });
      }
    };

    ws.onerror = (e) => {
      post({ type: "wsStatus", state: "error", detail: e });
    };

    ws.onclose = () => {
      post({ type: "wsStatus", state: "closed" });
      disconnectWs();
      stopJsonFlush();
      jsonQueue = [];
    };
  } catch (e) {
    post({ type: "wsStatus", state: "error", detail: e });
  }
}

// -------------------- message handling --------------------
self.onmessage = (ev: MessageEvent<WorkerMsg>) => {
  const msg = ev.data;

  if (msg.type === "stop") {
    stopAll();
    return;
  }

  if (msg.type === "disconnectWs") {
    disconnectWs();
    stopJsonFlush();
    jsonQueue = [];
    return;
  }

  if (msg.type === "updatePolicy") {
    policyMode = msg.mode === "agg" ? "agg" : "raw";

    // Forward policy to server if connected
    if (ws && ws.readyState === WebSocket.OPEN) {
      try { ws.send(JSON.stringify({ type: "subscriptionPolicy", mode: policyMode })); } catch {}
    }
    return;
  }

  if (msg.type === "connectWs") {
    connectWs(msg.url, msg.protocols, msg.auth, msg.tickMs);
    return;
  }

  if (msg.type === "startRecipes") {
    const streams = [
      { stream: "lineSine" as const, bufferId: msg.lineBufferId >>> 0 },
      { stream: "rectBars" as const, bufferId: msg.rectBufferId >>> 0 },
      { stream: "candles" as const, bufferId: msg.candleBufferId >>> 0 },
      { stream: "pointsCos" as const, bufferId: msg.pointsBufferId >>> 0 }
    ];
    startFake(msg.tickMs ?? 33, streams);
    return;
  }

  if (msg.type === "startStreams") {
    const streams = (msg.streams ?? []).map((s) => ({ stream: s.stream, bufferId: s.bufferId >>> 0 }));
    startFake(msg.tickMs ?? 33, streams);
    return;
  }
};
