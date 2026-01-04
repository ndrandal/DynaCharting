/// <reference lib="webworker" />

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

type StreamType = "lineSine" | "pointsCos" | "rectBars" | "candles";

type WorkerMsg =
  | {
      // Back-compat: emit all 4 fake streams (your existing behavior)
      type: "startRecipes";
      lineBufferId: number;
      rectBufferId: number;
      candleBufferId: number;
      pointsBufferId: number;
      tickMs?: number;
    }
  | {
      // Recipe subscriptions (fake streams)
      type: "startStreams";
      tickMs?: number;
      streams: Array<{ stream: StreamType; bufferId: number }>;
    }
  | {
      // NEW: real server integration
      type: "connectWs";
      url: string;
      protocols?: string | string[];
      // optional: auth payload you want to send after open
      auth?: any;
      // optional: batching timer, only used for JSON record mode
      tickMs?: number;
    }
  | { type: "disconnectWs" }
  | { type: "stop" };

type OutMsg =
  | { type: "batch"; buffer: ArrayBuffer }
  | { type: "wsStatus"; state: "connecting" | "open" | "closed" | "error"; detail?: any }
  | { type: "wsStats"; rxBytes: number; rxMsgs: number; dropped: number };

let running = false;
let timer: number | null = null;

// WebSocket state
let ws: WebSocket | null = null;
let wsRxBytes = 0;
let wsRxMsgs = 0;
let wsDropped = 0;

// For JSON-record mode: we accumulate records and flush to one binary batch periodically
type JsonRecord =
  | { op: "append"; bufferId: number; payload: Uint8Array }
  | { op: "updateRange"; bufferId: number; offsetBytes: number; payload: Uint8Array };

let jsonRecordQueue: JsonRecord[] = [];
let jsonFlushTimer: number | null = null;
let jsonTickMs = 16; // default flush cadence

function post(msg: OutMsg, transfer?: Transferable[]) {
  (self as any).postMessage(msg, transfer ?? []);
}

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
  payloadBytes: Uint8Array
): number {
  out[cursor] = op;
  u32(view, cursor + 1, bufferId);
  u32(view, cursor + 5, offsetBytes);
  u32(view, cursor + 9, payloadBytes.byteLength);
  out.set(payloadBytes, cursor + 13);
  return cursor + 13 + payloadBytes.byteLength;
}

function buildBatchFromJsonRecords(records: JsonRecord[]): ArrayBuffer | null {
  if (records.length === 0) return null;

  let total = 0;
  for (const r of records) total += 13 + r.payload.byteLength;

  const buf = new ArrayBuffer(total);
  const out = new Uint8Array(buf);
  const view = new DataView(buf);

  let c = 0;
  for (const r of records) {
    const op = r.op === "append" ? OP_APPEND : OP_UPDATE_RANGE;
    const off = r.op === "append" ? 0 : r.offsetBytes >>> 0;
    c = writeRecord(out, view, c, op, r.bufferId >>> 0, off, r.payload);
  }

  return buf;
}

// -------------------- Fake stream payload builders --------------------
function buildLineSineBytes(t: number): Uint8Array {
  const lineVerts = 512; // even for LINES
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

  let total = 0;
  for (const p of payloads) total += 13 + p.bytes.byteLength;

  const outBuf = new ArrayBuffer(total);
  const out = new Uint8Array(outBuf);
  const view = new DataView(outBuf);

  let c = 0;
  for (const p of payloads) c = writeRecord(out, view, c, OP_UPDATE_RANGE, p.bufferId, 0, p.bytes);

  post({ type: "batch", buffer: outBuf }, [outBuf]);
}

function startInterval(tickMs: number, fn: () => void) {
  running = true;
  if (timer !== null) clearInterval(timer);
  timer = setInterval(() => {
    if (!running) return;
    fn();
  }, tickMs) as unknown as number;
}

function stopIntervals() {
  running = false;
  if (timer !== null) { clearInterval(timer); timer = null; }
  if (jsonFlushTimer !== null) { clearInterval(jsonFlushTimer); jsonFlushTimer = null; }
}

// -------------------- WebSocket integration --------------------

function disconnectWs() {
  if (ws) {
    try { ws.close(); } catch {}
  }
  ws = null;
}

function startJsonFlushLoop(tickMs: number) {
  jsonTickMs = Math.max(8, tickMs | 0);

  if (jsonFlushTimer !== null) clearInterval(jsonFlushTimer);
  jsonFlushTimer = setInterval(() => {
    if (!ws) return; // only flush in ws mode
    if (jsonRecordQueue.length === 0) return;

    // batch all queued records into one ArrayBuffer
    const batch = buildBatchFromJsonRecords(jsonRecordQueue);
    jsonRecordQueue = [];

    if (batch) post({ type: "batch", buffer: batch }, [batch]);
  }, jsonTickMs) as unknown as number;
}

function handleWsMessage(data: any) {
  // CASE 1: server sends binary ArrayBuffer that's already our batch format.
  if (data instanceof ArrayBuffer) {
    wsRxBytes += data.byteLength;
    wsRxMsgs += 1;
    post({ type: "batch", buffer: data }, [data]);
    return;
  }

  // CASE 1b: server sends Blob (common) -> read to ArrayBuffer
  if (typeof Blob !== "undefined" && data instanceof Blob) {
    // Note: async; fine for worker
    data.arrayBuffer().then((ab) => {
      wsRxBytes += ab.byteLength;
      wsRxMsgs += 1;
      post({ type: "batch", buffer: ab }, [ab]);
    }).catch(() => {
      wsDropped += 1;
    });
    return;
  }

  // CASE 2: server sends JSON text; parse and queue records
  if (typeof data === "string") {
    wsRxBytes += data.length;
    wsRxMsgs += 1;

    let obj: any;
    try { obj = JSON.parse(data); }
    catch { wsDropped += 1; return; }

    // Support either a single record or {records:[...]}
    const records = Array.isArray(obj?.records) ? obj.records : [obj];

    for (const r of records) {
      const type = r?.type;
      const bufferId = (r?.bufferId >>> 0) || 0;
      if (!bufferId) { wsDropped += 1; continue; }

      // payload may be base64 or an array of numbers (bytes)
      let payload: Uint8Array | null = null;

      if (typeof r?.payloadBase64 === "string") {
        payload = base64ToU8(r.payloadBase64);
      } else if (Array.isArray(r?.payloadBytes)) {
        payload = new Uint8Array(r.payloadBytes);
      }

      if (!payload) { wsDropped += 1; continue; }

      if (type === "append") {
        jsonRecordQueue.push({ op: "append", bufferId, payload });
      } else if (type === "updateRange") {
        const offsetBytes = (r?.offsetBytes >>> 0) || 0;
        jsonRecordQueue.push({ op: "updateRange", bufferId, offsetBytes, payload });
      } else {
        wsDropped += 1;
      }
    }

    return;
  }

  wsDropped += 1;
}

function base64ToU8(b64: string): Uint8Array {
  // atob exists in workers
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i) & 255;
  return out;
}

function connectWs(url: string, protocols?: string | string[], auth?: any, tickMs?: number) {
  // stop fake loops so we don’t double-feed
  stopIntervals();
  disconnectWs();

  try {
    post({ type: "wsStatus", state: "connecting", detail: url });
    ws = protocols ? new WebSocket(url, protocols) : new WebSocket(url);
    ws.binaryType = "arraybuffer";

    ws.onopen = () => {
      post({ type: "wsStatus", state: "open" });

      if (auth !== undefined) {
        try { ws?.send(JSON.stringify({ type: "auth", ...auth })); } catch {}
      }

      // If server is JSON record mode, we flush queue periodically.
      // If server sends pure binary batches, this loop does almost nothing.
      startJsonFlushLoop(tickMs ?? 16);
    };

    ws.onmessage = (ev) => {
      handleWsMessage(ev.data);

      // optional periodic stats
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
      if (jsonFlushTimer !== null) { clearInterval(jsonFlushTimer); jsonFlushTimer = null; }
    };
  } catch (e) {
    post({ type: "wsStatus", state: "error", detail: e });
  }
}

// -------------------- message handling --------------------
self.onmessage = (ev: MessageEvent<WorkerMsg>) => {
  const msg = ev.data;

  if (msg.type === "stop") {
    stopIntervals();
    return;
  }

  if (msg.type === "disconnectWs") {
    disconnectWs();
    if (jsonFlushTimer !== null) { clearInterval(jsonFlushTimer); jsonFlushTimer = null; }
    return;
  }

  if (msg.type === "connectWs") {
    connectWs(msg.url, msg.protocols, msg.auth, msg.tickMs);
    return;
  }

  if (msg.type === "startRecipes") {
    // Ensure ws is off; this is fake mode
    disconnectWs();
    if (jsonFlushTimer !== null) { clearInterval(jsonFlushTimer); jsonFlushTimer = null; }

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
    // Ensure ws is off; this is fake mode
    disconnectWs();
    if (jsonFlushTimer !== null) { clearInterval(jsonFlushTimer); jsonFlushTimer = null; }

    const streams = (msg.streams ?? []).map((s) => ({
      stream: s.stream,
      bufferId: s.bufferId >>> 0
    }));

    const tickMs = Math.max(16, msg.tickMs ?? 33);
    startInterval(tickMs, () => tickStreams(streams));
    return;
  }
};
