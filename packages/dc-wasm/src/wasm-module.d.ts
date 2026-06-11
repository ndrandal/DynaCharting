/* Ambient declaration for the Emscripten-built WASM ES6 module artifact.
 *
 * The build emits packages/dc-wasm/wasm/dc_engine_host.js (an ES6 module whose
 * default export is the createDcEngineHost factory) + dc_engine_host.wasm. There
 * are no hand-written .d.ts for the generated glue, so this ambient module lets
 * tsc type the dynamic `import("../wasm/dc_engine_host.js")` in wasm.ts. The real
 * factory type is DcEngineHostFactory (see wasm.ts).
 */
declare module "*/wasm/dc_engine_host.js" {
  const factory: (
    overrides?: Record<string, unknown>,
  ) => Promise<unknown>;
  export default factory;
}
