// run_dc_wasm.mjs — node driver proving the dc logic core runs in WASM (ENC-502).
//
// Loads the Emscripten ES6 module (build-wasm/core/dc_wasm.js), instantiates a
// DcCore, applies a sequence of JSON commands across the JS<->WASM boundary
// (createPane / createLayer / createBuffer / createGeometry / createDrawItem),
// and reads back scene state. Exits non-zero if any command fails or the final
// state is wrong.
//
// Usage:  node core/wasm/run_dc_wasm.mjs  [path/to/dc_wasm.js]

import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const modPath =
  process.argv[2] ??
  resolve(here, "../../build-wasm/core/dc_wasm.js");

const { default: createDcCore } = await import(modPath);
const Module = await createDcCore();

const core = new Module.DcCore();

function apply(json) {
  const r = core.applyCommand(json);
  const tag = r.ok ? "ok" : `FAIL[${r.errorCode}:${r.errorMessage}]`;
  console.log(`apply ${json}  -> ${tag} createdId=${r.createdId}`);
  if (!r.ok) {
    console.error(`command failed: ${json} -> ${r.errorCode} ${r.errorMessage}`);
    process.exit(1);
  }
  return r;
}

// Round-trip a minimal scene through the WASM core.
apply(`{"cmd":"hello"}`);
const pane = apply(`{"cmd":"createPane","name":"Main"}`);
const layer = apply(`{"cmd":"createLayer","paneId":${pane.createdId},"name":"L1"}`);
const buffer = apply(`{"cmd":"createBuffer","name":"buf0","byteLength":256}`);
apply(`{"cmd":"createGeometry","name":"geo0","vertexBufferId":${buffer.createdId},"vertexCount":3,"format":"pos2_clip"}`);
apply(`{"cmd":"createDrawItem","layerId":${layer.createdId},"name":"ItemA"}`);
apply(`{"cmd":"createDrawItem","layerId":${layer.createdId},"name":"ItemB"}`);

const panes = core.paneCount();
const layers = core.layerCount();
const drawItems = core.drawItemCount();
const buffers = core.bufferCount();
const geometries = core.geometryCount();

console.log("\n--- WASM core state ---");
console.log(`panes=${panes} layers=${layers} drawItems=${drawItems} buffers=${buffers} geometries=${geometries}`);
console.log("listResources():", core.listResources());

core.delete?.();

const okState =
  panes === 1 && layers === 1 && drawItems === 2 && buffers === 1 && geometries === 1;

console.log(
  `\npane created, drawItem count = ${drawItems}, ok=${okState}`
);

if (!okState) {
  console.error("UNEXPECTED STATE");
  process.exit(1);
}
console.log("ENC-502 WASM round-trip PASS");
