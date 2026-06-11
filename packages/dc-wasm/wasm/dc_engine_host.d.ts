// Type declaration for the Emscripten-generated dc_engine_host.js ES module.
// The .js (built from core/wasm/dc_engine_host.cpp) has no hand-written types;
// its default export is the createDcEngineHost factory. This .d.ts sits next to
// the .js so module resolution types the dynamic `import("../wasm/dc_engine_host.js")`
// in src/wasm.ts even under a strict consumer (e.g. customer-layer, ENC-507) —
// the ambient wildcard in src/wasm-module.d.ts only applies to UNresolvable
// specifiers, but this import resolves to the real file.
declare const createDcEngineHost: (
  overrides?: Record<string, unknown>,
) => Promise<unknown>;
export default createDcEngineHost;
