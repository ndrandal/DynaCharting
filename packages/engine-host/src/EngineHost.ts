export type PickResult = { drawItemId: number } | null;

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

  // your hud.ts already implements setStats — make it official here
  setStats?: (s: EngineStats) => void;

  // optional (you already have it)
  setPick?: (id: number | null) => void;
};

type CpuGpuBuffer = {
  id: number;
  cpu: Uint8Array;          // authoritative bytes
  gl: WebGLBuffer;
  gpuByteLength: number;    // allocated size on GPU
};

type Geometry = {
  id: number;
  vertexBufferId: number;
  vertexCount: number;      // number of vertices (vec2)
  format: "pos2_clip";
};

type DrawItem = {
  id: number;
  geometryId: number;
  pipeline: "flat";
};

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  private frames = 0;
  private lastFpsT = 0;

  private stats: EngineStats = {
    frameMs: 0,
    drawCalls: 0,
    uploadedBytesThisFrame: 0,
    activeBuffers: 0,
    debug: { showBounds: false, wireframe: false }
  };

  // Bytes staged during the JS tick, applied to stats on renderOnce()
  private pendingUploadBytes = 0;

  // ---- Minimal scene state ----
  private buffers = new Map<number, CpuGpuBuffer>();
  private geometries = new Map<number, Geometry>();
  private drawItems = new Map<number, DrawItem>();

  // ---- Minimal shader for pos2_clip triangles ----
  private prog: WebGLProgram | null = null;
  private aPosLoc: number = -1;

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

    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.BLEND);
    gl.clearColor(0.02, 0.07, 0.10, 1.0);

    this.hud?.setGl(gl.getParameter(gl.VERSION) as string);

    this.prog = this.createProgram(
      `#version 300 es
       precision highp float;
       in vec2 aPos;
       void main() {
         gl_Position = vec4(aPos, 0.0, 1.0);
       }`,
      `#version 300 es
       precision highp float;
       out vec4 outColor;
       void main() {
         outColor = vec4(0.85, 0.15, 0.20, 1.0);
       }`
    );
    this.aPosLoc = gl.getAttribLocation(this.prog, "aPos");

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

    const gl = this.gl;
    if (gl) {
      for (const b of this.buffers.values()) gl.deleteBuffer(b.gl);
      if (this.prog) gl.deleteProgram(this.prog);
    }

    this.buffers.clear();
    this.geometries.clear();
    this.drawItems.clear();
    this.prog = null;

    this.gl = null;
    this.canvas = null;
  }

  // ============================================================
  // D3.1 — Control plane (JSON)
  // ============================================================

  /**
   * Minimal control-plane command dispatcher.
   * Supported:
   *  - createBuffer {id}
   *  - createGeometry {id, vertexBufferId, format:"pos2_clip"}
   *  - createDrawItem {id, geometryId, pipeline?}
   */
  applyControl(jsonTextOrObj: string | any): { ok: true } | { ok: false; error: string } {
    let obj: any;
    try {
      obj = typeof jsonTextOrObj === "string" ? JSON.parse(jsonTextOrObj) : jsonTextOrObj;
    } catch {
      return { ok: false, error: "Control: invalid JSON" };
    }

    const cmd = obj?.cmd;
    if (typeof cmd !== "string") return { ok: false, error: "Control: missing cmd" };

    try {
      if (cmd === "createBuffer") {
        const id = toU32(obj.id);
        if (!id) throw new Error("createBuffer: missing id");
        this.ensureBuffer(id);
        return { ok: true };
      }

      if (cmd === "createGeometry") {
        const id = toU32(obj.id);
        const vb = toU32(obj.vertexBufferId);
        const fmt = obj.format ?? "pos2_clip";
        if (!id || !vb) throw new Error("createGeometry: missing id or vertexBufferId");
        if (fmt !== "pos2_clip") throw new Error("createGeometry: only format=pos2_clip supported");
        this.ensureBuffer(vb);

        this.geometries.set(id, {
          id,
          vertexBufferId: vb,
          vertexCount: 0,
          format: "pos2_clip"
        });
        return { ok: true };
      }

      if (cmd === "createDrawItem") {
        const id = toU32(obj.id);
        const gid = toU32(obj.geometryId);
        const pipe = (obj.pipeline ?? "flat") as "flat";
        if (!id || !gid) throw new Error("createDrawItem: missing id or geometryId");
        if (!this.geometries.has(gid)) throw new Error("createDrawItem: geometryId not found");
        if (pipe !== "flat") throw new Error("createDrawItem: only pipeline=flat supported in v1");

        this.drawItems.set(id, { id, geometryId: gid, pipeline: "flat" });
        return { ok: true };
      }

      return { ok: false, error: `Control: unknown cmd ${cmd}` };
    } catch (e: any) {
      return { ok: false, error: e?.message ?? String(e) };
    }
  }

  // ============================================================
  // D3.1 — Data plane (binary batches)
  // ============================================================

  /**
   * Apply a binary batch containing bufferAppend / bufferUpdateRange records.
   * Format (little-endian):
   *  u8 op (1 append, 2 updateRange)
   *  u32 bufferId
   *  u32 offsetBytes (updateRange only; 0 for append)
   *  u32 payloadBytes
   *  payloadBytes bytes
   */
  applyDataBatch(batch: ArrayBuffer) {
    const gl = this.gl;
    if (!gl) return;

    const dv = new DataView(batch);
    let p = 0;

    while (p < dv.byteLength) {
      if (p + 1 + 4 + 4 + 4 > dv.byteLength) {
        throw new Error("DataBatch: truncated header");
      }

      const op = dv.getUint8(p); p += 1;
      const bufferId = dv.getUint32(p, true); p += 4;
      const offsetBytes = dv.getUint32(p, true); p += 4;
      const payloadBytes = dv.getUint32(p, true); p += 4;

      if (p + payloadBytes > dv.byteLength) {
        throw new Error("DataBatch: truncated payload");
      }

      const payload = new Uint8Array(batch, p, payloadBytes);
      p += payloadBytes;

      if (op === OP_APPEND) {
        this.bufferAppend(bufferId, payload);
      } else if (op === OP_UPDATE_RANGE) {
        this.bufferUpdateRange(bufferId, offsetBytes, payload);
      } else {
        throw new Error(`DataBatch: unknown op ${op}`);
      }
    }
  }

  private bufferAppend(bufferId: number, payload: Uint8Array) {
    const b = this.ensureBuffer(bufferId);

    const oldLen = b.cpu.byteLength;
    const newLen = oldLen + payload.byteLength;

    const next = new Uint8Array(newLen);
    next.set(b.cpu, 0);
    next.set(payload, oldLen);
    b.cpu = next;

    // Grow GPU allocation if needed
    this.syncGpuBuffer(b, /*forceRealloc*/ newLen > b.gpuByteLength);

    // Count “uploaded bytes” as payload size (good enough for pass criteria)
    this.pendingUploadBytes += payload.byteLength;

    // Any geometry using this buffer extends automatically
    this.refreshGeometriesForBuffer(bufferId);
  }

  private bufferUpdateRange(bufferId: number, offsetBytes: number, payload: Uint8Array) {
    const b = this.ensureBuffer(bufferId);

    const end = offsetBytes + payload.byteLength;
    if (end > b.cpu.byteLength) {
      // v1: allow implicit growth via zero-fill
      const next = new Uint8Array(end);
      next.set(b.cpu, 0);
      b.cpu = next;
    }

    b.cpu.set(payload, offsetBytes);

    // Ensure GPU big enough
    const needsRealloc = b.cpu.byteLength > b.gpuByteLength;
    this.syncGpuBuffer(b, needsRealloc, offsetBytes, payload);

    this.pendingUploadBytes += payload.byteLength;
    this.refreshGeometriesForBuffer(bufferId);
  }

  private refreshGeometriesForBuffer(bufferId: number) {
    // pos2_clip = 2 floats = 8 bytes/vertex
    const b = this.buffers.get(bufferId)!;
    const vertexCount = Math.floor(b.cpu.byteLength / 8);

    for (const g of this.geometries.values()) {
      if (g.vertexBufferId === bufferId) {
        g.vertexCount = vertexCount;
      }
    }
  }

  private ensureBuffer(id: number): CpuGpuBuffer {
    const gl = this.gl;
    if (!gl) throw new Error("ensureBuffer: gl not ready");

    let b = this.buffers.get(id);
    if (b) return b;

    const glb = gl.createBuffer();
    if (!glb) throw new Error("Failed to create WebGLBuffer");

    b = {
      id,
      cpu: new Uint8Array(0),
      gl: glb,
      gpuByteLength: 0
    };
    this.buffers.set(id, b);

    this.stats.activeBuffers = this.buffers.size;
    return b;
  }

  private syncGpuBuffer(
    b: CpuGpuBuffer,
    forceRealloc: boolean,
    subOffset?: number,
    subPayload?: Uint8Array
  ) {
    const gl = this.gl!;
    gl.bindBuffer(gl.ARRAY_BUFFER, b.gl);

    if (forceRealloc) {
      // Reallocate + upload whole CPU buffer (simple, safe for v1)
      gl.bufferData(gl.ARRAY_BUFFER, b.cpu, gl.DYNAMIC_DRAW);
      b.gpuByteLength = b.cpu.byteLength;
      return;
    }

    if (subOffset != null && subPayload) {
      gl.bufferSubData(gl.ARRAY_BUFFER, subOffset, subPayload);
    } else {
      // If caller didn’t provide sub-range, just upload whole thing
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, b.cpu);
    }
  }

  // ============================================================
  // Render loop + HUD
  // ============================================================

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

    this.stats.drawCalls = 0;
    this.stats.uploadedBytesThisFrame = this.pendingUploadBytes;
    this.pendingUploadBytes = 0;

    gl.clear(gl.COLOR_BUFFER_BIT);

    if (this.prog && this.aPosLoc >= 0) {
      gl.useProgram(this.prog);

      // Draw all drawItems (each references a geometry, which references a buffer)
      for (const di of this.drawItems.values()) {
        const g = this.geometries.get(di.geometryId);
        if (!g) continue;

        const b = this.buffers.get(g.vertexBufferId);
        if (!b) continue;

        if (g.vertexCount < 3) continue;

        gl.bindBuffer(gl.ARRAY_BUFFER, b.gl);
        gl.enableVertexAttribArray(this.aPosLoc);
        gl.vertexAttribPointer(this.aPosLoc, 2, gl.FLOAT, false, 0, 0);

        gl.drawArrays(gl.TRIANGLES, 0, g.vertexCount);
        this.stats.drawCalls++;
      }
    }

    const t1 = performance.now();
    this.stats.frameMs = t1 - t0;

    this.hud?.setStats?.(this.getStats());
  }

  getStats(): EngineStats {
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

  private createProgram(vsSrc: string, fsSrc: string): WebGLProgram {
    const gl = this.gl!;
    const vs = this.compileShader(gl.VERTEX_SHADER, vsSrc);
    const fs = this.compileShader(gl.FRAGMENT_SHADER, fsSrc);

    const prog = gl.createProgram();
    if (!prog) throw new Error("Failed to create program");

    gl.attachShader(prog, vs);
    gl.attachShader(prog, fs);
    gl.linkProgram(prog);

    gl.deleteShader(vs);
    gl.deleteShader(fs);

    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) {
      const log = gl.getProgramInfoLog(prog) || "(no log)";
      gl.deleteProgram(prog);
      throw new Error("Program link failed: " + log);
    }
    return prog;
  }

  private compileShader(type: number, src: string): WebGLShader {
    const gl = this.gl!;
    const sh = gl.createShader(type);
    if (!sh) throw new Error("Failed to create shader");

    gl.shaderSource(sh, src);
    gl.compileShader(sh);

    if (!gl.getShaderParameter(sh, gl.COMPILE_STATUS)) {
      const log = gl.getShaderInfoLog(sh) || "(no log)";
      gl.deleteShader(sh);
      throw new Error("Shader compile failed: " + log);
    }
    return sh;
  }
}

function toU32(v: any): number {
  const n = Number(v);
  if (!Number.isFinite(n) || n <= 0) return 0;
  return n >>> 0;
}
