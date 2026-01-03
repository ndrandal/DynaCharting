/* packages/engine-host/src/EngineHost.ts
 *
 * EngineHost:
 * - Control plane: JSON-adjacent commands (createBuffer, createGeometry, createDrawItem, setDebug)
 * - Data plane: binary batches (bufferAppend, bufferUpdateRange)
 * - Worker ingest stub: enqueueData(ArrayBuffer) with Transferables; drain during renderOnce
 * - "Core ingest" is represented by an internal CoreIngestStub class (no server yet)
 * - Minimal renderer: draws pos2_clip vertices as TRIANGLES
 * - Picking v1: CPU point-in-triangle over drawItem geometry
 */

export type PickResult = { drawItemId: number } | null;

export type EngineStats = {
  frameMs: number;
  frameMsP95: number;

  drawCalls: number;

  // Semantic bytes coming in from data plane (payload bytes)
  ingestedBytesThisFrame: number;

  // Actual bytes uploaded to GPU this frame (bufferData/bufferSubData)
  uploadedBytesThisFrame: number;

  activeBuffers: number;

  queuedBatches: number;
  droppedBatches: number;
  droppedBytesThisFrame: number;

  debug: {
    showBounds: boolean;
    wireframe: boolean; // host may ignore if not available
  };
};

export type EngineHostHudSink = {
  setFps: (fps: number) => void;
  setGl: (label: string) => void;
  setMem: (label: string) => void;
  setStats?: (s: EngineStats) => void;
  setPick?: (id: number | null) => void;
};

// -------------------- Data plane record format --------------------
// u8  op (1 append, 2 updateRange)
// u32 bufferId
// u32 offsetBytes (for updateRange; 0 for append)
// u32 payloadBytes
// payload
const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

// -------------------- Minimal scene --------------------
type Geometry = {
  id: number;
  vertexBufferId: number;
  vertexCount: number; // derived from CPU buffer bytes / 8
  format: "pos2_clip";
};

type DrawItem = {
  id: number;
  geometryId: number;
  pipeline: "flat";
};

// -------------------- Core ingest stub --------------------
type IngestResult = {
  touchedBufferIds: number[];
  payloadBytes: number;
  droppedBytes: number;
  // optional: future range deltas
};

type CpuBuffer = {
  id: number;
  cpu: Uint8Array;
};

class CoreIngestStub {
  // Simple hard cap to avoid runaway memory (ring buffer later)
  readonly MAX_BUFFER_BYTES = 4 * 1024 * 1024; // 4 MiB per buffer

  private buffers = new Map<number, CpuBuffer>();

  ensureBuffer(bufferId: number) {
    if (!this.buffers.has(bufferId)) {
      this.buffers.set(bufferId, { id: bufferId, cpu: new Uint8Array(0) });
    }
  }

  getActiveBufferCount(): number {
    return this.buffers.size;
  }

  getBufferBytes(bufferId: number): Uint8Array {
    const b = this.buffers.get(bufferId);
    return b ? b.cpu : new Uint8Array(0);
  }

  // Parse + apply a binary batch (same record format as D3.1)
  ingestData(batch: ArrayBuffer): IngestResult {
    const dv = new DataView(batch);
    let p = 0;

    const touched = new Set<number>();
    let payloadBytes = 0;
    let droppedBytes = 0;

    while (p < dv.byteLength) {
      if (p + 1 + 4 + 4 + 4 > dv.byteLength) {
        throw new Error("CoreIngestStub: truncated header");
      }

      const op = dv.getUint8(p); p += 1;
      const bufferId = dv.getUint32(p, true); p += 4;
      const offsetBytes = dv.getUint32(p, true); p += 4;
      const len = dv.getUint32(p, true); p += 4;

      if (p + len > dv.byteLength) {
        throw new Error("CoreIngestStub: truncated payload");
      }

      const payload = new Uint8Array(batch, p, len);
      p += len;

      payloadBytes += len;

      this.ensureBuffer(bufferId);
      const b = this.buffers.get(bufferId)!;

      if (op === OP_APPEND) {
        if (b.cpu.byteLength >= this.MAX_BUFFER_BYTES) {
          droppedBytes += len;
          continue;
        }

        const allowed = Math.min(len, this.MAX_BUFFER_BYTES - b.cpu.byteLength);
        if (allowed <= 0) {
          droppedBytes += len;
          continue;
        }

        const oldLen = b.cpu.byteLength;
        const next = new Uint8Array(oldLen + allowed);
        next.set(b.cpu, 0);
        next.set(payload.subarray(0, allowed), oldLen);
        b.cpu = next;

        if (allowed < len) droppedBytes += (len - allowed);

        touched.add(bufferId);
        continue;
      }

      if (op === OP_UPDATE_RANGE) {
        // clamp to cap
        if (offsetBytes >= this.MAX_BUFFER_BYTES) {
          droppedBytes += len;
          continue;
        }

        const end = offsetBytes + len;
        const allowedEnd = Math.min(end, this.MAX_BUFFER_BYTES);
        const allowedLen = allowedEnd - offsetBytes;

        if (allowedLen <= 0) {
          droppedBytes += len;
          continue;
        }

        // grow CPU buffer if needed
        if (allowedEnd > b.cpu.byteLength) {
          const grown = new Uint8Array(allowedEnd);
          grown.set(b.cpu, 0);
          b.cpu = grown;
        }

        b.cpu.set(payload.subarray(0, allowedLen), offsetBytes);

        if (allowedLen < len) droppedBytes += (len - allowedLen);

        touched.add(bufferId);
        continue;
      }

      throw new Error(`CoreIngestStub: unknown op ${op}`);
    }

    return { touchedBufferIds: [...touched], payloadBytes, droppedBytes };
  }
}

// -------------------- GPU buffer tracking --------------------
type GpuBuffer = {
  gl: WebGLBuffer;
  gpuByteLength: number;
};

// -------------------- EngineHost --------------------
export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  // FPS calc
  private frames = 0;
  private lastFpsT = 0;

  // Worker -> main thread queue (Transferables)
  private dataQueue: ArrayBuffer[] = [];
  private droppedBatches = 0;

  // Safety: cap queue length to prevent memory blowup if worker outruns render
  private readonly MAX_QUEUE = 512;

  // Drain budget so you don’t stall frames
  private readonly MAX_BATCHES_PER_FRAME = 64;

  // Core ingest stub
  private core = new CoreIngestStub();

  // GPU buffers mirror core buffers
  private gpuBuffers = new Map<number, GpuBuffer>();

  // Scene
  private geometries = new Map<number, Geometry>();
  private drawItems = new Map<number, DrawItem>();

  // Shader (pos2_clip)
  private prog: WebGLProgram | null = null;
  private aPosLoc = -1;

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
    debug: { showBounds: false, wireframe: false }
  };

  // Rolling window for p95
  private frameWindow: number[] = [];
  private readonly FRAME_WINDOW_MAX = 240;

  constructor(private hud?: EngineHostHudSink) {}

  // -------------------- lifecycle --------------------
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

    // Minimal shader program
    this.prog = this.createProgram(
      `#version 300 es
       precision highp float;
       in vec2 aPos;
       void main() { gl_Position = vec4(aPos, 0.0, 1.0); }`,
      `#version 300 es
       precision highp float;
       out vec4 outColor;
       void main() { outColor = vec4(0.85, 0.15, 0.20, 1.0); }`
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
      for (const gb of this.gpuBuffers.values()) gl.deleteBuffer(gb.gl);
      if (this.prog) gl.deleteProgram(this.prog);
    }

    this.gpuBuffers.clear();
    this.dataQueue = [];
    this.geometries.clear();
    this.drawItems.clear();

    this.gl = null;
    this.canvas = null;
  }

  // -------------------- D5.1: Worker ingest entrypoint --------------------
  // Called from worker.onmessage: do not parse here; just enqueue.
  enqueueData(batch: ArrayBuffer) {
    if (this.dataQueue.length >= this.MAX_QUEUE) {
      this.droppedBatches++;
      return;
    }
    this.dataQueue.push(batch);
  }

  // -------------------- D3.1: Data plane (direct, no worker) --------------------
  // Useful for tests/demos. This runs ingest immediately (main thread), so prefer enqueueData for hot loop simulation.
  applyDataBatch(batch: ArrayBuffer) {
    // For direct calls, we still route through core and upload on next frame:
    this.enqueueData(batch);
  }

  // -------------------- Control plane (JSON-adjacent) --------------------
  /**
   * Supported cmds:
   * - createBuffer {id}
   * - createGeometry {id, vertexBufferId, format:"pos2_clip"}
   * - createDrawItem {id, geometryId, pipeline:"flat"}
   * - setDebug {showBounds?, wireframe?}
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
        this.createBuffer(id);
        return { ok: true };
      }

      if (cmd === "createGeometry") {
        const id = toU32(obj.id);
        const vb = toU32(obj.vertexBufferId);
        const fmt = obj.format ?? "pos2_clip";
        if (!id || !vb) throw new Error("createGeometry: missing id or vertexBufferId");
        if (fmt !== "pos2_clip") throw new Error("createGeometry: only format=pos2_clip supported");
        this.createGeometry(id, vb);
        return { ok: true };
      }

      if (cmd === "createDrawItem") {
        const id = toU32(obj.id);
        const gid = toU32(obj.geometryId);
        const pipe = (obj.pipeline ?? "flat") as string;
        if (!id || !gid) throw new Error("createDrawItem: missing id or geometryId");
        if (pipe !== "flat") throw new Error("createDrawItem: only pipeline=flat supported for now");
        this.createDrawItem(id, gid);
        return { ok: true };
      }

      if (cmd === "setDebug") {
        const showBounds = obj.showBounds;
        const wireframe = obj.wireframe;
        this.setDebugToggles({
          showBounds: typeof showBounds === "boolean" ? showBounds : undefined,
          wireframe: typeof wireframe === "boolean" ? wireframe : undefined
        });
        return { ok: true };
      }

      return { ok: false, error: `Control: unknown cmd ${cmd}` };
    } catch (e: any) {
      return { ok: false, error: e?.message ?? String(e) };
    }
  }

  // Convenience helpers (also used by demo code)
  createBuffer(bufferId: number) {
    this.core.ensureBuffer(bufferId);
    this.stats.activeBuffers = this.core.getActiveBufferCount();
  }

  createGeometry(id: number, vertexBufferId: number) {
    this.createBuffer(vertexBufferId);
    this.geometries.set(id, {
      id,
      vertexBufferId,
      vertexCount: 0,
      format: "pos2_clip"
    });
  }

  createDrawItem(id: number, geometryId: number) {
    if (!this.geometries.has(geometryId)) throw new Error("createDrawItem: geometry not found");
    this.drawItems.set(id, { id, geometryId, pipeline: "flat" });
  }

  // -------------------- D10.1: stats/debug --------------------
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
      debug: { ...this.stats.debug }
    };
  }

  setDebugToggles(toggles: Partial<EngineStats["debug"]>) {
    this.stats.debug = { ...this.stats.debug, ...toggles };
  }

  // -------------------- D1.4: picking v1 --------------------
  // pick(x,y) expects canvas-relative CSS pixels.
  pick(x: number, y: number): PickResult {
    const canvas = this.canvas;
    if (!canvas) return null;

    const rect = canvas.getBoundingClientRect();
    const w = rect.width || 1;
    const h = rect.height || 1;

    const ndcX = (x / w) * 2 - 1;
    const ndcY = 1 - (y / h) * 2;

    // Back-to-front: later draw items win (in insertion order we don't have z, so just iterate reverse)
    const drawIds = [...this.drawItems.keys()];
    for (let i = drawIds.length - 1; i >= 0; i--) {
      const drawItemId = drawIds[i];
      const di = this.drawItems.get(drawItemId);
      if (!di) continue;

      const g = this.geometries.get(di.geometryId);
      if (!g) continue;

      const cpu = this.core.getBufferBytes(g.vertexBufferId);
      const f32 = asF32(cpu);
      const vcount = Math.floor(f32.length / 2);
      const triCountVerts = vcount - (vcount % 3);
      if (triCountVerts < 3) continue;

      // Test each triangle; v1 crude but correct
      for (let vi = 0; vi < triCountVerts; vi += 3) {
        const x0 = f32[(vi + 0) * 2 + 0], y0 = f32[(vi + 0) * 2 + 1];
        const x1 = f32[(vi + 1) * 2 + 0], y1 = f32[(vi + 1) * 2 + 1];
        const x2 = f32[(vi + 2) * 2 + 0], y2 = f32[(vi + 2) * 2 + 1];
        if (pointInTri(ndcX, ndcY, x0, y0, x1, y1, x2, y2)) {
          return { drawItemId };
        }
      }
    }

    return null;
  }

  // -------------------- loop --------------------
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

  private renderOnce() {
    const gl = this.gl;
    if (!gl) return;

    const t0 = performance.now();

    // reset per-frame counters
    this.stats.drawCalls = 0;
    this.stats.ingestedBytesThisFrame = 0;
    this.stats.uploadedBytesThisFrame = 0;
    this.stats.droppedBytesThisFrame = 0;

    // Drain queue within budget and ingest
    const touched = new Set<number>();
    let processed = 0;

    while (processed < this.MAX_BATCHES_PER_FRAME && this.dataQueue.length > 0) {
      const batch = this.dataQueue.shift()!;
      const r = this.core.ingestData(batch);
      this.stats.ingestedBytesThisFrame += r.payloadBytes;
      this.stats.droppedBytesThisFrame += r.droppedBytes;
      for (const id of r.touchedBufferIds) touched.add(id);
      processed++;
    }

    this.stats.queuedBatches = this.dataQueue.length;
    this.stats.droppedBatches = this.droppedBatches;
    this.stats.activeBuffers = this.core.getActiveBufferCount();

    // Upload touched buffers once per frame
    for (const bufferId of touched) {
      this.syncGpuBufferFull(bufferId);
    }

    // Clear + draw
    gl.clear(gl.COLOR_BUFFER_BIT);

    if (this.prog && this.aPosLoc >= 0) {
      gl.useProgram(this.prog);

      for (const di of this.drawItems.values()) {
        const g = this.geometries.get(di.geometryId);
        if (!g) continue;

        const cpu = this.core.getBufferBytes(g.vertexBufferId);
        // pos2_clip: 8 bytes per vertex (2 float32)
        const vcount = Math.floor(cpu.byteLength / 8);
        g.vertexCount = vcount;

        const triCountVerts = vcount - (vcount % 3);
        if (triCountVerts < 3) continue;

        const gb = this.gpuBuffers.get(g.vertexBufferId);
        if (!gb) continue;

        gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
        gl.enableVertexAttribArray(this.aPosLoc);
        gl.vertexAttribPointer(this.aPosLoc, 2, gl.FLOAT, false, 0, 0);

        gl.drawArrays(gl.TRIANGLES, 0, triCountVerts);
        this.stats.drawCalls++;
      }
    }

    // Frame timing + p95
    const t1 = performance.now();
    const frameMs = t1 - t0;
    this.stats.frameMs = frameMs;

    this.frameWindow.push(frameMs);
    if (this.frameWindow.length > this.FRAME_WINDOW_MAX) this.frameWindow.shift();
    this.stats.frameMsP95 = percentile(this.frameWindow, 0.95);

    // Update HUD stats every frame (cheap string updates happen in hud.ts)
    this.hud?.setStats?.(this.getStats());
  }

  // Full upload (simple & safe): bufferData whole CPU buffer.
  // Later you’ll optimize to range updates and preallocated GPU capacity.
  private syncGpuBufferFull(bufferId: number) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(bufferId);

    let gb = this.gpuBuffers.get(bufferId);
    if (!gb) {
      const glb = gl.createBuffer();
      if (!glb) throw new Error("Failed to create WebGLBuffer");
      gb = { gl: glb, gpuByteLength: 0 };
      this.gpuBuffers.set(bufferId, gb);
    }

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.bufferData(gl.ARRAY_BUFFER, cpu, gl.DYNAMIC_DRAW);

    gb.gpuByteLength = cpu.byteLength;
    this.stats.uploadedBytesThisFrame += cpu.byteLength;
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

  // -------------------- WebGL helpers --------------------
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

// -------------------- small utils --------------------
function toU32(v: any): number {
  const n = Number(v);
  if (!Number.isFinite(n) || n <= 0) return 0;
  return n >>> 0;
}

function percentile(values: number[], p: number): number {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const idx = Math.min(sorted.length - 1, Math.max(0, Math.floor(p * (sorted.length - 1))));
  return sorted[idx];
}

function asF32(u8: Uint8Array): Float32Array {
  // If byteLength isn't multiple of 4, floor it.
  const n = (u8.byteLength / 4) | 0;
  return new Float32Array(u8.buffer, u8.byteOffset, n);
}

// -------------------- picking helper --------------------
function pointInTri(
  px: number,
  py: number,
  x0: number, y0: number,
  x1: number, y1: number,
  x2: number, y2: number
): boolean {
  const b0 = sign(px, py, x0, y0, x1, y1) < 0;
  const b1 = sign(px, py, x1, y1, x2, y2) < 0;
  const b2 = sign(px, py, x2, y2, x0, y0) < 0;
  return (b0 === b1) && (b1 === b2);
}

function sign(px: number, py: number, ax: number, ay: number, bx: number, by: number): number {
  return (px - bx) * (ay - by) - (ax - bx) * (py - by);
}
