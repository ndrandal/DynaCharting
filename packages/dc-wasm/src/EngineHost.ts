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

type EngineErrorCode =
  | "UNKNOWN_PIPELINE"
  | "VALIDATION_NO_GEOMETRY"
  | "WASM_NOT_READY"
  | "CONTROL_REJECTED";

type EngineError = { code: EngineErrorCode; message: string; details?: unknown };

/**
 * Options for constructing the WASM EngineHost. `factory` injects the WASM
 * module (tests / custom bundler wiring); when omitted the built artifact is
 * imported from ../wasm/dc_engine_host.js.
 */
export type EngineHostOptions = {
  hud?: EngineHostHudSink;
  factory?: DcEngineHostFactory;
  moduleOverrides?: Record<string, unknown>;
};

export class EngineHost {
  private hud?: EngineHostHudSink;
  private factory?: DcEngineHostFactory;
  private moduleOverrides?: Record<string, unknown>;

  private canvas: HTMLCanvasElement | null = null;
  private ctx2d: CanvasRenderingContext2D | null = null;

  private module: DcEngineHostModule | null = null;
  private core: DcEngineHostInstance | null = null;
  private ready = false;
  private initPromise: Promise<void> | null = null;

  private running = false;
  private raf = 0;
  private frameDirty = true;

  // Commands/batches buffered until the WASM module is ready (init is async).
  private pendingControl: string[] = [];
  private dataQueue: ArrayBuffer[] = [];
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
    }
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

    // Flush buffered control commands + data batches in order.
    for (const json of this.pendingControl) {
      const r = this.core.applyControl(json);
      if (!r.ok) {
        this.lastErrors.push({ code: "CONTROL_REJECTED", message: r.error });
      }
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
    this.lastErrors = [];
  }

  /** Alias matching engine-host callers that use dispose() naming. */
  dispose(): void {
    this.shutdown();
  }

  // -------------------- data plane --------------------
  /** Queue a binary batch; drained into the WASM core each frame. */
  enqueueData(batch: ArrayBuffer): void {
    if (this.ready && this.core) {
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

    if (!this.ready || !this.core) {
      this.pendingControl.push(json);
      return { ok: true };
    }

    const r = this.core.applyControl(json);
    if (!r.ok) {
      this.lastErrors.push({ code: "CONTROL_REJECTED", message: r.error });
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
    this.applyControl({ cmd: "attachTransform", targetId, transformId });
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
    if (!this.core) return new Uint8Array(0);
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
    if (this.frameDirty && this.ready) {
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

    const w = Math.max(1, canvas.width || canvas.clientWidth || 1);
    const h = Math.max(1, canvas.height || canvas.clientHeight || 1);

    const status = await core.render(w, h);
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
    const bytes = new Uint8ClampedArray(view.slice(0, fbW * fbH * 4));
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
