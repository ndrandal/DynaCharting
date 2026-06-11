/* packages/dc-wasm/src/wasm.ts
 *
 * Typed loader for the ENC-506 dc_engine_host Emscripten module. The module is
 * an ES6 factory (EXPORT_NAME=createDcEngineHost) emitted by the WASM build
 * (core/wasm/dc_engine_host.cpp). It exposes one Embind class, DcEngineHost,
 * whose methods the TS EngineHost wrapper routes to.
 *
 * The .js + .wasm artifacts live in packages/dc-wasm/wasm/. They are built from
 * the C++ core by scripts/build-wasm.sh (documented there). We load the factory
 * lazily so importing @repo/dc-wasm has no side effects until init().
 */

/** Result of DcEngineHost.applyControl — mirrors the C++ value_object. */
export interface DcControlResult {
  ok: boolean;
  error: string;
}

/** Per-frame stats from DcEngineHost.stats() — mirrors the C++ value_object. */
export interface DcEngineStatsRaw {
  frameMs: number;
  drawCalls: number;
  culledDrawCalls: number;
  ingestedBytesThisFrame: number;
  uploadedBytesThisFrame: number;
  activeBuffers: number;
}

/**
 * The Embind DcEngineHost instance surface (the C++ class mirrored to JS). All
 * methods are synchronous from JS's view EXCEPT render()/pick(), which SUSPEND
 * via ASYNCIFY (device acquisition + GPU readback) and therefore return a
 * thenable — we `await` them in the TS wrapper.
 */
export interface DcEngineHostInstance {
  applyControl(jsonText: string): DcControlResult;
  applyDataBatch(bytes: Uint8Array): void;
  render(w: number, h: number): number | Promise<number>;
  pick(w: number, h: number, x: number, y: number): number | Promise<number>;
  dispose(): void;
  framebuffer(): Uint8Array;
  framebufferWidth(): number;
  framebufferHeight(): number;
  renderMessage(): string;
  backend(): string;
  stats(): DcEngineStatsRaw;
  paneCount(): number;
  layerCount(): number;
  drawItemCount(): number;
  bufferCount(): number;
  geometryCount(): number;
  listResources(): string;
  getBufferBytes(bufferId: number): Uint8Array;
  bufferSize(bufferId: number): number;
  delete(): void;
}

/** The instantiated Emscripten Module: the Embind class + the HEAP view. */
export interface DcEngineHostModule {
  DcEngineHost: { new (): DcEngineHostInstance };
  HEAPU8: Uint8Array;
}

/** The default-exported ES6 module factory. */
export type DcEngineHostFactory = (
  overrides?: Record<string, unknown>,
) => Promise<DcEngineHostModule>;

/**
 * Load + instantiate the dc_engine_host WASM module. `factory` lets callers
 * inject the module (e.g. a test or a custom locateFile); when omitted we import
 * the built artifact from ../wasm/dc_engine_host.js relative to this file.
 *
 * `moduleOverrides` is forwarded to the Emscripten factory (e.g. locateFile to
 * point at the .wasm in a bundler, or a custom canvas — unused here since the
 * canvas is bound by the TS wrapper via putImageData, not by the module).
 */
export async function loadDcEngineHost(
  factory?: DcEngineHostFactory,
  moduleOverrides?: Record<string, unknown>,
): Promise<DcEngineHostModule> {
  let f = factory;
  if (!f) {
    // Vite/bundlers and node both resolve this relative ESM import. The .wasm
    // sits next to the .js; Emscripten's default locateFile finds it.
    const mod = (await import("../wasm/dc_engine_host.js")) as {
      default: DcEngineHostFactory;
    };
    f = mod.default;
  }
  return f(moduleOverrides);
}
