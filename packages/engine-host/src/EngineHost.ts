export type EngineStats = {
  frameMs: number;
  drawCalls: number;
  uploadedBytesThisFrame: number;
  activeBuffers: number;
  debug: {
    showBounds: boolean;
    wireframe: boolean;
  };
};

export type EngineHostHudSink = {
  setFps: (fps: number) => void;
  setGl: (label: string) => void;
  setMem: (label: string) => void;

  // NEW (optional): one-shot stats update
  setStats?: (s: EngineStats) => void;
};

export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  private frames = 0;
  private lastFpsT = 0;

  // ---- D10.1 stats ----
  private stats: EngineStats = {
    frameMs: 0,
    drawCalls: 0,
    uploadedBytesThisFrame: 0,
    activeBuffers: 0,
    debug: {
      showBounds: false,
      wireframe: false
    }
  };

  // Count uploads that happen “between frames” into the next frame.
  private pendingUploadBytes = 0;

  // Minimal GPU buffer store so we can prove uploaded bytes change.
  private buffers = new Map<number, WebGLBuffer>();

  constructor(private hud?: EngineHostHudSink) {}

  init(canvas: HTMLCanvasElement) {
    this.canvas = canvas;

    const gl = canvas.getContext("webgl2", {
      antialias: false,
      alpha: false,
      depth: false,
      stencil: false,
      preserveDrawingBuffer: false,
      powerPreference: "high-performance"
    });

    if (!gl) throw new Error("WebGL2 not available.");

    this.gl = gl;

    // baseline GL state
    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.BLEND);
    gl.clearColor(0.02, 0.07, 0.10, 1.0);

    this.hud?.setGl(gl.getParameter(gl.VERSION) as string);

    this.onResize();
    window.addEventListener("resize", this.onResize, { passive: true });
  }

  start() {
    if (!this.gl) throw new Error("EngineHost.start(): call init(canvas) first.");
    if (this.running) return;

    this.running = true;
    this.frames = 0;
    this.lastFpsT = performance.now();
    this.raf = requestAnimationFrame(this.tick);
  }

  shutdown() {
    this.running = false;
    cancelAnimationFrame(this.raf);
    this.raf = 0;

    window.removeEventListener("resize", this.onResize);

    // cleanup GPU objects
    if (this.gl) {
      for (const b of this.buffers.values()) this.gl.deleteBuffer(b);
    }
    this.buffers.clear();

    this.gl = null;
    this.canvas = null;
  }

  // ---------- D10.1 public API ----------

  /** Snapshot of latest frame stats (updated once per renderOnce). */
  getStats(): EngineStats {
    // Return a copy to avoid external mutation.
    return {
      frameMs: this.stats.frameMs,
      drawCalls: this.stats.drawCalls,
      uploadedBytesThisFrame: this.stats.uploadedBytesThisFrame,
      activeBuffers: this.stats.activeBuffers,
      debug: { ...this.stats.debug }
    };
  }

  setDebugToggles(toggles: Partial<EngineStats["debug"]>) {
    this.stats.debug = { ...this.stats.debug, ...toggles };
  }

  /**
   * Create or update a GPU buffer by numeric id.
   * Counts uploaded bytes *this frame* (or next frame if called between frames).
   */
  uploadBuffer(id: number, data: ArrayBufferView, usage: number = 0x88e4 /* gl.DYNAMIC_DRAW */) {
    const gl = this.gl;
    if (!gl) return;

    let b = this.buffers.get(id);
    if (!b) {
      b = gl.createBuffer();
      if (!b) throw new Error("Failed to create WebGLBuffer");
      this.buffers.set(id, b);
    }

    gl.bindBuffer(gl.ARRAY_BUFFER, b);
    gl.bufferData(gl.ARRAY_BUFFER, data, usage);

    this.pendingUploadBytes += data.byteLength;
    this.stats.activeBuffers = this.buffers.size;
  }

  deleteBuffer(id: number) {
    const gl = this.gl;
    if (!gl) return;
    const b = this.buffers.get(id);
    if (!b) return;
    gl.deleteBuffer(b);
    this.buffers.delete(id);
    this.stats.activeBuffers = this.buffers.size;
  }

  // ---------- render loop ----------

  private readonly tick = (t: number) => {
    if (!this.running) return;

    this.renderOnce();
    this.updateHud(t);

    this.raf = requestAnimationFrame(this.tick);
  };

  private readonly onResize = () => {
    if (!this.canvas || !this.gl) return;

    const dpr = Math.max(1, window.devicePixelRatio || 1);
    const rect = this.canvas.getBoundingClientRect();

    const w = Math.max(1, Math.floor(rect.width * dpr));
    const h = Math.max(1, Math.floor(rect.height * dpr));

    if (this.canvas.width !== w || this.canvas.height !== h) {
      this.canvas.width = w;
      this.canvas.height = h;
      this.gl.viewport(0, 0, w, h);
    }
  };

  renderOnce() {
    const gl = this.gl;
    if (!gl) return;

    const t0 = performance.now();

    // Reset per-frame counters
    this.stats.drawCalls = 0;
    this.stats.uploadedBytesThisFrame = this.pendingUploadBytes;
    this.pendingUploadBytes = 0;

    // Clear (no draw calls counted for clear)
    gl.clear(gl.COLOR_BUFFER_BIT);

    // Future: render scene, increment drawCalls, optionally draw bounds, etc.

    const t1 = performance.now();
    this.stats.frameMs = t1 - t0;

    // Push stats to overlay sink if present
    this.hud?.setStats?.(this.getStats());
  }

  private updateHud(t: number) {
    this.frames++;

    const dt = t - this.lastFpsT;
    if (dt >= 250) {
      const fps = (this.frames * 1000) / dt;
      this.hud?.setFps(fps);

      const pm = (performance as any)?.memory;
      if (pm && typeof pm.usedJSHeapSize === "number") {
        const usedMB = pm.usedJSHeapSize / (1024 * 1024);
        const totalMB = pm.totalJSHeapSize / (1024 * 1024);
        this.hud?.setMem(`${usedMB.toFixed(1)} / ${totalMB.toFixed(1)} MB`);
      } else {
        this.hud?.setMem("n/a");
      }

      this.frames = 0;
      this.lastFpsT = t;
    }
  }
}
