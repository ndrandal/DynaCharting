import type { EngineHost } from "@repo/engine-host";

export type WorkerSubscription = {
  kind: "workerStream";
  stream: string;
  bufferId: number;
};

export type RecipeBuildResult = {
  commands: any[];
  dispose: any[];
  subscriptions: WorkerSubscription[];
};

export type RecipeBuilder = (cfg: { idBase: number }) => RecipeBuildResult;

export type ChartControllerOptions = {
  idStride?: number;   // size of ID block reserved per mount
  tickMs?: number;     // worker tick
};

export class ChartController {
  private current: { idBase: number; recipe: RecipeBuildResult } | null = null;

  // ID management is “block-based”: each mount gets a stable idBase
  private nextBase = 10000;
  private readonly stride: number;

  // Transform state (uniform-only). Controller owns it.
  private tx = 0;
  private ty = 0;
  private sx = 1;
  private sy = 1;

  // Drag tracking
  private dragging = false;
  private lastClientX = 0;
  private lastClientY = 0;

  private tickMs: number;

  constructor(
    private host: EngineHost,
    private worker: Worker,
    opts: ChartControllerOptions = {}
  ) {
    this.stride = Math.max(1000, opts.idStride ?? 10000);
    this.tickMs = Math.max(16, opts.tickMs ?? 33);
  }

  // -------------------- Recipe management --------------------
  mount(builder: RecipeBuilder): void {
    // Unmount any existing recipe first (keeps UI simple)
    if (this.current) this.unmount();

    const idBase = this.nextBase;
    this.nextBase += this.stride;

    const recipe = builder({ idBase });

    // Apply commands
    for (const c of recipe.commands) this.must(this.host.applyControl(c));

    // Start worker subscriptions
    this.worker.postMessage({
      type: "startStreams",
      tickMs: this.tickMs,
      streams: recipe.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
    });

    this.current = { idBase, recipe };

    // Reset view each mount (optional but makes behavior predictable)
    this.setViewTransform(0, 0, 1, 1);

    // If recipe created view transform in the standard slot, push it now:
    // (By convention: view transform = idBase + 2000)
    this.applyViewTransform();
  }

  unmount(): void {
    if (!this.current) return;

    // Stop worker first so no more updates arrive for deleted buffers
    this.worker.postMessage({ type: "stop" });

    // Dispose
    const { recipe } = this.current;
    for (const c of recipe.dispose) this.must(this.host.applyControl(c));

    this.current = null;

    // Reset controller state
    this.dragging = false;
    this.tx = 0; this.ty = 0; this.sx = 1; this.sy = 1;
  }

  isMounted(): boolean {
    return !!this.current;
  }

  connectServer(url: string, auth?: any) {
    this.worker.postMessage({ type: "connectWs", url, auth, tickMs: 16 });
  }

  disconnectServer() {
    this.worker.postMessage({ type: "disconnectWs" });
  }


  getIdBase(): number | null {
    return this.current?.idBase ?? null;
  }

  // -------------------- Pan/Zoom (transform updates only) --------------------
  /**
   * Directly set transform parameters (clip-space affine).
   * In React, call this from your gesture logic.
   */
  setViewTransform(tx: number, ty: number, sx: number, sy: number): void {
    this.tx = tx;
    this.ty = ty;
    this.sx = sx;
    this.sy = sy;
    this.applyViewTransform();
  }

  /**
   * UI helper: call on pointer down.
   */
  onPointerDown(clientX: number, clientY: number): void {
    if (!this.current) return;
    this.dragging = true;
    this.lastClientX = clientX;
    this.lastClientY = clientY;
  }

  /**
   * UI helper: call on pointer move with canvas size (CSS pixels).
   * dx/dy are converted to clip space and applied to tx/ty.
   */
  onPointerMove(clientX: number, clientY: number, canvasCssWidth: number, canvasCssHeight: number): void {
    if (!this.current) return;
    if (!this.dragging) return;

    const dxPx = clientX - this.lastClientX;
    const dyPx = clientY - this.lastClientY;
    this.lastClientX = clientX;
    this.lastClientY = clientY;

    const w = Math.max(1, canvasCssWidth);
    const h = Math.max(1, canvasCssHeight);

    const dxClip = (dxPx / w) * 2;
    const dyClip = -(dyPx / h) * 2;

    this.tx += dxClip;
    this.ty += dyClip;

    this.applyViewTransform();
  }

  onPointerUp(): void {
    this.dragging = false;
  }

  /**
   * UI helper: wheel zoom around center (uniform-only).
   * If you want cursor-anchored zoom later, we’ll upgrade.
   */
  onWheel(deltaY: number): void {
    if (!this.current) return;
    const zoom = Math.exp(-deltaY * 0.001);
    this.sx *= zoom;
    this.sy *= zoom;
    this.applyViewTransform();
  }

  resetView(): void {
    if (!this.current) return;
    this.tx = 0; this.ty = 0; this.sx = 1; this.sy = 1;
    this.applyViewTransform();
  }

  // -------------------- DOM binding helper (demo only) --------------------
  /**
   * Not required for React, but useful for your demo app:
   * controller.bindCanvas(canvas) wires mouse + wheel to controller methods.
   */
  bindCanvas(canvas: HTMLCanvasElement): () => void {
    const onDown = (e: MouseEvent) => this.onPointerDown(e.clientX, e.clientY);
    const onUp = () => this.onPointerUp();
    const onMove = (e: MouseEvent) => {
      const r = canvas.getBoundingClientRect();
      this.onPointerMove(e.clientX, e.clientY, r.width, r.height);
    };
    const onWheel = (e: WheelEvent) => {
      e.preventDefault();
      this.onWheel(e.deltaY);
    };

    canvas.addEventListener("mousedown", onDown);
    window.addEventListener("mouseup", onUp);
    window.addEventListener("mousemove", onMove);
    canvas.addEventListener("wheel", onWheel, { passive: false });

    return () => {
      canvas.removeEventListener("mousedown", onDown);
      window.removeEventListener("mouseup", onUp);
      window.removeEventListener("mousemove", onMove);
      canvas.removeEventListener("wheel", onWheel as any);
    };
  }

  // -------------------- Internals --------------------
  private viewTransformId(): number | null {
    // Convention used by your recipe: T_VIEW = idBase + 2000
    if (!this.current) return null;
    return this.current.idBase + 2000;
  }

  private applyViewTransform(): void {
    const id = this.viewTransformId();
    if (id === null) return;

    // Transform updates only — no buffer mutations
    this.host.applyControl({ cmd: "setTransform", id, tx: this.tx, ty: this.ty, sx: this.sx, sy: this.sy });
  }

  private must(r: { ok: true } | { ok: false; error: string }) {
    if (!r.ok) throw new Error(r.error);
  }
}
