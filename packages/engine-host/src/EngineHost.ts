/* packages/engine-host/src/EngineHost.ts */

import { PIPELINES, PipelineId, PipelineSpec } from "./pipelines";

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

// -------------------- Data plane record format --------------------
const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;

// -------------------- Scene types --------------------
type VertexGeometry = {
  kind: "vertex";
  id: number;
  vertexBufferId: number;
  strideBytes: number; // usually 8 for pos2
  format: "pos2_clip";
};

type InstancedGeometry = {
  kind: "instanced";
  id: number;
  instanceBufferId: number;
  instanceStrideBytes: number;
  instanceFormat: "rect4" | "candle6";
};

type Geometry = VertexGeometry | InstancedGeometry;

type DrawItem = {
  id: number;
  geometryId: number;
  pipeline: PipelineId;
  transformId: number | null; // D1.5
};

// -------------------- D1.5 Transform resources --------------------
export type TransformParams = {
  // clip-space affine transform
  tx: number; // translate x (clip)
  ty: number; // translate y (clip)
  sx: number; // scale x
  sy: number; // scale y
};

type TransformResource = {
  id: number;
  params: TransformParams;
  mat3: Float32Array; // column-major mat3 for GLSL
};

// -------------------- Errors --------------------
type EngineErrorCode =
  | "UNKNOWN_PIPELINE"
  | "VALIDATION_NO_GEOMETRY"
  | "VALIDATION_BAD_GEOMETRY_KIND"
  | "VALIDATION_NO_BUFFER"
  | "VALIDATION_BAD_STRIDE"
  | "VALIDATION_BAD_FORMAT"
  | "VALIDATION_NO_TRANSFORM";

type EngineError = { code: EngineErrorCode; message: string; details?: any };
function err(code: EngineErrorCode, message: string, details?: any): EngineError {
  return { code, message, details };
}

// -------------------- Core ingest stub --------------------
type IngestResult = { touchedBufferIds: number[]; payloadBytes: number; droppedBytes: number };
type CpuBuffer = { id: number; cpu: Uint8Array };

class CoreIngestStub {
  readonly MAX_BUFFER_BYTES = 4 * 1024 * 1024;
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

  ingestData(batch: ArrayBuffer): IngestResult {
    const dv = new DataView(batch);
    let p = 0;

    const touched = new Set<number>();
    let payloadBytes = 0;
    let droppedBytes = 0;

    while (p < dv.byteLength) {
      if (p + 1 + 4 + 4 + 4 > dv.byteLength) throw new Error("CoreIngestStub: truncated header");

      const op = dv.getUint8(p); p += 1;
      const bufferId = dv.getUint32(p, true); p += 4;
      const offsetBytes = dv.getUint32(p, true); p += 4;
      const len = dv.getUint32(p, true); p += 4;

      if (p + len > dv.byteLength) throw new Error("CoreIngestStub: truncated payload");

      const payload = new Uint8Array(batch, p, len);
      p += len;
      payloadBytes += len;

      this.ensureBuffer(bufferId);
      const b = this.buffers.get(bufferId)!;

      if (op === OP_APPEND) {
        if (b.cpu.byteLength >= this.MAX_BUFFER_BYTES) { droppedBytes += len; continue; }
        const allowed = Math.min(len, this.MAX_BUFFER_BYTES - b.cpu.byteLength);
        if (allowed <= 0) { droppedBytes += len; continue; }
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
        if (offsetBytes >= this.MAX_BUFFER_BYTES) { droppedBytes += len; continue; }
        const end = offsetBytes + len;
        const allowedEnd = Math.min(end, this.MAX_BUFFER_BYTES);
        const allowedLen = allowedEnd - offsetBytes;
        if (allowedLen <= 0) { droppedBytes += len; continue; }
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

// -------------------- GPU buffers --------------------
type GpuBuffer = { gl: WebGLBuffer; gpuByteLength: number };

// -------------------- Programs --------------------
type ProgramBundle = {
  prog: WebGLProgram;
  a_pos?: number;
  a_rect?: number;
  a_c0?: number;
  a_c1?: number;
  u_transform?: WebGLUniformLocation | null;
  u_pointSize?: WebGLUniformLocation | null;
};

// -------------------- EngineHost --------------------
export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  private frames = 0;
  private lastFpsT = 0;

  private dataQueue: ArrayBuffer[] = [];
  private droppedBatches = 0;

  private readonly MAX_QUEUE = 512;
  private readonly MAX_BATCHES_PER_FRAME = 64;

  private core = new CoreIngestStub();
  private gpuBuffers = new Map<number, GpuBuffer>();

  private geometries = new Map<number, Geometry>();
  private drawItems = new Map<number, DrawItem>();

  // D1.5 transforms
  private transforms = new Map<number, TransformResource>();
  private readonly IDENTITY_MAT3 = new Float32Array([1,0,0, 0,1,0, 0,0,1]);

  private lastErrors: EngineError[] = [];

  private progPos2: ProgramBundle | null = null;
  private progInstRect: ProgramBundle | null = null;
  private progInstCandle: ProgramBundle | null = null;

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

    // ---- pos2 program (triSolid/line/points) with u_transform ----
    this.progPos2 = this.createProgramBundle(
      `#version 300 es
       precision highp float;
       in vec2 a_pos;
       uniform mat3 u_transform;
       uniform float u_pointSize;
       void main() {
         vec3 p = u_transform * vec3(a_pos, 1.0);
         gl_Position = vec4(p.xy, 0.0, 1.0);
         gl_PointSize = u_pointSize;
       }`,
      `#version 300 es
       precision highp float;
       out vec4 outColor;
       void main() { outColor = vec4(0.85, 0.15, 0.20, 1.0); }`,
      ["a_pos"],
      ["u_transform", "u_pointSize"]
    );

    // ---- instanced rect with u_transform ----
    this.progInstRect = this.createProgramBundle(
      `#version 300 es
       precision highp float;
       in vec4 a_rect; // x0,y0,x1,y1
       uniform mat3 u_transform;

       void main() {
         int vid = gl_VertexID % 6;
         vec2 uv;
         if (vid == 0) uv = vec2(0.0, 0.0);
         else if (vid == 1) uv = vec2(1.0, 0.0);
         else if (vid == 2) uv = vec2(0.0, 1.0);
         else if (vid == 3) uv = vec2(0.0, 1.0);
         else if (vid == 4) uv = vec2(1.0, 0.0);
         else uv = vec2(1.0, 1.0);

         float x = mix(a_rect.x, a_rect.z, uv.x);
         float y = mix(a_rect.y, a_rect.w, uv.y);

         vec3 p = u_transform * vec3(x, y, 1.0);
         gl_Position = vec4(p.xy, 0.0, 1.0);
       }`,
      `#version 300 es
       precision highp float;
       out vec4 outColor;
       void main() { outColor = vec4(0.15, 0.75, 0.25, 1.0); }`,
      ["a_rect"],
      ["u_transform"]
    );

    // ---- instanced candle with u_transform ----
    this.progInstCandle = this.createProgramBundle(
      `#version 300 es
       precision highp float;

       in vec4 a_c0; // x, open, high, low
       in vec2 a_c1; // close, halfWidth
       uniform mat3 u_transform;

       void main() {
         float x = a_c0.x;
         float open = a_c0.y;
         float high = a_c0.z;
         float low  = a_c0.w;
         float close = a_c1.x;
         float hw = a_c1.y;

         float body0 = min(open, close);
         float body1 = max(open, close);

         float wickW = hw * 0.25;

         int vid = gl_VertexID % 12;
         bool isWick = (vid >= 6);
         int lid = isWick ? (vid - 6) : vid;

         vec2 uv;
         if (lid == 0) uv = vec2(0.0, 0.0);
         else if (lid == 1) uv = vec2(1.0, 0.0);
         else if (lid == 2) uv = vec2(0.0, 1.0);
         else if (lid == 3) uv = vec2(0.0, 1.0);
         else if (lid == 4) uv = vec2(1.0, 0.0);
         else uv = vec2(1.0, 1.0);

         float x0 = isWick ? (x - wickW) : (x - hw);
         float x1 = isWick ? (x + wickW) : (x + hw);

         float y0 = isWick ? low : body0;
         float y1 = isWick ? high : body1;

         float vx = mix(x0, x1, uv.x);
         float vy = mix(y0, y1, uv.y);

         vec3 p = u_transform * vec3(vx, vy, 1.0);
         gl_Position = vec4(p.xy, 0.0, 1.0);
       }`,
      `#version 300 es
       precision highp float;
       out vec4 outColor;
       void main() { outColor = vec4(0.20, 0.55, 0.95, 1.0); }`,
      ["a_c0", "a_c1"],
      ["u_transform"]
    );

    this.onResize();
    window.addEventListener("resize", this.onResize, { passive: true });

    // Ensure transform 0 exists (identity) as a safe default (optional)
    this.ensureTransform(0);
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
      if (this.progPos2) gl.deleteProgram(this.progPos2.prog);
      if (this.progInstRect) gl.deleteProgram(this.progInstRect.prog);
      if (this.progInstCandle) gl.deleteProgram(this.progInstCandle.prog);
    }

    this.gpuBuffers.clear();
    this.dataQueue = [];
    this.geometries.clear();
    this.drawItems.clear();
    this.transforms.clear();
    this.lastErrors = [];

    this.gl = null;
    this.canvas = null;
  }

  // -------------------- worker ingest --------------------
  enqueueData(batch: ArrayBuffer) {
    if (this.dataQueue.length >= this.MAX_QUEUE) {
      this.droppedBatches++;
      return;
    }
    this.dataQueue.push(batch);
  }

  applyDataBatch(batch: ArrayBuffer) {
    this.enqueueData(batch);
  }

  // -------------------- D1.5 transforms API --------------------
  createTransform(transformId: number) {
    this.ensureTransform(transformId);
  }

  setTransform(transformId: number, params: Partial<TransformParams>) {
    const tr = this.ensureTransform(transformId);
    tr.params = { ...tr.params, ...sanitizeTransformParams(params) };
    tr.mat3 = mat3FromParams(tr.params);
  }

  /**
   * attachTransform(targetId, transformId)
   * D1.5 spec says pane/layer/drawItem; for now we implement drawItem only.
   */
  attachTransform(targetId: number, transformId: number) {
    const di = this.drawItems.get(targetId);
    if (!di) throw new Error(`attachTransform: drawItem ${targetId} not found`);
    this.ensureTransform(transformId);
    di.transformId = transformId;
  }

  private ensureTransform(transformId: number): TransformResource {
    let tr = this.transforms.get(transformId);
    if (!tr) {
      const p: TransformParams = { tx: 0, ty: 0, sx: 1, sy: 1 };
      tr = { id: transformId, params: p, mat3: mat3FromParams(p) };
      this.transforms.set(transformId, tr);
    }
    return tr;
  }

  private resolveTransformMat(di: DrawItem): Float32Array {
    const id = di.transformId;
    if (id === null || id === undefined) return this.IDENTITY_MAT3;
    const tr = this.transforms.get(id);
    if (!tr) {
      // Keep drawing with identity but record a structured error for visibility.
      this.lastErrors.push(err("VALIDATION_NO_TRANSFORM", "DrawItem references missing transform", { drawItemId: di.id, transformId: id }));
      return this.IDENTITY_MAT3;
    }
    return tr.mat3;
  }

  // -------------------- control plane --------------------
  /**
   * Supported cmds:
   * - createBuffer {id}
   * - createGeometry {id, vertexBufferId, format:"pos2_clip", strideBytes?}
   * - createInstancedGeometry {id, instanceBufferId, instanceFormat:"rect4"|"candle6", instanceStrideBytes}
   * - createDrawItem {id, geometryId, pipeline}
   * - setDrawItemPipeline {id, pipeline}
   * - createTransform {id}
   * - setTransform {id, tx?, ty?, sx?, sy?}
   * - attachTransform {targetId, transformId}
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
        if (!id && id !== 0) throw new Error("createBuffer: missing id");
        this.createBuffer(id);
        return { ok: true };
      }

      if (cmd === "createGeometry") {
        const id = toU32(obj.id);
        const vb = toU32(obj.vertexBufferId);
        const fmt = obj.format ?? "pos2_clip";
        const strideBytes = typeof obj.strideBytes === "number" ? (obj.strideBytes | 0) : 8;

        if (!id && id !== 0) throw new Error("createGeometry: missing id");
        if (!vb && vb !== 0) throw new Error("createGeometry: missing vertexBufferId");
        if (fmt !== "pos2_clip") throw new Error("createGeometry: only format=pos2_clip supported");

        this.createVertexGeometry(id, vb, strideBytes);
        return { ok: true };
      }

      if (cmd === "createInstancedGeometry") {
        const id = toU32(obj.id);
        const ib = toU32(obj.instanceBufferId);
        const instanceFormat = obj.instanceFormat as "rect4" | "candle6";
        const instanceStrideBytes = typeof obj.instanceStrideBytes === "number" ? (obj.instanceStrideBytes | 0) : 0;

        if (!id && id !== 0) throw new Error("createInstancedGeometry: missing id");
        if (!ib && ib !== 0) throw new Error("createInstancedGeometry: missing instanceBufferId");
        if (instanceFormat !== "rect4" && instanceFormat !== "candle6") throw new Error("createInstancedGeometry: instanceFormat must be rect4|candle6");
        if (instanceStrideBytes <= 0) throw new Error("createInstancedGeometry: missing/invalid instanceStrideBytes");

        this.createInstancedGeometry(id, ib, instanceFormat, instanceStrideBytes);
        return { ok: true };
      }

      if (cmd === "createDrawItem") {
        const id = toU32(obj.id);
        const gid = toU32(obj.geometryId);
        const pipeline = toPipelineId(obj.pipeline ?? "triSolid@1");
        if (!id && id !== 0) throw new Error("createDrawItem: missing id");
        if (!gid && gid !== 0) throw new Error("createDrawItem: missing geometryId");
        this.createDrawItem(id, gid, pipeline);
        return { ok: true };
      }

      if (cmd === "delete") {
        const kind = String(obj.kind ?? "");
        const id = toU32(obj.id);
        if (obj.id === undefined) throw new Error("delete: missing id");
        if (!kind) throw new Error("delete: missing kind");

        if (kind === "drawItem") this.deleteDrawItem(id);
        else if (kind === "geometry") this.deleteGeometry(id);
        else if (kind === "transform") this.deleteTransform(id);
        else if (kind === "buffer") this.deleteBuffer(id);
        else throw new Error(`delete: unknown kind '${kind}'`);

        return { ok: true };
      }


      if (cmd === "setDrawItemPipeline") {
        const id = toU32(obj.id);
        const pipeline = toPipelineId(obj.pipeline);
        if (!id && id !== 0) throw new Error("setDrawItemPipeline: missing id");
        this.setDrawItemPipeline(id, pipeline);
        return { ok: true };
      }

      // ---- D1.5 transforms ----
      if (cmd === "createTransform") {
        const id = toU32(obj.id);
        if (id === undefined || id === null) throw new Error("createTransform: missing id");
        this.createTransform(id);
        return { ok: true };
      }

      if (cmd === "setTransform") {
        const id = toU32(obj.id);
        if (id === undefined || id === null) throw new Error("setTransform: missing id");
        this.setTransform(id, { tx: obj.tx, ty: obj.ty, sx: obj.sx, sy: obj.sy });
        return { ok: true };
      }

      if (cmd === "attachTransform") {
        const targetId = toU32(obj.targetId);
        const transformId = toU32(obj.transformId);
        if (targetId === undefined || targetId === null) throw new Error("attachTransform: missing targetId");
        if (transformId === undefined || transformId === null) throw new Error("attachTransform: missing transformId");
        this.attachTransform(targetId, transformId);
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

  createBuffer(bufferId: number) {
    this.core.ensureBuffer(bufferId);
    this.stats.activeBuffers = this.core.getActiveBufferCount();
  }

  private createVertexGeometry(id: number, vertexBufferId: number, strideBytes: number) {
    this.createBuffer(vertexBufferId);
    this.geometries.set(id, { kind: "vertex", id, vertexBufferId, strideBytes, format: "pos2_clip" });
  }

  private createInstancedGeometry(id: number, instanceBufferId: number, instanceFormat: "rect4" | "candle6", instanceStrideBytes: number) {
    this.createBuffer(instanceBufferId);
    this.geometries.set(id, { kind: "instanced", id, instanceBufferId, instanceFormat, instanceStrideBytes });
  }

  private createDrawItem(id: number, geometryId: number, pipeline: PipelineId) {
    if (!this.geometries.has(geometryId)) throw new Error("createDrawItem: geometry not found");
    if (!PIPELINES[pipeline]) throw new Error(`createDrawItem: unknown pipeline '${pipeline}'`);
    this.drawItems.set(id, { id, geometryId, pipeline, transformId: null });
  }

  deleteDrawItem(id: number) {
    this.drawItems.delete(id);
  }

  deleteGeometry(id: number) {
    this.geometries.delete(id);
  }

  deleteTransform(id: number) {
    this.transforms.delete(id);
  }

  deleteBuffer(id: number) {
    // core
    this.core.deleteBuffer(id);

    // gpu
    const gl = this.gl;
    const gb = this.gpuBuffers.get(id);
    if (gl && gb) gl.deleteBuffer(gb.gl);
    this.gpuBuffers.delete(id);

    this.stats.activeBuffers = this.core.getActiveBufferCount();
  }


  private setDrawItemPipeline(drawItemId: number, pipeline: PipelineId) {
    const di = this.drawItems.get(drawItemId);
    if (!di) throw new Error("setDrawItemPipeline: drawItem not found");
    if (!PIPELINES[pipeline]) throw new Error(`setDrawItemPipeline: unknown pipeline '${pipeline}'`);
    di.pipeline = pipeline;
  }

  // -------------------- stats/debug --------------------
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

  getLastErrors(): EngineError[] {
    return [...this.lastErrors];
  }

  setDebugToggles(toggles: Partial<EngineStats["debug"]>) {
    this.stats.debug = { ...this.stats.debug, ...toggles };
  }

  // -------------------- picking (triangles only; uses transformed positions) --------------------
  pick(x: number, y: number): PickResult {
    const canvas = this.canvas;
    if (!canvas) return null;

    const rect = canvas.getBoundingClientRect();
    const w = rect.width || 1;
    const h = rect.height || 1;

    const ndcX = (x / w) * 2 - 1;
    const ndcY = 1 - (y / h) * 2;

    const drawIds = [...this.drawItems.keys()];
    for (let i = drawIds.length - 1; i >= 0; i--) {
      const drawItemId = drawIds[i];
      const di = this.drawItems.get(drawItemId);
      if (!di) continue;
      if (di.pipeline !== "triSolid@1") continue;

      const g = this.geometries.get(di.geometryId);
      if (!g || g.kind !== "vertex") continue;

      const cpu = this.core.getBufferBytes(g.vertexBufferId);
      const f32 = asF32(cpu);
      const vcount = Math.floor(cpu.byteLength / g.strideBytes);
      const triCountVerts = vcount - (vcount % 3);
      if (triCountVerts < 3) continue;

      const M = this.resolveTransformMat(di);

      for (let vi = 0; vi < triCountVerts; vi += 3) {
        const p0 = applyMat3ToPos2(M, f32[(vi + 0) * 2 + 0], f32[(vi + 0) * 2 + 1]);
        const p1 = applyMat3ToPos2(M, f32[(vi + 1) * 2 + 0], f32[(vi + 1) * 2 + 1]);
        const p2 = applyMat3ToPos2(M, f32[(vi + 2) * 2 + 0], f32[(vi + 2) * 2 + 1]);

        if (pointInTri(ndcX, ndcY, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y)) return { drawItemId };
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

    this.stats.drawCalls = 0;
    this.stats.ingestedBytesThisFrame = 0;
    this.stats.uploadedBytesThisFrame = 0;
    this.stats.droppedBytesThisFrame = 0;
    this.lastErrors = [];

    // Drain + ingest
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

    // Upload touched buffers
    for (const bufferId of touched) this.syncGpuBufferFull(bufferId);

    gl.clear(gl.COLOR_BUFFER_BIT);

    // Draw items (validate per-item; errors => no draw)
    for (const di of this.drawItems.values()) {
      const spec = PIPELINES[di.pipeline];
      if (!spec) {
        this.lastErrors.push(err("UNKNOWN_PIPELINE", `Unknown pipeline '${di.pipeline}'`, { drawItemId: di.id }));
        continue;
      }

      const g = this.geometries.get(di.geometryId);
      if (!g) {
        this.lastErrors.push(err("VALIDATION_NO_GEOMETRY", "DrawItem has no geometry", { drawItemId: di.id }));
        continue;
      }

      const errors = this.validate(spec, g, di);
      if (errors.length) {
        this.lastErrors.push(...errors);
        continue;
      }

      if (di.pipeline === "triSolid@1") this.drawTriSolid(di, g as VertexGeometry);
      else if (di.pipeline === "line2d@1") this.drawLine2d(di, g as VertexGeometry);
      else if (di.pipeline === "points@1") this.drawPoints(di, g as VertexGeometry);
      else if (di.pipeline === "instancedRect@1") this.drawInstancedRect(di, g as InstancedGeometry);
      else if (di.pipeline === "instancedCandle@1") this.drawInstancedCandle(di, g as InstancedGeometry);
    }

    const frameMs = performance.now() - t0;
    this.stats.frameMs = frameMs;
    this.frameWindow.push(frameMs);
    if (this.frameWindow.length > this.FRAME_WINDOW_MAX) this.frameWindow.shift();
    this.stats.frameMsP95 = percentile(this.frameWindow, 0.95);

    this.hud?.setStats?.(this.getStats());
  }

  // -------------------- validation --------------------
  private validate(spec: PipelineSpec, g: Geometry, di: DrawItem): EngineError[] {
    const errors: EngineError[] = [];

    // if drawItem references a transform, it must exist
    if (di.transformId !== null && !this.transforms.has(di.transformId)) {
      errors.push(err("VALIDATION_NO_TRANSFORM", "Attached transform not found", { drawItemId: di.id, transformId: di.transformId }));
    }

    if (spec.draw === "triangles" || spec.draw === "lines" || spec.draw === "points") {
      if (g.kind !== "vertex") {
        errors.push(err("VALIDATION_BAD_GEOMETRY_KIND", `${spec.id} requires vertex geometry`, { drawItemId: di.id, geometryId: g.id }));
        return errors;
      }

      const expected = spec.attributes["a_pos"]?.strideBytes ?? 8;
      if (g.strideBytes !== expected) {
        errors.push(err("VALIDATION_BAD_STRIDE", `${spec.id} requires a_pos strideBytes=${expected}`, { got: g.strideBytes }));
      }

      const gb = this.gpuBuffers.get(g.vertexBufferId);
      if (!gb && this.core.getBufferBytes(g.vertexBufferId).byteLength > 0) {
        errors.push(err("VALIDATION_NO_BUFFER", "Vertex buffer not uploaded on GPU", { vertexBufferId: g.vertexBufferId }));
      }

      return errors;
    }

    if (spec.draw === "instancedTriangles") {
      if (g.kind !== "instanced") {
        errors.push(err("VALIDATION_BAD_GEOMETRY_KIND", `${spec.id} requires instanced geometry`, { drawItemId: di.id, geometryId: g.id }));
        return errors;
      }

      if (spec.id === "instancedRect@1") {
        if (g.instanceFormat !== "rect4") errors.push(err("VALIDATION_BAD_FORMAT", "instancedRect@1 requires instanceFormat=rect4", { got: g.instanceFormat }));
        if (g.instanceStrideBytes !== 16) errors.push(err("VALIDATION_BAD_STRIDE", "instancedRect@1 requires instanceStrideBytes=16", { got: g.instanceStrideBytes }));
      }

      if (spec.id === "instancedCandle@1") {
        if (g.instanceFormat !== "candle6") errors.push(err("VALIDATION_BAD_FORMAT", "instancedCandle@1 requires instanceFormat=candle6", { got: g.instanceFormat }));
        if (g.instanceStrideBytes !== 24) errors.push(err("VALIDATION_BAD_STRIDE", "instancedCandle@1 requires instanceStrideBytes=24", { got: g.instanceStrideBytes }));
      }

      const gb = this.gpuBuffers.get(g.instanceBufferId);
      if (!gb && this.core.getBufferBytes(g.instanceBufferId).byteLength > 0) {
        errors.push(err("VALIDATION_NO_BUFFER", "Instance buffer not uploaded on GPU", { instanceBufferId: g.instanceBufferId }));
      }

      return errors;
    }

    return errors;
  }

  // -------------------- draw implementations (now set u_transform) --------------------
  private drawTriSolid(di: DrawItem, g: VertexGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.vertexBufferId);
    const vcount = Math.floor(cpu.byteLength / g.strideBytes);
    const triCount = vcount - (vcount % 3);
    if (triCount < 3) return;

    const gb = this.gpuBuffers.get(g.vertexBufferId);
    if (!gb) return;

    const p = this.progPos2!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));
    gl.uniform1f(p.u_pointSize!, 1.0);

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.enableVertexAttribArray(p.a_pos!);
    gl.vertexAttribPointer(p.a_pos!, 2, gl.FLOAT, false, g.strideBytes, 0);
    gl.vertexAttribDivisor(p.a_pos!, 0);

    gl.drawArrays(gl.TRIANGLES, 0, triCount);
    this.stats.drawCalls++;
  }

  private drawLine2d(di: DrawItem, g: VertexGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.vertexBufferId);
    const vcount = Math.floor(cpu.byteLength / g.strideBytes);
    const lineCount = vcount - (vcount % 2);
    if (lineCount < 2) return;

    const gb = this.gpuBuffers.get(g.vertexBufferId);
    if (!gb) return;

    const p = this.progPos2!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));
    gl.uniform1f(p.u_pointSize!, 1.0);

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.enableVertexAttribArray(p.a_pos!);
    gl.vertexAttribPointer(p.a_pos!, 2, gl.FLOAT, false, g.strideBytes, 0);
    gl.vertexAttribDivisor(p.a_pos!, 0);

    gl.drawArrays(gl.LINES, 0, lineCount);
    this.stats.drawCalls++;
  }

  private drawPoints(di: DrawItem, g: VertexGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.vertexBufferId);
    const vcount = Math.floor(cpu.byteLength / g.strideBytes);
    if (vcount < 1) return;

    const gb = this.gpuBuffers.get(g.vertexBufferId);
    if (!gb) return;

    const p = this.progPos2!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));
    gl.uniform1f(p.u_pointSize!, 6.0);

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.enableVertexAttribArray(p.a_pos!);
    gl.vertexAttribPointer(p.a_pos!, 2, gl.FLOAT, false, g.strideBytes, 0);
    gl.vertexAttribDivisor(p.a_pos!, 0);

    gl.drawArrays(gl.POINTS, 0, vcount);
    this.stats.drawCalls++;
  }

  private drawInstancedRect(di: DrawItem, g: InstancedGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.instanceBufferId);
    const icount = Math.floor(cpu.byteLength / g.instanceStrideBytes);
    if (icount < 1) return;

    const gb = this.gpuBuffers.get(g.instanceBufferId);
    if (!gb) return;

    const p = this.progInstRect!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.enableVertexAttribArray(p.a_rect!);
    gl.vertexAttribPointer(p.a_rect!, 4, gl.FLOAT, false, g.instanceStrideBytes, 0);
    gl.vertexAttribDivisor(p.a_rect!, 1);

    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, icount);
    this.stats.drawCalls++;
  }

  private drawInstancedCandle(di: DrawItem, g: InstancedGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.instanceBufferId);
    const icount = Math.floor(cpu.byteLength / g.instanceStrideBytes);
    if (icount < 1) return;

    const gb = this.gpuBuffers.get(g.instanceBufferId);
    if (!gb) return;

    const p = this.progInstCandle!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);

    gl.enableVertexAttribArray(p.a_c0!);
    gl.vertexAttribPointer(p.a_c0!, 4, gl.FLOAT, false, g.instanceStrideBytes, 0);
    gl.vertexAttribDivisor(p.a_c0!, 1);

    gl.enableVertexAttribArray(p.a_c1!);
    gl.vertexAttribPointer(p.a_c1!, 2, gl.FLOAT, false, g.instanceStrideBytes, 16);
    gl.vertexAttribDivisor(p.a_c1!, 1);

    gl.drawArraysInstanced(gl.TRIANGLES, 0, 12, icount);
    this.stats.drawCalls++;
  }

  // -------------------- GPU upload --------------------
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

      if (this.lastErrors.length) console.warn("[EngineHost] validation errors:", this.lastErrors);

      this.frames = 0;
      this.lastFpsT = t;
    }
  }

  // -------------------- WebGL program helper --------------------
  private createProgramBundle(vsSrc: string, fsSrc: string, attribs: string[], uniforms: string[]): ProgramBundle {
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

    const out: ProgramBundle = { prog };

    for (const a of attribs) {
      const loc = gl.getAttribLocation(prog, a);
      if (a === "a_pos") out.a_pos = loc;
      if (a === "a_rect") out.a_rect = loc;
      if (a === "a_c0") out.a_c0 = loc;
      if (a === "a_c1") out.a_c1 = loc;
    }

    for (const u of uniforms) {
      const loc = gl.getUniformLocation(prog, u);
      if (u === "u_transform") out.u_transform = loc;
      if (u === "u_pointSize") out.u_pointSize = loc;
    }

    // sanity: ensure required uniforms exist (if shader declares them)
    return out;
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

// -------------------- utils --------------------
function toU32(v: any): number {
  const n = Number(v);
  if (!Number.isFinite(n) || n < 0) return 0;
  return n >>> 0;
}

function toPipelineId(v: any): PipelineId {
  if (typeof v !== "string") throw new Error("pipeline must be a string");
  const p = v as PipelineId;
  if (!(p in PIPELINES)) throw new Error(`Unknown pipeline '${v}'`);
  return p;
}

function percentile(values: number[], p: number): number {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  const idx = Math.min(sorted.length - 1, Math.max(0, Math.floor(p * (sorted.length - 1))));
  return sorted[idx];
}

function asF32(u8: Uint8Array): Float32Array {
  const n = (u8.byteLength / 4) | 0;
  return new Float32Array(u8.buffer, u8.byteOffset, n);
}

// ---- mat3 helpers (column-major for WebGL uniformMatrix3fv) ----
function sanitizeTransformParams(p: Partial<TransformParams>): Partial<TransformParams> {
  const out: Partial<TransformParams> = {};
  if (Number.isFinite(p.tx as any)) out.tx = Number(p.tx);
  if (Number.isFinite(p.ty as any)) out.ty = Number(p.ty);
  if (Number.isFinite(p.sx as any)) out.sx = Number(p.sx);
  if (Number.isFinite(p.sy as any)) out.sy = Number(p.sy);
  return out;
}

function mat3FromParams(p: TransformParams): Float32Array {
  // Affine 2D:
  // [ sx  0  tx ]
  // [  0 sy  ty ]
  // [  0  0   1 ]
  // Column-major array:
  return new Float32Array([
    p.sx, 0,    0,
    0,    p.sy, 0,
    p.tx, p.ty, 1
  ]);
}

function applyMat3ToPos2(M: Float32Array, x: number, y: number): { x: number; y: number } {
  // column-major:
  // x' = m0*x + m3*y + m6*1
  // y' = m1*x + m4*y + m7*1
  const x2 = M[0] * x + M[3] * y + M[6];
  const y2 = M[1] * x + M[4] * y + M[7];
  return { x: x2, y: y2 };
}

// -------------------- picking helper --------------------
function pointInTri(px: number, py: number, x0: number, y0: number, x1: number, y1: number, x2: number, y2: number): boolean {
  const b0 = sign(px, py, x0, y0, x1, y1) < 0;
  const b1 = sign(px, py, x1, y1, x2, y2) < 0;
  const b2 = sign(px, py, x2, y2, x0, y0) < 0;
  return b0 === b1 && b1 === b2;
}

function sign(px: number, py: number, ax: number, ay: number, bx: number, by: number): number {
  return (px - bx) * (ay - by) - (ax - bx) * (py - by);
}
