import type { EngineHost } from "@repo/engine-host";

export type WorkerSubscription = {
  kind: "workerStream";
  stream: string;  // "lineSine" | ...
  bufferId: number;
};

export type RecipeBuildResult = {
  commands: any[];
  dispose: any[];
  subscriptions: WorkerSubscription[];
};

export type RecipeBuilder = (cfg: { idBase: number }) => RecipeBuildResult;

export type PolicyMode = "raw" | "agg";

export type ChartControllerOptions = {
  idStride?: number;       // size of id block per mount
  tickMs?: number;         // worker tick for fake mode
  policyDebounceMs?: number;

  // hysteresis thresholds on sx (zoom)
  rawOnZoom?: number;      // switch to raw when sx >= this
  aggOnZoom?: number;      // switch to agg when sx <= this

  // retention caps per mode (bytes per buffer)
  rawMaxBytes?: number;
  aggMaxBytes?: number;

  // optional: shrink immediately when switching to agg
  shrinkOnAgg?: boolean;
};

export class ChartController {
  private current: { idBase: number; recipe: RecipeBuildResult } | null = null;

  private nextBase = 10000;
  private stride: number;

  // View transform params (clip space)
  private tx = 0;
  private ty = 0;
  private sx = 1;
  private sy = 1;

  // Drag state
  private dragging = false;
  private lastClientX = 0;
  private lastClientY = 0;

  // Policy
  private mode: PolicyMode = "raw";
  private policyTimer: number | null = null;

  // Options
  private tickMs: number;
  private policyDebounceMs: number;
  private rawOnZoom: number;
  private aggOnZoom: number;
  private rawMaxBytes: number;
  private aggMaxBytes: number;
  private shrinkOnAgg: boolean;

  constructor(
    private host: EngineHost,
    private worker: Worker,
    opts: ChartControllerOptions = {}
  ) {
    this.stride = Math.max(1000, opts.idStride ?? 10000);
    this.tickMs = Math.max(16, opts.tickMs ?? 33);

    this.policyDebounceMs = Math.max(0, opts.policyDebounceMs ?? 120);
    this.rawOnZoom = opts.rawOnZoom ?? 1.25;
    this.aggOnZoom = opts.aggOnZoom ?? 0.85;

    this.rawMaxBytes = opts.rawMaxBytes ?? (4 * 1024 * 1024);
    this.aggMaxBytes = opts.aggMaxBytes ?? (1 * 1024 * 1024);

    this.shrinkOnAgg = opts.shrinkOnAgg ?? true;
  }

  // -------------------- recipe lifecycle --------------------
  mount(builder: RecipeBuilder): void {
    if (this.current) this.unmount();

    const idBase = this.nextBase;
    this.nextBase += this.stride;

    const recipe = builder({ idBase });

    // Apply creation commands
    for (const c of recipe.commands) this.must(this.host.applyControl(c));

    // Start fake streams (or server can ignore it; worker decides mode)
    this.worker.postMessage({
      type: "startStreams",
      tickMs: this.tickMs,
      streams: recipe.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
    });

    // Initial policy: raw
    this.current = { idBase, recipe };
    this.mode = "raw";
    this.worker.postMessage({ type: "updatePolicy", mode: "raw" });

    // Initial retention caps
    this.applyRetentionCaps(this.mode);

    // Reset view and push transform
    this.setViewTransform(0, 0, 1, 1);

    // In case zoom changes rapidly right after mount
    this.schedulePolicyUpdate();
  }

  unmount(): void {
    if (!this.current) return;

    this.cancelPolicyTimer();

    // stop worker first
    this.worker.postMessage({ type: "stop" });

    // dispose commands
    for (const c of this.current.recipe.dispose) this.must(this.host.applyControl(c));

    this.current = null;

    // reset state
    this.dragging = false;
    this.tx = 0; this.ty = 0; this.sx = 1; this.sy = 1;
    this.mode = "raw";
  }

  isMounted(): boolean {
    return !!this.current;
  }

  getIdBase(): number | null {
    return this.current?.idBase ?? null;
  }

  // -------------------- server switching (D5.2) --------------------
  connectServer(url: string, auth?: any, protocols?: string | string[]): void {
    // worker will stop fake loops internally; host/core unchanged
    this.worker.postMessage({ type: "connectWs", url, auth, protocols, tickMs: 16 });
    // send current policy immediately
    this.worker.postMessage({ type: "updatePolicy", mode: this.mode });
  }

  disconnectServer(): void {
    this.worker.postMessage({ type: "disconnectWs" });
    // optionally return to fake streams if recipe is mounted
    if (this.current) {
      this.worker.postMessage({
        type: "startStreams",
        tickMs: this.tickMs,
        streams: this.current.recipe.subscriptions.map((s) => ({ stream: s.stream, bufferId: s.bufferId }))
      });
      this.worker.postMessage({ type: "updatePolicy", mode: this.mode });
    }
  }

  // -------------------- view transform (D1.5) --------------------
  setViewTransform(tx: number, ty: number, sx: number, sy: number): void {
    if (!this.current) return;

    this.tx = tx;
    this.ty = ty;
    this.sx = sx;
    this.sy = sy;

    this.applyViewTransform();
    this.schedulePolicyUpdate();
  }

  resetView(): void {
    this.setViewTransform(0, 0, 1, 1);
  }

  onPointerDown(clientX: number, clientY: number): void {
    if (!this.current) return;
    this.dragging = true;
    this.lastClientX = clientX;
    this.lastClientY = clientY;
  }

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

    // pan does not change policy, but updating transform should remain instant
    this.applyViewTransform();
  }

  onPointerUp(): void {
    this.dragging = false;
  }

  onWheel(deltaY: number): void {
    if (!this.current) return;

    const zoom = Math.exp(-deltaY * 0.001);
    this.sx *= zoom;
    this.sy *= zoom;

    this.applyViewTransform();
    this.schedulePolicyUpdate();
  }

  // Convenience for demos (React should call the methods directly)
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

  // -------------------- D6.1 policy selection --------------------
  private chooseModeFromZoom(): PolicyMode {
    const z = this.sx;

    // hysteresis: don’t flap around 1.0
    if (this.mode === "raw") {
      if (z <= this.aggOnZoom) return "agg";
      return "raw";
    } else {
      if (z >= this.rawOnZoom) return "raw";
      return "agg";
    }
  }

  private schedulePolicyUpdate(): void {
    if (!this.current) return;

    this.cancelPolicyTimer();

    if (this.policyDebounceMs === 0) {
      this.applyPolicyNow();
      return;
    }

    this.policyTimer = setTimeout(() => {
      this.policyTimer = null;
      this.applyPolicyNow();
    }, this.policyDebounceMs) as unknown as number;
  }

  private cancelPolicyTimer(): void {
    if (this.policyTimer !== null) {
      clearTimeout(this.policyTimer);
      this.policyTimer = null;
    }
  }

  private applyPolicyNow(): void {
    if (!this.current) return;

    const next = this.chooseModeFromZoom();
    if (next === this.mode) return;

    this.mode = next;

    // 1) tell worker (stub density OR server policy forwarding)
    this.worker.postMessage({ type: "updatePolicy", mode: this.mode });

    // 2) retention caps (core cache policy)
    this.applyRetentionCaps(this.mode);
  }

  private applyRetentionCaps(mode: PolicyMode): void {
    if (!this.current) return;

    const maxBytes = mode === "raw" ? this.rawMaxBytes : this.aggMaxBytes;

    for (const s of this.current.recipe.subscriptions) {
      this.host.applyControl({ cmd: "bufferSetMaxBytes", id: s.bufferId, maxBytes });
    }

    if (mode === "agg" && this.shrinkOnAgg) {
      // force shrink immediately to prevent any “tail” from raw mode lingering
      for (const s of this.current.recipe.subscriptions) {
        this.host.applyControl({ cmd: "bufferKeepLast", id: s.bufferId, bytes: maxBytes });
      }
    }
  }

  // -------------------- internals --------------------
  private viewTransformId(): number | null {
    // Convention: view transform = idBase + 2000 (matches your recipe)
    if (!this.current) return null;
    return this.current.idBase + 2000;
  }

  private applyViewTransform(): void {
    const id = this.viewTransformId();
    if (id === null) return;

    // D1.5 pass criteria: ONLY transform changes during pan/zoom
    this.host.applyControl({ cmd: "setTransform", id, tx: this.tx, ty: this.ty, sx: this.sx, sy: this.sy });
  }

  private must(r: { ok: true } | { ok: false; error: string }) {
    if (!r.ok) throw new Error(r.error);
  }
}
