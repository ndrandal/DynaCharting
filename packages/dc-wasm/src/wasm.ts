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
  /** CPU wall-clock inside DawnSceneRenderer::render — scene walk + draw encode
   *  + submit. NOT a GPU time and NOT a frame budget. ENC-1265; it replaces
   *  `frameMs`, which was assigned by nothing in core/ and read 0.0 forever. */
  renderCpuMs: number;
  /** CPU wall-clock of readFramebufferRGBA — the full-target RGBA8 copy back to
   *  the heap that precedes the canvas blit. ENC-1265. */
  readbackMs: number;
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
  /**
   * Upload CPU pixels for a logical textureId so the texturedQuad@1 pipeline can
   * sample them (heatmap/spectrogram/weather colormaps). `format` is a
   * dc::TextureFormat code: 0 = R8, 1 = RGBA8. (ENC-532)
   */
  setTexturePixels(
    textureId: number,
    pixels: Uint8Array,
    w: number,
    h: number,
    format: number,
  ): void;
  /**
   * Load a TTF/OTF font into the SDF glyph atlas (textSDF@1). Must be called
   * once before setTextGeometry. Returns true on a successful load. (ENC-589)
   */
  loadFont(fontBytes: Uint8Array): boolean;
  /**
   * Lay out `text` at clip-space (clipX, clipY) baseline into the Glyph8 instance
   * buffer `bufferId` and set geometry `geometryId`'s vertexCount to the glyph
   * count, so a textSDF@1 DrawItem bound to it renders positioned text. The
   * pane/layer/drawItem/buffer/geometry(glyph8)/bind(textSDF@1)/color scaffolding
   * is created via applyControl first. Returns the glyph instance count written,
   * or -1 if no font has been loaded. (ENC-589)
   */
  setTextGeometry(
    bufferId: number,
    geometryId: number,
    text: string,
    clipX: number,
    clipY: number,
    fontSize: number,
  ): number;
  /**
   * `setTextGeometry` with an explicit horizontal scale (ENC-1253). Clip space
   * is not square — one clip unit is `w/2` pixels across and `h/2` down — so the
   * single isotropic scale `setTextGeometry` can express renders every string
   * stretched by the canvas's aspect ratio. Pass `xScale = height/width` for
   * glyphs with the font's own proportions; `setTextGeometry` is this with
   * `xScale = 1`, so no existing caller's output moves.
   */
  setTextGeometryX(
    bufferId: number,
    geometryId: number,
    text: string,
    clipX: number,
    clipY: number,
    fontSize: number,
    xScale: number,
  ): number;
  /**
   * Measure `text` WITHOUT drawing it, running the same `dc::layoutText` loop
   * `setTextGeometry` runs (ENC-1253). Every field is in CLIP units, relative to
   * a baseline-left origin; `glyphCount` is -1 with no font loaded. This is what
   * makes right-aligning a price label, centring a time label, and asserting
   * that label boxes are disjoint possible from JS — the glyph quads themselves
   * go into the RENDER store, which `getBufferBytes` (an ingest reader) cannot
   * see.
   */
  measureText(
    text: string,
    fontSize: number,
    xScale: number,
  ): {
    advanceWidth: number;
    inkMinX: number;
    inkMaxX: number;
    inkMinY: number;
    inkMaxY: number;
    glyphCount: number;
    glyphPx: number;
  };
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
  /**
   * Export the LIVE scene as SceneDocument JSON — the structural half of a
   * save/snapshot (ENC-984). Returns a COPIED JS string, unlike
   * getBufferBytes()/framebuffer() which hand back a live view into the WASM
   * heap that the next call can invalidate; a document is meant to be held, so
   * it is copied at the boundary and is safe to keep across renders.
   *
   * Buffer BYTES are not inlined — the document carries each buffer's
   * byteLength and you read the contents with getBufferBytes(id).
   */
  getSceneDocument(compact: boolean): string;
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
