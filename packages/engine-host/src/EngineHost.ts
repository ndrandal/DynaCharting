export type EngineHostHudSink = {
  setFps: (fps: number) => void;
  setGl: (label: string) => void;
  setMem: (label: string) => void;
};

export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  private frames = 0;
  private lastFpsT = 0;

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

    // optional hard cleanup test:
    // const ext = this.gl?.getExtension("WEBGL_lose_context");
    // ext?.loseContext();

    this.gl = null;
    this.canvas = null;
  }

  renderOnce() {
    if (!this.gl) return;
    this.gl.clear(this.gl.COLOR_BUFFER_BIT);
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
