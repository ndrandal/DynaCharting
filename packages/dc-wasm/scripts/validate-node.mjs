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
//    fields (drawCalls/renderCpuMs/readbackMs) are 0 in node (no GPU render),
//    but the struct shape + activeBuffers must be correct.
//
//    ENC-1265 — this key list is the ONLY check in the repo that the committed
//    .wasm's embind struct still matches the TS type that reads it. `frameMs`
//    became `renderCpuMs` + `readbackMs`; a stale .wasm makes both `undefined`
//    here, which fails loudly rather than reaching the HUD as NaN.
// ---------------------------------------------------------------------------
console.log("\n[stats] EngineStats shape + counters");
const s = host.stats();
const statsKeys = [
  "renderCpuMs",
  "readbackMs",
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
// 3b. getSceneDocument — ENC-984. The structural half of a save/snapshot: the
//     LIVE scene exported as SceneDocument JSON, in the exact shape the engine's
//     own parseSceneDocument/SceneReconciler restore from. Before ENC-984 the
//     C++ serializeSceneDocument existed and was round-trip tested, but was
//     never bound here — `strings dc_engine_host.wasm | grep -c sceneDocument`
//     was 0 and this method did not exist on the module, so a browser could
//     build a chart and never read its structure back.
// ---------------------------------------------------------------------------
console.log("\n[getSceneDocument] export the live scene as SceneDocument JSON");

assert(typeof host.getSceneDocument === "function", "getSceneDocument() present on the module");

const docJson = host.getSceneDocument(false);
assert(typeof docJson === "string" && docJson.length > 0, "returns a non-empty string");

// A copied JS string, not a view into the WASM heap: it must survive the kind of
// call that invalidates getBufferBytes()'s typed_memory_view.
const heldDoc = docJson;
host.applyDataBatch(encodeBatch([{ op: OP_APPEND, bufferId: 20, offset: 4, payload: Uint8Array.from([9, 9]) }]));
assert(heldDoc === docJson && heldDoc.length > 0, "the returned document survives a later applyDataBatch (it is a copy)");

const doc = JSON.parse(docJson);
assert(doc.version === 1, `document version == 1 (got ${doc.version})`);

// The scene built in step 1: pane 1, layer 2, drawItem 3, buffers 10 & 20,
// geometry 100. Every id must be present, with its real properties — not just
// an id list (which is all listResources() has ever given).
assert(!!doc.panes && !!doc.panes["1"], "pane 1 present");
assert(doc.panes["1"].name === "P", `pane 1 name == "P" (got ${doc.panes?.["1"]?.name})`);
assert(!!doc.layers?.["2"] && doc.layers["2"].paneId === 1, "layer 2 present and parented to pane 1");
assert(!!doc.geometries?.["100"], "geometry 100 present");
assert(
  doc.geometries["100"].vertexBufferId === 10 && doc.geometries["100"].format === "pos2_clip",
  "geometry 100 carries vertexBufferId 10 + format pos2_clip",
);
const di = doc.drawItems?.["3"];
assert(!!di, "drawItem 3 present");
assert(di?.layerId === 2, "drawItem 3 parented to layer 2");
assert(di?.pipeline === "triSolid@1", `drawItem 3 pipeline == triSolid@1 (got ${di?.pipeline})`);
assert(di?.geometryId === 100, "drawItem 3 bound to geometry 100");
assert(
  Array.isArray(di?.color) && di.color[0] === 1 && di.color[1] === 0 && di.color[2] === 0,
  `drawItem 3 color == red (got ${JSON.stringify(di?.color)})`,
);

// Structure here, BYTES there: the document reports byteLength and the caller
// reads the contents with the existing getBufferBytes(id).
assert(!!doc.buffers?.["10"], "buffer 10 present in the document");
assert(
  doc.buffers["10"].byteLength === 24,
  `buffer 10 byteLength == 24 (got ${doc.buffers?.["10"]?.byteLength})`,
);
assert(
  doc.buffers["10"].data === undefined,
  "buffer bytes are NOT inlined (read them with getBufferBytes)",
);

// compact=true must drop defaulted fields and stay a strict subset in size.
const compactJson = host.getSceneDocument(true);
assert(compactJson.length < docJson.length, `compact document is smaller (${compactJson.length} < ${docJson.length})`);
assert(JSON.parse(compactJson).drawItems["3"].pipeline === "triSolid@1", "compact document still carries the pipeline binding");

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
console.log("\nENC-506 @repo/dc-wasm node core+ingest validation PASS (incl. ENC-984 getSceneDocument)");
