// run_dc_ingest.mjs — node driver proving the WASM binary data-plane ingest
// (ENC-505 / P6.4) round-trips a DcCommand[] batch into the dc core and produces
// buffer state identical to the TypeScript CoreIngestStub.
//
// What it does:
//   1. Builds binary record batches in JS (append + updateRange) matching the
//      wire format: [1B op][4B bufferId LE][4B offsetBytes LE][4B payloadBytes LE][payload].
//   2. Feeds each batch to DcCore.ingestBinary (the WASM ingest entry).
//   3. Independently replays the SAME batches through a faithful JS port of the
//      CoreIngestStub append/updateRange semantics, then asserts the WASM
//      buffer bytes (read back via getBufferBytes) match the stub byte-for-byte
//      (plus an FNV-1a digest cross-check and the IngestResult counters).
//
// Pure logic — no WebGPU/browser. Exits non-zero on any mismatch.
//
// Usage:  node core/wasm/run_dc_ingest.mjs  [path/to/dc_wasm.js]

import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const modPath =
  process.argv[2] ?? resolve(here, "../../build-wasm/core/dc_wasm.js");

const { default: createDcCore } = await import(modPath);
const Module = await createDcCore();
const core = new Module.DcCore();

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

// --- Reference: faithful JS port of CoreIngestStub semantics (default cap). ---
// Only models the buffer-byte result (append rolling-window keep-last cap +
// updateRange grow/clamp), which is what we assert against. Default per-buffer
// cap is 4 MiB; our test payloads are tiny so no eviction/clamp triggers, i.e.
// append == plain append, updateRange == grow-to-(offset+len)+memcpy.
const MAX_BUFFER_BYTES = 4 * 1024 * 1024;
class StubBuffers {
  constructor() {
    this.map = new Map(); // id -> Uint8Array
  }
  ensure(id) {
    if (!this.map.has(id)) this.map.set(id, new Uint8Array(0));
  }
  ingest(records) {
    let payloadBytes = 0;
    let droppedBytes = 0;
    const touched = new Set();
    for (const rec of records) {
      const { op, bufferId, offset, payload } = rec;
      const len = payload.length;
      payloadBytes += len;
      this.ensure(bufferId);
      let cpu = this.map.get(bufferId);
      const cap = MAX_BUFFER_BYTES;
      if (op === OP_APPEND) {
        const incoming = len > cap ? payload.subarray(len - cap) : payload;
        const next = new Uint8Array(cpu.length + incoming.length);
        next.set(cpu, 0);
        next.set(incoming, cpu.length);
        cpu = next;
        if (cpu.length > cap) cpu = cpu.subarray(cpu.length - cap);
        if (len > incoming.length) droppedBytes += len - incoming.length;
        this.map.set(bufferId, cpu);
        touched.add(bufferId);
      } else if (op === OP_UPDATE_RANGE) {
        const end = offset + len;
        // (cap branch omitted: our tests never exceed MAX_BUFFER_BYTES)
        if (end > cpu.length) {
          const grown = new Uint8Array(end);
          grown.set(cpu, 0);
          cpu = grown;
        }
        cpu.set(payload, offset);
        this.map.set(bufferId, cpu);
        touched.add(bufferId);
      } else {
        throw new Error(`unknown op ${op}`);
      }
    }
    return { touchedBufferCount: touched.size, payloadBytes, droppedBytes };
  }
  bytes(id) {
    return this.map.get(id) ?? new Uint8Array(0);
  }
}

// --- Binary batch encoder (matches the wire format exactly). ---
function encodeBatch(records) {
  let total = 0;
  for (const r of records) total += 13 + r.payload.length;
  const buf = new Uint8Array(total);
  const dv = new DataView(buf.buffer);
  let p = 0;
  for (const r of records) {
    dv.setUint8(p, r.op); p += 1;
    dv.setUint32(p, r.bufferId, true); p += 4;
    dv.setUint32(p, r.offset, true); p += 4;
    dv.setUint32(p, r.payload.length, true); p += 4;
    buf.set(r.payload, p); p += r.payload.length;
  }
  return buf;
}

// WASM getBufferBytes returns a typed_memory_view into the WASM heap; copy it
// (slice) into a standalone Uint8Array before the next ingest can realloc.
function readBuffer(id) {
  const view = core.getBufferBytes(id);
  return Uint8Array.from(view); // copies element-by-element, detaches from heap
}

function fnv1a(u8) {
  let h = 2166136261 >>> 0;
  for (let i = 0; i < u8.length; i++) {
    h ^= u8[i];
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h >>> 0;
}

function bytesEqual(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}

const hex = (u8) => Array.from(u8, (x) => x.toString(16).padStart(2, "0")).join(" ");

// --- Test batches: a mix of append + updateRange across two buffers. ---
const B1 = 7;   // buffer id (arbitrary, exercises non-trivial u32 LE)
const B2 = 300; // > 255 so the LE encoding spans 2 bytes

// Batch A: append to B1, append to B2, append more to B1.
const batchA = [
  { op: OP_APPEND, bufferId: B1, offset: 0, payload: Uint8Array.from([0xde, 0xad, 0xbe, 0xef]) },
  { op: OP_APPEND, bufferId: B2, offset: 0, payload: Uint8Array.from([0x01, 0x00, 0x02, 0x00, 0x03, 0x00]) },
  { op: OP_APPEND, bufferId: B1, offset: 0, payload: Uint8Array.from([0xca, 0xfe]) },
];

// Batch B: updateRange overwriting the middle of B1 (in bounds), and an
// updateRange on B2 that GROWS the buffer past its current end (zero-fill gap).
const batchB = [
  // B1 is currently [de ad be ef ca fe]; overwrite offset 2..3 -> [.. .. 11 22 .. ..]
  { op: OP_UPDATE_RANGE, bufferId: B1, offset: 2, payload: Uint8Array.from([0x11, 0x22]) },
  // B2 is currently 6 bytes; write 4 bytes at offset 8 -> grows to 12, [6 unchanged][2 zero][4 new]
  { op: OP_UPDATE_RANGE, bufferId: B2, offset: 8, payload: Uint8Array.from([0xaa, 0xbb, 0xcc, 0xdd]) },
];

const stub = new StubBuffers();

let failures = 0;
function assert(cond, msg) {
  if (cond) {
    console.log(`  PASS: ${msg}`);
  } else {
    console.error(`  FAIL: ${msg}`);
    failures++;
  }
}

function runBatch(label, records) {
  console.log(`\n[${label}] ${records.length} records`);
  const batch = encodeBatch(records);

  // WASM ingest.
  const wasmRes = core.ingestBinary(batch);
  // Reference stub ingest.
  const stubRes = stub.ingest(records);

  // Counter cross-checks (touched buffers / payload / dropped).
  assert(
    wasmRes.touchedBufferCount === stubRes.touchedBufferCount,
    `touchedBufferCount wasm=${wasmRes.touchedBufferCount} stub=${stubRes.touchedBufferCount}`
  );
  assert(
    wasmRes.payloadBytes === stubRes.payloadBytes,
    `payloadBytes wasm=${wasmRes.payloadBytes} stub=${stubRes.payloadBytes}`
  );
  assert(
    wasmRes.droppedBytes === stubRes.droppedBytes,
    `droppedBytes wasm=${wasmRes.droppedBytes} stub=${stubRes.droppedBytes}`
  );

  // Per-buffer byte-for-byte verification against the stub.
  for (const id of new Set(records.map((r) => r.bufferId))) {
    const expected = stub.bytes(id);
    const got = readBuffer(id);
    const equal = bytesEqual(got, expected);
    assert(
      equal,
      `buffer ${id} bytes match stub (len=${expected.length}) [${hex(expected)}]` +
        (equal ? "" : `  got [${hex(got)}]`)
    );
    // Size + digest cross-checks.
    assert(core.bufferSize(id) === expected.length, `buffer ${id} size=${expected.length}`);
    assert(
      (core.bufferDigest(id) >>> 0) === fnv1a(expected),
      `buffer ${id} FNV-1a digest matches`
    );
  }
}

runBatch("Batch A: appends", batchA);
runBatch("Batch B: updateRange (overwrite + grow)", batchB);

// Final expected state, spelled out, as an extra explicit anchor:
//   B1: [de ad 11 22 ca fe]   (append de ad be ef, append ca fe, then overwrite [2..3]=11 22)
//   B2: [01 00 02 00 03 00 00 00 aa bb cc dd]  (append 6, then updateRange@8 grows +2 zero +4)
const expB1 = Uint8Array.from([0xde, 0xad, 0x11, 0x22, 0xca, 0xfe]);
const expB2 = Uint8Array.from([0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0xaa, 0xbb, 0xcc, 0xdd]);
console.log("\n[Final state] explicit expected-byte anchors");
assert(bytesEqual(readBuffer(B1), expB1), `B1 == [${hex(expB1)}]`);
assert(bytesEqual(readBuffer(B2), expB2), `B2 == [${hex(expB2)}]`);

core.delete?.();

if (failures > 0) {
  console.error(`\nENC-505 WASM ingest FAILED (${failures} assertion(s))`);
  process.exit(1);
}
console.log("\nENC-505 WASM binary ingest round-trip PASS (matches CoreIngestStub semantics)");
