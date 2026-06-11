// validate-node.mjs — ENC-506 (P6.5) node validation of @repo/dc-wasm's
// NON-RENDER surface: it loads the dc_engine_host WASM module and exercises the
// EngineHost-shaped API the WASM core implements — applyControl (a few JSON
// commands), applyDataBatch (a binary record batch), and stats/buffer readback —
// asserting correctness. Reuses the ENC-502 (command round-trip) + ENC-505
// (binary ingest) patterns. WebGPU render()/pick() are NOT exercised here (no
// navigator.gpu in node) — that's the browser harness (examples/engine_host_demo.html).
//
// Exits non-zero on any failed assertion.
//
// Usage: node scripts/validate-node.mjs [path/to/dc_engine_host.js]

import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const modPath = process.argv[2] ?? resolve(here, "../wasm/dc_engine_host.js");

const { default: createDcEngineHost } = await import(modPath);
const Module = await createDcEngineHost();
const host = new Module.DcEngineHost();

let failures = 0;
function assert(cond, msg) {
  if (cond) {
    console.log(`  PASS: ${msg}`);
  } else {
    console.error(`  FAIL: ${msg}`);
    failures++;
  }
}

// ---------------------------------------------------------------------------
// 1. applyControl — round-trip a minimal scene through the WASM CommandProcessor
//    (the same verbs the native render tests + dc_webgpu_all use).
// ---------------------------------------------------------------------------
console.log("\n[applyControl] build a scene via JSON commands");

function control(obj) {
  const r = host.applyControl(JSON.stringify(obj));
  if (!r.ok) console.error(`    applyControl rejected: ${r.error}  cmd=${JSON.stringify(obj)}`);
  return r;
}

assert(control({ cmd: "createPane", id: 1, name: "P" }).ok, "createPane ok");
assert(control({ cmd: "createLayer", id: 2, paneId: 1 }).ok, "createLayer ok");
assert(control({ cmd: "createDrawItem", id: 3, layerId: 2 }).ok, "createDrawItem ok");
assert(
  control({ cmd: "createBuffer", id: 10, byteLength: 24 }).ok,
  "createBuffer ok",
);
assert(
  control({
    cmd: "createGeometry",
    id: 100,
    vertexBufferId: 10,
    vertexCount: 3,
    format: "pos2_clip",
  }).ok,
  "createGeometry ok",
);
assert(
  control({
    cmd: "bindDrawItem",
    drawItemId: 3,
    pipeline: "triSolid@1",
    geometryId: 100,
  }).ok,
  "bindDrawItem (triSolid@1) ok",
);
assert(
  control({ cmd: "setDrawItemColor", drawItemId: 3, r: 1, g: 0, b: 0, a: 1 }).ok,
  "setDrawItemColor ok",
);

// A bad command must be rejected (ok:false) — proves error routing works.
const bad = host.applyControl(JSON.stringify({ cmd: "thisVerbDoesNotExist" }));
assert(!bad.ok && typeof bad.error === "string", "unknown cmd rejected with error string");

// Scene readback.
assert(host.paneCount() === 1, `paneCount == 1 (got ${host.paneCount()})`);
assert(host.layerCount() === 1, `layerCount == 1 (got ${host.layerCount()})`);
assert(host.drawItemCount() === 1, `drawItemCount == 1 (got ${host.drawItemCount()})`);
assert(host.bufferCount() === 1, `bufferCount == 1 (got ${host.bufferCount()})`);
assert(host.geometryCount() === 1, `geometryCount == 1 (got ${host.geometryCount()})`);

// ---------------------------------------------------------------------------
// 2. applyDataBatch — feed a binary record batch (ENC-505 wire format) and
//    assert the buffer bytes + ingest stats round-trip.
// ---------------------------------------------------------------------------
console.log("\n[applyDataBatch] ingest a binary record batch");

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

function encodeBatch(records) {
  let total = 0;
  for (const r of records) total += 13 + r.payload.length;
  const buf = new Uint8Array(total);
  const dv = new DataView(buf.buffer);
  let p = 0;
  for (const r of records) {
    dv.setUint8(p, r.op);
    p += 1;
    dv.setUint32(p, r.bufferId, true);
    p += 4;
    dv.setUint32(p, r.offset, true);
    p += 4;
    dv.setUint32(p, r.payload.length, true);
    p += 4;
    buf.set(r.payload, p);
    p += r.payload.length;
  }
  return buf;
}

function readBuffer(id) {
  return Uint8Array.from(host.getBufferBytes(id));
}
function bytesEqual(a, b) {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}
const hex = (u8) => Array.from(u8, (x) => x.toString(16).padStart(2, "0")).join(" ");

// Append 24 bytes (6 floats — the triSolid vertex buffer id 10 created above) so
// the ingested bytes land in the SAME buffer the geometry references. We write
// 3 pos2 clip-space verts.
const verts = new Float32Array([-0.9, -0.7, -0.3, -0.7, -0.6, 0.7]);
const vbytes = new Uint8Array(verts.buffer.slice(0));
const batchA = [{ op: OP_APPEND, bufferId: 10, offset: 0, payload: vbytes }];
host.applyDataBatch(encodeBatch(batchA));

assert(host.bufferSize(10) === 24, `buffer 10 size == 24 after append (got ${host.bufferSize(10)})`);
assert(bytesEqual(readBuffer(10), vbytes), `buffer 10 bytes match appended verts [${hex(vbytes)}]`);

// updateRange: overwrite the first float (offset 0, 4 bytes) and append a new
// buffer to exercise both ops + a second buffer.
const overwrite = new Uint8Array(new Float32Array([-0.8]).buffer.slice(0));
const batchB = [
  { op: OP_UPDATE_RANGE, bufferId: 10, offset: 0, payload: overwrite },
  { op: OP_APPEND, bufferId: 20, offset: 0, payload: Uint8Array.from([1, 2, 3, 4]) },
];
host.applyDataBatch(encodeBatch(batchB));

const expected10 = new Uint8Array(24);
expected10.set(vbytes, 0);
expected10.set(overwrite, 0);
assert(bytesEqual(readBuffer(10), expected10), "buffer 10 reflects updateRange overwrite");
assert(host.bufferSize(20) === 4, `buffer 20 size == 4 after append (got ${host.bufferSize(20)})`);
assert(bytesEqual(readBuffer(20), Uint8Array.from([1, 2, 3, 4])), "buffer 20 bytes match");

// ---------------------------------------------------------------------------
// 3. stats — the EngineStats-shaped counters the WASM core fills. Render-driven
//    fields (drawCalls/frameMs) are 0 in node (no GPU render), but the struct
//    shape + activeBuffers must be correct.
// ---------------------------------------------------------------------------
console.log("\n[stats] EngineStats shape + counters");
const s = host.stats();
const statsKeys = [
  "frameMs",
  "drawCalls",
  "culledDrawCalls",
  "ingestedBytesThisFrame",
  "uploadedBytesThisFrame",
  "activeBuffers",
];
for (const k of statsKeys) {
  assert(k in s && typeof s[k] === "number", `stats has numeric '${k}' (=${s[k]})`);
}

// ---------------------------------------------------------------------------
// 4. render()/pick() are present (callable) but require WebGPU. We only assert
//    the methods exist on the surface (the browser harness validates pixels).
// ---------------------------------------------------------------------------
console.log("\n[surface] render/pick present (browser-validated, not run here)");
assert(typeof host.render === "function", "render() present");
assert(typeof host.pick === "function", "pick() present");
assert(typeof host.dispose === "function", "dispose() present");
assert(typeof host.framebuffer === "function", "framebuffer() present");

host.dispose?.();
host.delete?.();

if (failures > 0) {
  console.error(`\nENC-506 dc-wasm node validation FAILED (${failures} assertion(s))`);
  process.exit(1);
}
console.log("\nENC-506 @repo/dc-wasm node core+ingest validation PASS");
