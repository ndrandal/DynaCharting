/* packages/dc-wasm/src/EngineHost.ts — ENC-506 (P6.5)
 *
 * The WASM-backed EngineHost. Presents the SAME public API as
 * @repo/engine-host's EngineHost (the WebGL2 prototype) so customer-layer can
 * swap `import { EngineHost } from "@repo/engine-host"` for
 * `from "@repo/dc-wasm"` with near-zero change (ENC-507).
 *
 * Internally it loads the dc_engine_host WASM module (DcCore + the full Dawn
 * renderer on emdawnwebgpu) and routes:
 *   init(canvas)            -> load + instantiate WASM, bind the canvas (JS-side)
 *   applyControl(jsonOrObj) -> DcEngineHost.applyControl(jsonText)
 *   applyDataBatch(buffer)  -> DcEngineHost.applyDataBatch(bytes)
 *   enqueueData(buffer)     -> queue, drained into applyDataBatch on render
 *   render frame            -> DcEngineHost.render(w,h) + blit framebuffer -> canvas
 *   pick(x,y)               -> DcEngineHost.pick(w,h,x,y) -> { drawItemId } | null
 *   getStats()              -> DcEngineHost.stats() + JS-queue counters
 *
 * CANVAS BINDING: the WASM renderer renders OFFSCREEN and reads the framebuffer
 * back; this class blits those bytes onto the caller-provided <canvas>'s 2D
 * context via putImageData (the proven dc_webgpu_all path). The external canvas
 * is owned by JS — the WASM module never creates its own.
 *
 * ASYNC NOTE: init(), render and pick are ASYNC under the hood (WASM device
 * acquisition + GPU readback SUSPEND via ASYNCIFY). The engine-host TS API is
 * synchronous, so we keep the SAME synchronous SIGNATURES (init/start/pick
 * return synchronously) and drive the async WASM work on an internal queue:
 *   - init(canvas) kicks off async module load; the render loop and queued
 *     commands wait for it (commands are buffered until ready).
 *   - pick(x,y) returns the LAST KNOWN pick synchronously and refreshes
 *     asynchronously (documented divergence — see pick()).
 * Where a method has no WASM equivalent yet it is a documented stub that keeps
 * the contract (it never throws).
 */

import { PIPELINES, PipelineId, PipelineSpec } from "./pipelines";
import {
  loadDcEngineHost,
  type DcEngineHostFactory,
  type DcEngineHostInstance,
  type DcEngineHostModule,
} from "./wasm";

// ---- Public types: re-exported to MATCH @repo/engine-host exactly ----------
export type PickResult = { drawItemId: number } | null;

export type EngineStats = {
  frameMs: number;
  frameMsP95: number;
  drawCalls: number;

  ingestedBytesThisFrame: number;
  uploadedBytesThisFrame: number;

  activeBuffers: number;

  queuedBatches: number;
  droppedBatches: number;
  droppedBytesThisFrame: number;

  debug: { showBounds: boolean; wireframe: boolean };
};

export type EngineHostHudSink = {
  setFps: (fps: number) => void;
  setGl: (label: string) => void;
  setMem: (label: string) => void;
  setStats?: (s: EngineStats) => void;
  setPick?: (id: number | null) => void;
};

export type TransformParams = {
  tx: number;
  ty: number;
  sx: number;
  sy: number;
};

/**
 * Texture pixel format for setTexturePixels — mirrors the C++ dc::TextureFormat
 * codes (0 = R8 single-channel, 1 = RGBA8). RGBA8 is the default for the
 * texturedQuad@1 colormap escape hatch. (ENC-532)
 */
export const enum TextureFormat {
  R8 = 0,
  RGBA8 = 1,
}

/** A texture upload buffered until the WASM module is ready (init is async). */
type PendingTexture = {
  textureId: number;
  pixels: Uint8Array;
  w: number;
  h: number;
  format: number;
};

type EngineErrorCode =
  | "UNKNOWN_PIPELINE"
  | "VALIDATION_NO_GEOMETRY"
  | "WASM_NOT_READY"
  | "CONTROL_REJECTED";

type EngineError = { code: EngineErrorCode; message: string; details?: unknown };

/**
 * A rejected `applyControl` command, surfaced via
 * `EngineHostOptions.onControlRejected` (ENC-701/G5b). `command` is the JSON
 * the WASM core rejected (e.g. an `ID_TAKEN` create or a bad bind).
 */
export type ControlRejection = {
  code: EngineErrorCode;
  message: string;
  command: string;
};

/**
 * Options for constructing the WASM EngineHost. `factory` injects the WASM
 * module (tests / custom bundler wiring); when omitted the built artifact is
 * imported from ../wasm/dc_engine_host.js.
 */
export type EngineHostOptions = {
  hud?: EngineHostHudSink;
  factory?: DcEngineHostFactory;
  moduleOverrides?: Record<string, unknown>;
  /**
   * Invoked whenever the WASM core REJECTS an applyControl command (e.g.
   * ID_TAKEN, an invalid bind). When omitted, rejections are `console.warn`'d
   * so they are never silently swallowed (ENC-701/G5b). Either way the rejection
   * is also recorded on `getLastErrors()`.
   */
  onControlRejected?: (rejection: ControlRejection) => void;
};

export class EngineHost {
  private hud?: EngineHostHudSink;
  private factory?: DcEngineHostFactory;
  private moduleOverrides?: Record<string, unknown>;
  private onControlRejected?: (rejection: ControlRejection) => void;

  private canvas: HTMLCanvasElement | null = null;
  private ctx2d: CanvasRenderingContext2D | null = null;

  private module: DcEngineHostModule | null = null;
  private core: DcEngineHostInstance | null = null;
  private ready = false;
  private initPromise: Promise<void> | null = null;

  private running = false;
  private raf = 0;
  private frameDirty = true;
  // True while an async core.render() is in flight. The WASM/WebGPU renderer
  // permits only ONE async GPU op at a time; under a high-rate data plane the
  // rAF tick can re-enter renderOnce() before the prior render resolves, which
  // aborts the runtime ("multiple async operations in flight"). This guard
  // serializes renders — a frame that arrives mid-render just leaves frameDirty
  // set so the next tick redraws.
  private rendering = false;

  // Commands/batches buffered until the WASM module is ready (init is async).
  private pendingControl: string[] = [];
  private dataQueue: ArrayBuffer[] = [];
  // Texture uploads buffered until the WASM module is ready (ENC-532).
  private pendingTextures: PendingTexture[] = [];
  private droppedBatches = 0;
  private readonly MAX_QUEUE = 512;

  private lastErrors: EngineError[] = [];
  private lastPick: number | null = null;

  // FPS
  private frames = 0;
  private lastFpsT = 0;

  // Stats
  private stats: EngineStats = {
    frameMs: 0,
    frameMsP95: 0,
    drawCalls: 0,
    ingestedBytesThisFrame: 0,
    uploadedBytesThisFrame: 0,
    activeBuffers: 0,
    queuedBatches: 0,
    droppedBatches: 0,
    droppedBytesThisFrame: 0,
    debug: { showBounds: false, wireframe: false },
  };
  private frameWindow: number[] = [];
  private readonly FRAME_WINDOW_MAX = 240;

  // Track attached transforms so applyControl convenience methods can compose
  // the JSON the WASM core expects (the core owns the authoritative state).
  constructor(hudOrOptions?: EngineHostHudSink | EngineHostOptions) {
    // Accept the engine-host constructor shape `new EngineHost(hud?)` AND an
    // options object (so tests can inject a WASM factory). A bare HUD sink has
    // setFps; an options object does not.
    if (hudOrOptions && "setFps" in hudOrOptions) {
      this.hud = hudOrOptions as EngineHostHudSink;
    } else if (hudOrOptions) {
      const o = hudOrOptions as EngineHostOptions;
      this.hud = o.hud;
      this.factory = o.factory;
      this.moduleOverrides = o.moduleOverrides;
      this.onControlRejected = o.onControlRejected;
    }
  }

  /**
   * Record AND surface a rejected control command. Previously rejections were
   * pushed onto lastErrors and silently dropped by callers (e.g. ID_TAKEN never
   * reached anyone — ENC-701/G5b). Now they additionally fire the
   * onControlRejected callback, or console.warn by default, so they are visible.
   */
  private recordControlRejection(json: string, error: string): void {
    this.lastErrors.push({
      code: "CONTROL_REJECTED",
      message: error,
      details: json,
    });
    if (this.onControlRejected) {
      this.onControlRejected({
        code: "CONTROL_REJECTED",
        message: error,
        command: json,
      });
      return;
    }
    const cmd = json.length > 200 ? `${json.slice(0, 200)}…` : json;
    console.warn(`[dc-wasm] applyControl rejected: ${error} :: ${cmd}`);
  }

  // -------------------- lifecycle --------------------
  /**
   * Bind the caller's canvas and begin loading the WASM module. Matches
   * EngineHost.init(canvas)'s synchronous signature; the actual WASM load is
   * async (ASYNCIFY device acquisition). Commands/batches issued before the
   * module is ready are buffered and flushed on completion. Use ready()/whenReady()
   * to await readiness if needed.
   */
  init(canvas: HTMLCanvasElement): void {
    this.canvas = canvas;
    const ctx = canvas.getContext("2d");
    this.ctx2d = ctx;
    this.hud?.setGl("dc-wasm (WebGPU/Dawn via emdawnwebgpu)");

    this.initPromise = this.loadModule();
    // Surface load failures on the error list rather than as an unhandled
    // rejection; consumers poll getLastErrors().
    this.initPromise.catch((e) => {
      this.lastErrors.push({
        code: "WASM_NOT_READY",
        message: `WASM init failed: ${(e as Error)?.message ?? String(e)}`,
      });
    });
  }

  private async loadModule(): Promise<void> {
    const mod = await loadDcEngineHost(this.factory, this.moduleOverrides);
    this.module = mod;
    this.core = new mod.DcEngineHost();
    this.ready = true;

    // Flush buffered texture uploads first so any control command / draw that
    // references a textureId finds its pixels already present (ENC-532).
    for (const t of this.pendingTextures) {
      this.core.setTexturePixels(t.textureId, t.pixels, t.w, t.h, t.format);
    }
    this.pendingTextures = [];

    // Flush buffered control commands + data batches in order.
    for (const json of this.pendingControl) {
      const r = this.core.applyControl(json);
      if (!r.ok) this.recordControlRejection(json, r.error);
    }
    this.pendingControl = [];
    for (const batch of this.dataQueue) {
      this.core.applyDataBatch(new Uint8Array(batch));
    }
    this.dataQueue = [];
    this.frameDirty = true;
  }

  /** True once the WASM module is loaded and the core exists. */
  isReady(): boolean {
    return this.ready;
  }

  /** Await module readiness (resolves immediately if already ready). */
  whenReady(): Promise<void> {
    return this.initPromise ?? Promise.resolve();
  }

  start(): void {
    if (this.running) return;
    this.running = true;
    this.frames = 0;
    this.lastFpsT = now();
    this.scheduleFrame();
  }

  shutdown(): void {
    this.running = false;
    if (this.raf) cancelAnimationFrame(this.raf);
    this.raf = 0;
    try {
      this.core?.dispose();
      this.core?.delete();
    } catch {
      /* core may already be gone */
    }
    this.core = null;
    this.module = null;
    this.ready = false;
    this.canvas = null;
    this.ctx2d = null;
    this.pendingControl = [];
    this.dataQueue = [];
    this.pendingTextures = [];
    this.lastErrors = [];
  }

  /** Alias matching engine-host callers that use dispose() naming. */
  dispose(): void {
    this.shutdown();
  }

  // -------------------- data plane --------------------
  /** Queue a binary batch; drained into the WASM core each frame. */
  enqueueData(batch: ArrayBuffer): void {
    // Defer while a render is in flight: the WASM core uses ASYNCIFY, which
    // permits only ONE async op at a time. core.render() suspends mid-await;
    // touching the core (applyDataBatch) during that window aborts the runtime
    // ("multiple async operations in flight"). Queued batches are drained after
    // the render resolves (see renderOnce + drainQueued).
    // Apply directly only when nothing is queued ahead of us: data must land
    // after the control commands (scene-init) that create its buffers, and
    // after earlier data, so once any control/data is pending we queue too.
    if (
      this.ready &&
      this.core &&
      !this.rendering &&
      this.pendingControl.length === 0 &&
      this.dataQueue.length === 0
    ) {
      this.core.applyDataBatch(new Uint8Array(batch));
      this.frameDirty = true;
      return;
    }
    if (this.dataQueue.length >= this.MAX_QUEUE) {
      this.droppedBatches++;
      return;
    }
    this.dataQueue.push(batch);
    this.frameDirty = true;
  }

  /** Same as enqueueData (engine-host parity). */
  applyDataBatch(batch: ArrayBuffer): void {
    this.enqueueData(batch);
  }

  // -------------------- textures (ENC-532) --------------------
  /**
   * Upload CPU pixels for a logical textureId so the texturedQuad@1 pipeline can
   * sample them — the escape hatch that lets heatmap/spectrogram/weather views
   * render a producer-rasterized colormap texture. Bind a DrawItem to the
   * textureId via the `setDrawItemTexture` control command. The pixels are
   * tightly packed (RGBA8 = 4 B/px, R8 = 1 B/px), row-major from row 0.
   *
   * Buffered until the WASM module is ready, then replayed (init is async).
   *
   * @param id      logical textureId referenced by setDrawItemTexture
   * @param bytes   tightly-packed pixel bytes
   * @param w       width in pixels
   * @param h       height in pixels
   * @param format  TextureFormat (default RGBA8)
   */
  setTexturePixels(
    id: number,
    bytes: Uint8Array,
    w: number,
    h: number,
    format: TextureFormat = TextureFormat.RGBA8,
  ): void {
    this.frameDirty = true;
    // Apply immediately only when the core is ready AND no render is in flight.
    // The WASM core uses ASYNCIFY (one async op at a time); core.render()
    // suspends mid-await, and touching the core during that window aborts the
    // runtime ("multiple async operations in flight"). When a render is in
    // flight we buffer + drain after it resolves — the same single-flight
    // discipline as enqueueData/applyControl. This is what makes an ANIMATED
    // texture track (setTexturePixels per replay frame, ENC-568) safe.
    if (this.ready && this.core && !this.rendering) {
      this.core.setTexturePixels(id, bytes, w, h, format);
      return;
    }
    // Copy the bytes so a caller-reused buffer can't mutate the buffered upload.
    this.pendingTextures.push({
      textureId: id,
      pixels: bytes.slice(),
      w,
      h,
      format,
    });
  }

  // -------------------- control plane --------------------
  /**
   * Apply one control command. Accepts a JSON string OR an object (engine-host
   * parity); objects are stringified and routed to the WASM core's applyControl
   * (CommandProcessor). Returns { ok } | { ok:false, error }. When the module is
   * still loading the command is buffered (returns ok:true optimistically) and
   * replayed on ready.
   */
  applyControl(
    jsonTextOrObj: string | unknown,
  ): { ok: true } | { ok: false; error: string } {
    let json: string;
    try {
      json =
        typeof jsonTextOrObj === "string"
          ? jsonTextOrObj
          : JSON.stringify(jsonTextOrObj);
    } catch {
      return { ok: false, error: "Control: invalid JSON" };
    }

    this.frameDirty = true;

    // Buffer until ready, or while a render is in flight (ASYNCIFY single-op
    // constraint — see enqueueData). Also buffer whenever controls are already
    // pending: applying directly would jump ahead of the queued commands and
    // break FIFO order (e.g. a runtime setTransform executing before the
    // still-buffered scene-init createTransform → "transform not found").
    // Buffered control replays in order after the render resolves (drainQueued).
    if (!this.ready || !this.core || this.rendering || this.pendingControl.length > 0) {
      this.pendingControl.push(json);
      return { ok: true };
    }

    const r = this.core.applyControl(json);
    if (!r.ok) {
      this.recordControlRejection(json, r.error);
      return { ok: false, error: r.error };
    }
    return { ok: true };
  }

  /** Mark the frame as needing a re-render (after external pan/zoom). */
  markDirty(): void {
    this.frameDirty = true;
  }

  // -------------------- transform convenience (engine-host parity) ----------
  // These compose the JSON the WASM core's CommandProcessor accepts. The core
  // owns the authoritative transform state.
  createTransform(transformId: number): void {
    this.applyControl({ cmd: "createTransform", id: transformId });
  }

  setTransform(transformId: number, params: Partial<TransformParams>): void {
    this.applyControl({ cmd: "setTransform", id: transformId, ...params });
  }

  attachTransform(targetId: number, transformId: number): void {
    // The core's cmdAttachTransform reads `drawItemId` (matching bindDrawItem/
    // setDrawItemColor); emitting `targetId` silently failed to attach, leaving
    // the draw item on the identity transform (geometry rendered off-screen).
    this.applyControl({ cmd: "attachTransform", drawItemId: targetId, transformId });
  }

  // -------------------- buffer/resource convenience (engine-host parity) -----
  createBuffer(bufferId: number): void {
    this.applyControl({ cmd: "createBuffer", id: bufferId });
  }

  deleteDrawItem(id: number): void {
    this.applyControl({ cmd: "delete", kind: "drawItem", id });
  }
  deleteGeometry(id: number): void {
    this.applyControl({ cmd: "delete", kind: "geometry", id });
  }
  deleteTransform(id: number): void {
    this.applyControl({ cmd: "delete", kind: "transform", id });
  }
  deleteBuffer(id: number): void {
    this.applyControl({ cmd: "delete", kind: "buffer", id });
  }

  /** Read back a buffer's CPU bytes (engine-host parity; reads WASM ingest). */
  getBufferBytes(bufferId: number): Uint8Array {
    // Never touch the core while a render is in flight — the ASYNCIFY runtime is
    // parked mid-await and any WASM entry aborts it ("multiple async operations
    // in flight"). Callers just retry (returns empty meanwhile).
    if (!this.core || this.rendering) return new Uint8Array(0);
    // getBufferBytes returns a typed_memory_view into the WASM heap; copy it.
    return Uint8Array.from(this.core.getBufferBytes(bufferId));
  }

  // -------------------- picking --------------------
  /**
   * Pick the DrawItem under the pixel (x,y). DIVERGENCE FROM engine-host: the
   * WASM GPU pick is ASYNC (renders the pick buffer + reads it back via
   * ASYNCIFY), but the engine-host signature is synchronous. We therefore return
   * the LAST KNOWN pick result synchronously and kick off an async refresh
   * (the result lands on the next call + via the HUD setPick sink). Most pick
   * UIs poll on pointer-move, so the one-frame staleness is invisible. Returns
   * null until the first async pick completes or when not ready.
   */
  pick(x: number, y: number): PickResult {
    if (this.ready && this.core && this.canvas) {
      const w = this.canvas.width || 1;
      const h = this.canvas.height || 1;
      // Fire-and-forget async refresh; update lastPick when it resolves.
      Promise.resolve(this.core.pick(w, h, Math.round(x), Math.round(y)))
        .then((id) => {
          this.lastPick = id > 0 ? id : null;
          this.hud?.setPick?.(this.lastPick);
        })
        .catch(() => {
          /* ignore transient pick failures */
        });
    }
    return this.lastPick !== null ? { drawItemId: this.lastPick } : null;
  }

  /** Async pick that resolves to the fresh result (no staleness). */
  async pickAsync(x: number, y: number): Promise<PickResult> {
    if (!this.ready || !this.core || !this.canvas) return null;
    const w = this.canvas.width || 1;
    const h = this.canvas.height || 1;
    const id = await this.core.pick(w, h, Math.round(x), Math.round(y));
    this.lastPick = id > 0 ? id : null;
    return this.lastPick !== null ? { drawItemId: this.lastPick } : null;
  }

  // -------------------- stats / debug --------------------
  getStats(): EngineStats {
    return {
      frameMs: this.stats.frameMs,
      frameMsP95: this.stats.frameMsP95,
      drawCalls: this.stats.drawCalls,
      ingestedBytesThisFrame: this.stats.ingestedBytesThisFrame,
      uploadedBytesThisFrame: this.stats.uploadedBytesThisFrame,
      activeBuffers: this.stats.activeBuffers,
      queuedBatches: this.stats.queuedBatches,
      droppedBatches: this.stats.droppedBatches,
      droppedBytesThisFrame: this.stats.droppedBytesThisFrame,
      debug: { ...this.stats.debug },
    };
  }

  getLastErrors(): EngineError[] {
    return [...this.lastErrors];
  }

  setDebugToggles(toggles: Partial<EngineStats["debug"]>): void {
    this.stats.debug = { ...this.stats.debug, ...toggles };
    // Forward to the core if it understands a debug command (best-effort).
    this.applyControl({ cmd: "setDebug", ...toggles });
  }

  // -------------------- render loop --------------------
  private scheduleFrame(): void {
    const raf =
      typeof requestAnimationFrame === "function"
        ? requestAnimationFrame
        : (cb: FrameRequestCallback) =>
            setTimeout(() => cb(now()), 16) as unknown as number;
    this.raf = raf(this.tick);
  }

  private readonly tick = (t: number): void => {
    if (!this.running) return;
    // Skip if a render is still in flight — re-entering core.render() while the
    // prior async GPU op is pending aborts the runtime. frameDirty stays set so
    // the next tick (after the in-flight render resolves) redraws.
    if (this.frameDirty && this.ready && !this.rendering) {
      this.frameDirty = false;
      void this.renderOnce();
    }
    this.updateHud(t);
    this.scheduleFrame();
  };

  /** Render one frame: WASM renders offscreen, then we blit to the canvas. */
  async renderOnce(): Promise<void> {
    const core = this.core;
    const canvas = this.canvas;
    if (!core || !canvas) return;
    // Single-flight guard: never start a render while one is in flight (the
    // WASM/WebGPU runtime aborts on overlapping async GPU ops).
    if (this.rendering) {
      this.frameDirty = true;
      return;
    }
    this.rendering = true;

    const w = Math.max(1, canvas.width || canvas.clientWidth || 1);
    const h = Math.max(1, canvas.height || canvas.clientHeight || 1);

    let status: number;
    try {
      status = await core.render(w, h);
    } finally {
      this.rendering = false;
      // Drain any control/data that arrived while the render was suspended.
      this.drainQueued();
    }
    if (status !== 1) {
      this.lastErrors.push({
        code: "VALIDATION_NO_GEOMETRY",
        message: `render failed: ${core.renderMessage()}`,
      });
      return;
    }

    this.blitFramebuffer();

    // Pull stats from the core.
    const s = core.stats();
    this.stats.frameMs = s.frameMs;
    this.stats.drawCalls = s.drawCalls;
    this.stats.ingestedBytesThisFrame = s.ingestedBytesThisFrame;
    this.stats.uploadedBytesThisFrame = s.uploadedBytesThisFrame;
    this.stats.activeBuffers = s.activeBuffers;
    this.stats.queuedBatches = this.dataQueue.length;
    this.stats.droppedBatches = this.droppedBatches;

    this.frameWindow.push(s.frameMs);
    if (this.frameWindow.length > this.FRAME_WINDOW_MAX) this.frameWindow.shift();
    this.stats.frameMsP95 = percentile(this.frameWindow, 0.95);

    this.hud?.setStats?.(this.getStats());
  }

  /**
   * Flush textures + control commands + data batches buffered while the core was
   * busy (loading, or a render in flight). Applied in order: textures first (so a
   * drawItem that binds a textureId finds its pixels), then control before data
   * (so a geometry/buffer exists before its bytes land). Safe to call only when
   * no render is in flight (the caller guarantees this).
   */
  private drainQueued(): void {
    if (!this.ready || !this.core || this.rendering) return;
    if (
      this.pendingTextures.length === 0 &&
      this.pendingControl.length === 0 &&
      this.dataQueue.length === 0
    )
      return;
    if (this.pendingTextures.length > 0) {
      const texs = this.pendingTextures;
      this.pendingTextures = [];
      for (const t of texs) {
        this.core.setTexturePixels(t.textureId, t.pixels, t.w, t.h, t.format);
      }
    }
    if (this.pendingControl.length > 0) {
      const ctrl = this.pendingControl;
      this.pendingControl = [];
      for (const json of ctrl) {
        const r = this.core.applyControl(json);
        if (!r.ok) this.recordControlRejection(json, r.error);
      }
    }
    if (this.dataQueue.length > 0) {
      const batches = this.dataQueue;
      this.dataQueue = [];
      for (const batch of batches) this.core.applyDataBatch(new Uint8Array(batch));
    }
    this.frameDirty = true;
  }

  /** Blit the WASM framebuffer bytes onto the caller's canvas 2D context. */
  private blitFramebuffer(): void {
    const core = this.core;
    const ctx = this.ctx2d;
    const canvas = this.canvas;
    if (!core || !ctx || !canvas) return;

    const fbW = core.framebufferWidth();
    const fbH = core.framebufferHeight();
    if (fbW <= 0 || fbH <= 0) return;

    // framebuffer() is a typed_memory_view into the WASM heap; copy it before
    // it can be invalidated by the next render.
    const view = core.framebuffer();
    const src = view.slice(0, fbW * fbH * 4);

    // Y-orientation fix (ENC-696, confirmed by ENC-695). The Dawn scene
    // pipelines render with a clip-space Y negation in every backend shader
    // (see DawnTriSolidBackend kTriSolidWgsl: `return vec4(p.x, -p.y, ...)`,
    // added to match the legacy GL bottom-left readback baseline), while the
    // framebuffer readback (DawnDevice::readFramebufferRGBA) is faithfully
    // top-down. Net effect: an authored clip-y-up vertex lands in the BOTTOM
    // rows of framebuffer(). putImageData is also top-down, so a raw blit
    // renders the whole scene upside-down (high prices at the bottom — masked
    // until now because the only live demo was an orientation-ambiguous line).
    // This is the single place every browser frame is painted to a <canvas>,
    // so flip rows here so clip-up == image-up. (A deeper fix — normalizing the
    // shader Y-negation + readback C++-side — is a larger follow-up that needs
    // re-baselining the Dawn golden PNGs and rebuilding the WASM artifact.)
    const rowBytes = fbW * 4;
    const bytes = new Uint8ClampedArray(src.length);
    for (let y = 0; y < fbH; y++) {
      const srcStart = (fbH - 1 - y) * rowBytes;
      bytes.set(src.subarray(srcStart, srcStart + rowBytes), y * rowBytes);
    }
    const img = new ImageData(bytes, fbW, fbH);
    if (canvas.width !== fbW) canvas.width = fbW;
    if (canvas.height !== fbH) canvas.height = fbH;
    ctx.putImageData(img, 0, 0);
  }

  private updateHud(t: number): void {
    this.frames++;
    const dt = t - this.lastFpsT;
    if (dt >= 250) {
      const fps = (this.frames * 1000) / dt;
      this.hud?.setFps(fps);
      this.hud?.setMem("n/a");
      this.frames = 0;
      this.lastFpsT = t;
    }
  }
}

// -------------------- utils --------------------
function now(): number {
  return typeof performance !== "undefined" ? performance.now() : Date.now();
}

function percentile(values: number[], p: number): number {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const idx = Math.min(
    sorted.length - 1,
    Math.max(0, Math.floor(p * (sorted.length - 1))),
  );
  return sorted[idx];
}

// Touch PIPELINES/PipelineSpec so unused-import lint stays quiet while keeping
// the same pipeline catalog available to consumers via the re-export in index.ts.
const _PIPELINES_REF: Record<PipelineId, PipelineSpec> = PIPELINES;
void _PIPELINES_REF;
