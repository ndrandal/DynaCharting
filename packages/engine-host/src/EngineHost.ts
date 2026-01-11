/* packages/engine-host/src/EngineHost.ts
 *
 * Full rewrite (D3 hardened):
 * - Supports D2.2 pipelines (triSolid@1, line2d@1, points@1, instancedRect@1, instancedCandle@1)
 * - Supports D1.5 transforms as first-class resources (create/set/attach)
 * - Supports D6.1 buffer cache policy (bufferSetMaxBytes, bufferEvictFront, bufferKeepLast)
 * - Supports D2.3 textSDF@1 pipeline + incremental glyph atlas uploads
 * - Keeps your data plane record format (append/updateRange) + worker batching model
 * - Picking v1 for triSolid@1 (CPU point-in-triangle + transform)
 *
 * D3 fixes:
 * - shutdown clears CPU ingest store (core.reset) as well as GPU resources
 * - cascade deletes to prevent dangling references:
 *   - deleteTransform detaches from drawItems
 *   - deleteGeometry removes dependent drawItems
 *   - deleteBuffer removes dependent geometries + drawItems before deleting the buffer
 */

import { PIPELINES, PipelineId, PipelineSpec } from "./pipelines";
import { CoreIngestStub } from "./CoreIngestStub";
import { GlyphAtlas } from "./GlyphAtlas";
import type { ControlCommand, ApplyResult } from "../../protocol/src/control";




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
  instanceFormat: "rect4" | "candle6" | "glyph8";
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
  tx: number;
  ty: number;
  sx: number;
  sy: number;
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
  | "VALIDATION_NO_TRANSFORM"
  | "TEXT_ATLAS_FULL"
  | "TEXT_NO_ATLAS";

type EngineError = { code: EngineErrorCode; message: string; details?: any };
function err(code: EngineErrorCode, message: string, details?: any): EngineError {
  return { code, message, details };
}

// -------------------- GPU buffers --------------------
type GpuBuffer = { gl: WebGLBuffer; gpuByteLength: number };

// -------------------- Programs --------------------
type ProgramBundle = {
  prog: WebGLProgram;
  // attribs
  a_pos?: number;
  a_rect?: number;
  a_c0?: number;
  a_c1?: number;
  a_g0?: number;
  a_g1?: number;

  // uniforms
  u_transform?: WebGLUniformLocation | null;
  u_pointSize?: WebGLUniformLocation | null;

  // text uniforms
  u_atlas?: WebGLUniformLocation | null;
  u_color?: WebGLUniformLocation | null;
  u_pxRange?: WebGLUniformLocation | null;
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

  private readonly MAX_QUEUE = 512;
  private readonly MAX_BATCHES_PER_FRAME = 64;

  // Core ingest stub (CPU side)
  private core = new CoreIngestStub();

  // GPU buffers mirror core buffers
  private gpuBuffers = new Map<number, GpuBuffer>();

  // Scene resources
  private geometries = new Map<number, Geometry>();
  private drawItems = new Map<number, DrawItem>();

  // D1.5 transforms
  private transforms = new Map<number, TransformResource>();
  private readonly IDENTITY_MAT3 = new Float32Array([1, 0, 0, 0, 1, 0, 0, 0, 1]);

  // Errors
  private lastErrors: EngineError[] = [];

  // Programs
  private progPos2: ProgramBundle | null = null;
  private progInstRect: ProgramBundle | null = null;
  private progInstCandle: ProgramBundle | null = null;
  private progTextSdf: ProgramBundle | null = null;

  // Text atlas
  private atlas = new GlyphAtlas({ atlasSize: 1024, glyphPx: 48, sdfRange: 12, pad: 2 });
  private atlasTex: WebGLTexture | null = null;

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
      powerPreference: "high-performance",
    });

    if (!gl) throw new Error("WebGL2 not available.");
    this.gl = gl;

    gl.disable(gl.DEPTH_TEST);
    gl.disable(gl.STENCIL_TEST);
    gl.disable(gl.CULL_FACE);
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

    // ---- instanced rect ----
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

    // ---- instanced candle ----
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

    // ---- textSDF@1 ----
    this.initAtlasTexture();

    this.progTextSdf = this.createProgramBundle(
      `#version 300 es
       precision highp float;

       in vec4 a_g0; // x0 y0 x1 y1
       in vec4 a_g1; // u0 v0 u1 v1

       uniform mat3 u_transform;
       out vec2 v_uv;

       void main() {
         int vid = gl_VertexID % 6;
         vec2 uv;
         if (vid == 0) uv = vec2(0.0, 0.0);
         else if (vid == 1) uv = vec2(1.0, 0.0);
         else if (vid == 2) uv = vec2(0.0, 1.0);
         else if (vid == 3) uv = vec2(0.0, 1.0);
         else if (vid == 4) uv = vec2(1.0, 0.0);
         else uv = vec2(1.0, 1.0);

         float x = mix(a_g0.x, a_g0.z, uv.x);
         float y = mix(a_g0.y, a_g0.w, uv.y);

         v_uv = vec2(mix(a_g1.x, a_g1.z, uv.x), mix(a_g1.y, a_g1.w, uv.y));

         vec3 p = u_transform * vec3(x, y, 1.0);
         gl_Position = vec4(p.xy, 0.0, 1.0);
       }`,
      `#version 300 es
       precision highp float;

       uniform sampler2D u_atlas;
       uniform vec4 u_color;
       uniform float u_pxRange;

       in vec2 v_uv;
       out vec4 outColor;

       void main() {
         float sdf = texture(u_atlas, v_uv).r;
         float dist = sdf - 0.5;
         float w = fwidth(dist) * u_pxRange;
         float a = smoothstep(-w, w, dist);
         outColor = vec4(u_color.rgb, u_color.a * a);
       }`,
      ["a_g0", "a_g1"],
      ["u_transform", "u_atlas", "u_color", "u_pxRange"]
    );

    this.onResize();
    window.addEventListener("resize", this.onResize, { passive: true });

    // Ensure transform 0 exists as a safe identity
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
    // Stop loop first
    this.running = false;
    cancelAnimationFrame(this.raf);
    this.raf = 0;

    window.removeEventListener("resize", this.onResize);

    const gl = this.gl;
    if (gl) {
      // GPU buffers
      for (const gb of this.gpuBuffers.values()) gl.deleteBuffer(gb.gl);

      // Programs
      if (this.progPos2) gl.deleteProgram(this.progPos2.prog);
      if (this.progInstRect) gl.deleteProgram(this.progInstRect.prog);
      if (this.progInstCandle) gl.deleteProgram(this.progInstCandle.prog);
      if (this.progTextSdf) gl.deleteProgram(this.progTextSdf.prog);

      // Text atlas
      if (this.atlasTex) gl.deleteTexture(this.atlasTex);
    }

    // Clear GPU-side maps
    this.gpuBuffers.clear();
    this.atlasTex = null;

    // Clear per-frame data
    this.dataQueue = [];
    this.droppedBatches = 0;

    // Clear scene resources
    this.drawItems.clear();
    this.geometries.clear();
    this.transforms.clear();
    this.lastErrors = [];

    // D3: release CPU-side ingest buffers
    // (prevents memory retention across remount/hot reload)
    this.core.reset();

    this.stats.activeBuffers = 0;

    // Null GL/canvas refs
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
      this.lastErrors.push(
        err("VALIDATION_NO_TRANSFORM", "DrawItem references missing transform", {
          drawItemId: di.id,
          transformId: id,
        })
      );
      return this.IDENTITY_MAT3;
    }
    return tr.mat3;
  }

  // -------------------- Control plane --------------------
  /**
   * Supported cmds:
   * - createBuffer {id}
   * - createGeometry {id, vertexBufferId, format:"pos2_clip", strideBytes?}
   * - createInstancedGeometry {id, instanceBufferId, instanceFormat:"rect4"|"candle6"|"glyph8", instanceStrideBytes}
   * - createDrawItem {id, geometryId, pipeline}
   * - setDrawItemPipeline {id, pipeline}
   * - delete {kind:"drawItem"|"geometry"|"transform"|"buffer", id}
   *
   * D1.5:
   * - createTransform {id}
   * - setTransform {id, tx?,ty?,sx?,sy?}
   * - attachTransform {targetId, transformId}
   *
   * D6.1 cache policy:
   * - bufferSetMaxBytes {id, maxBytes}
   * - bufferEvictFront {id, bytes}
   * - bufferKeepLast {id, bytes}
   *
   * D2.3:
   * - ensureGlyphs {chars, font?}
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
        if (obj.id === undefined) throw new Error("createBuffer: missing id");
        const id = toU32AllowZero(obj.id);
        this.createBuffer(id);
        return { ok: true };
      }

      if (cmd === "createGeometry") {
        if (obj.id === undefined) throw new Error("createGeometry: missing id");
        if (obj.vertexBufferId === undefined) throw new Error("createGeometry: missing vertexBufferId");

        const id = toU32AllowZero(obj.id);
        const vb = toU32AllowZero(obj.vertexBufferId);
        const fmt = obj.format ?? "pos2_clip";
        const strideBytes = typeof obj.strideBytes === "number" ? (obj.strideBytes | 0) : 8;

        if (fmt !== "pos2_clip") throw new Error("createGeometry: only format=pos2_clip supported");

        this.createVertexGeometry(id, vb, strideBytes);
        return { ok: true };
      }

      if (cmd === "createInstancedGeometry") {
        if (obj.id === undefined) throw new Error("createInstancedGeometry: missing id");
        if (obj.instanceBufferId === undefined) throw new Error("createInstancedGeometry: missing instanceBufferId");

        const id = toU32AllowZero(obj.id);
        const ib = toU32AllowZero(obj.instanceBufferId);
        const instanceFormat = String(obj.instanceFormat ?? "") as "rect4" | "candle6" | "glyph8";
        const instanceStrideBytes = typeof obj.instanceStrideBytes === "number" ? (obj.instanceStrideBytes | 0) : 0;

        if (instanceFormat !== "rect4" && instanceFormat !== "candle6" && instanceFormat !== "glyph8") {
          throw new Error("createInstancedGeometry: instanceFormat must be rect4|candle6|glyph8");
        }
        if (instanceStrideBytes <= 0) throw new Error("createInstancedGeometry: missing/invalid instanceStrideBytes");

        this.createInstancedGeometry(id, ib, instanceFormat, instanceStrideBytes);
        return { ok: true };
      }

      if (cmd === "createDrawItem") {
        if (obj.id === undefined) throw new Error("createDrawItem: missing id");
        if (obj.geometryId === undefined) throw new Error("createDrawItem: missing geometryId");

        const id = toU32AllowZero(obj.id);
        const gid = toU32AllowZero(obj.geometryId);
        const pipeline = toPipelineId(obj.pipeline ?? "triSolid@1");

        this.createDrawItem(id, gid, pipeline);
        return { ok: true };
      }

      if (cmd === "setDrawItemPipeline") {
        if (obj.id === undefined) throw new Error("setDrawItemPipeline: missing id");
        const id = toU32AllowZero(obj.id);
        const pipeline = toPipelineId(obj.pipeline);

        this.setDrawItemPipeline(id, pipeline);
        return { ok: true };
      }

      if (cmd === "delete") {
        if (obj.id === undefined) throw new Error("delete: missing id");
        const kind = String(obj.kind ?? "");
        const id = toU32AllowZero(obj.id);
        if (!kind) throw new Error("delete: missing kind");

        if (kind === "drawItem") this.deleteDrawItem(id);
        else if (kind === "geometry") this.deleteGeometry(id);
        else if (kind === "transform") this.deleteTransform(id);
        else if (kind === "buffer") this.deleteBuffer(id);
        else throw new Error(`delete: unknown kind '${kind}'`);

        return { ok: true };
      }

      // ---- D1.5 transforms ----
      if (cmd === "createTransform") {
        if (obj.id === undefined) throw new Error("createTransform: missing id");
        const id = toU32AllowZero(obj.id);
        this.createTransform(id);
        return { ok: true };
      }

      if (cmd === "setTransform") {
        if (obj.id === undefined) throw new Error("setTransform: missing id");
        const id = toU32AllowZero(obj.id);
        this.setTransform(id, { tx: obj.tx, ty: obj.ty, sx: obj.sx, sy: obj.sy });
        return { ok: true };
      }

      if (cmd === "attachTransform") {
        if (obj.targetId === undefined) throw new Error("attachTransform: missing targetId");
        if (obj.transformId === undefined) throw new Error("attachTransform: missing transformId");
        const targetId = toU32AllowZero(obj.targetId);
        const transformId = toU32AllowZero(obj.transformId);
        this.attachTransform(targetId, transformId);
        return { ok: true };
      }

      // ---- D6.1 cache policy ----
      if (cmd === "bufferSetMaxBytes") {
        if (obj.id === undefined) throw new Error("bufferSetMaxBytes: missing id");
        const id = toU32AllowZero(obj.id);
        const maxBytes = Number(obj.maxBytes);
        if (!Number.isFinite(maxBytes)) throw new Error("bufferSetMaxBytes: invalid maxBytes");
        this.core.ensureBuffer(id);
        this.core.setMaxBytes(id, maxBytes | 0);
        return { ok: true };
      }

      if (cmd === "bufferEvictFront") {
        if (obj.id === undefined) throw new Error("bufferEvictFront: missing id");
        const id = toU32AllowZero(obj.id);
        const bytes = Number(obj.bytes);
        if (!Number.isFinite(bytes)) throw new Error("bufferEvictFront: invalid bytes");
        this.core.evictFront(id, bytes | 0);
        return { ok: true };
      }

      if (cmd === "bufferKeepLast") {
        if (obj.id === undefined) throw new Error("bufferKeepLast: missing id");
        const id = toU32AllowZero(obj.id);
        const bytes = Number(obj.bytes);
        if (!Number.isFinite(bytes)) throw new Error("bufferKeepLast: invalid bytes");
        this.core.keepLast(id, bytes | 0);
        return { ok: true };
      }

      // ---- D2.3 text glyph uploads ----
      if (cmd === "ensureGlyphs") {
        const chars = String(obj.chars ?? "");
        if (!chars) throw new Error("ensureGlyphs: missing chars");
        if (obj.font) this.atlas.setFont(String(obj.font));
        this.ensureGlyphsUploaded(chars);
        return { ok: true };
      }

      // ---- debug ----
      if (cmd === "setDebug") {
        const showBounds = obj.showBounds;
        const wireframe = obj.wireframe;
        this.setDebugToggles({
          showBounds: typeof showBounds === "boolean" ? showBounds : undefined,
          wireframe: typeof wireframe === "boolean" ? wireframe : undefined,
        });
        return { ok: true };
      }

      return { ok: false, error: `Control: unknown cmd ${cmd}` };
    } catch (e: any) {
      return { ok: false, error: e?.message ?? String(e) };
    }
  }

  // -------------------- resource creation helpers --------------------
  createBuffer(bufferId: number) {
    this.core.ensureBuffer(bufferId);
    this.stats.activeBuffers = this.core.getActiveBufferCount();
  }

  private createVertexGeometry(id: number, vertexBufferId: number, strideBytes: number) {
    this.createBuffer(vertexBufferId);
    this.geometries.set(id, { kind: "vertex", id, vertexBufferId, strideBytes, format: "pos2_clip" });
  }

  private createInstancedGeometry(
    id: number,
    instanceBufferId: number,
    instanceFormat: InstancedGeometry["instanceFormat"],
    instanceStrideBytes: number
  ) {
    this.createBuffer(instanceBufferId);
    this.geometries.set(id, { kind: "instanced", id, instanceBufferId, instanceFormat, instanceStrideBytes });
  }

  private createDrawItem(id: number, geometryId: number, pipeline: PipelineId) {
    if (!this.geometries.has(geometryId)) throw new Error("createDrawItem: geometry not found");
    if (!PIPELINES[pipeline]) throw new Error(`createDrawItem: unknown pipeline '${pipeline}'`);
    this.drawItems.set(id, { id, geometryId, pipeline, transformId: null });
  }

  private setDrawItemPipeline(drawItemId: number, pipeline: PipelineId) {
    const di = this.drawItems.get(drawItemId);
    if (!di) throw new Error("setDrawItemPipeline: drawItem not found");
    if (!PIPELINES[pipeline]) throw new Error(`setDrawItemPipeline: unknown pipeline '${pipeline}'`);
    di.pipeline = pipeline;
  }

  // -------------------- D3: cascade deletes (prevents dangling references) --------------------
  private detachTransformFromDrawItems(transformId: number) {
    for (const di of this.drawItems.values()) {
      if (di.transformId === transformId) di.transformId = null;
    }
  }

  private deleteDrawItemsUsingGeometry(geometryId: number) {
    for (const [id, di] of this.drawItems) {
      if (di.geometryId === geometryId) this.drawItems.delete(id);
    }
  }

  private deleteGeometriesUsingBuffer(bufferId: number) {
    for (const [gid, g] of this.geometries) {
      if (g.kind === "vertex") {
        if (g.vertexBufferId !== bufferId) continue;
      } else {
        if (g.instanceBufferId !== bufferId) continue;
      }

      // delete dependent draw items then geometry
      this.deleteDrawItemsUsingGeometry(gid);
      this.geometries.delete(gid);
    }
  }

  deleteDrawItem(id: number) {
    this.drawItems.delete(id);
  }

  deleteGeometry(id: number) {
    // D3: delete dependent draw items first
    this.deleteDrawItemsUsingGeometry(id);
    this.geometries.delete(id);
  }

  deleteTransform(id: number) {
    // D3: detach from any draw item that references this transform
    this.detachTransformFromDrawItems(id);
    this.transforms.delete(id);
  }

  deleteBuffer(id: number) {
    // D3: remove dependent geometries + drawItems first (prevents dangling refs)
    this.deleteGeometriesUsingBuffer(id);

    // CPU
    this.core.deleteBuffer(id);

    // GPU
    const gl = this.gl;
    const gb = this.gpuBuffers.get(id);
    if (gl && gb) gl.deleteBuffer(gb.gl);
    this.gpuBuffers.delete(id);

    this.stats.activeBuffers = this.core.getActiveBufferCount();
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
      debug: { ...this.stats.debug },
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

      // pos2 packed at offset 0, 2 floats
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

    // per-frame reset
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

    // Draw items
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

      // dispatch
      if (di.pipeline === "triSolid@1") this.drawPos2(di, g as VertexGeometry, "triangles");
      else if (di.pipeline === "line2d@1") this.drawPos2(di, g as VertexGeometry, "lines");
      else if (di.pipeline === "points@1") this.drawPos2(di, g as VertexGeometry, "points");
      else if (di.pipeline === "instancedRect@1") this.drawInstancedRect(di, g as InstancedGeometry);
      else if (di.pipeline === "instancedCandle@1") this.drawInstancedCandle(di, g as InstancedGeometry);
      else if (di.pipeline === "textSDF@1") this.drawTextSdf(di, g as InstancedGeometry);
    }

    // timing + p95
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
      errors.push(
        err("VALIDATION_NO_TRANSFORM", "Attached transform not found", {
          drawItemId: di.id,
          transformId: di.transformId,
        })
      );
    }

    const isPos2 = spec.id === "triSolid@1" || spec.id === "line2d@1" || spec.id === "points@1";
    if (isPos2) {
      if (g.kind !== "vertex") {
        errors.push(
          err("VALIDATION_BAD_GEOMETRY_KIND", `${spec.id} requires vertex geometry`, { drawItemId: di.id, geometryId: g.id })
        );
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
        errors.push(
          err("VALIDATION_BAD_GEOMETRY_KIND", `${spec.id} requires instanced geometry`, { drawItemId: di.id, geometryId: g.id })
        );
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

      if (spec.id === "textSDF@1") {
        if (g.instanceFormat !== "glyph8") errors.push(err("VALIDATION_BAD_FORMAT", "textSDF@1 requires instanceFormat=glyph8", { got: g.instanceFormat }));
        if (g.instanceStrideBytes !== 32) errors.push(err("VALIDATION_BAD_STRIDE", "textSDF@1 requires instanceStrideBytes=32", { got: g.instanceStrideBytes }));
        if (!this.atlasTex) errors.push(err("TEXT_NO_ATLAS", "textSDF@1 atlas texture missing"));
      }

      const gb = this.gpuBuffers.get(g.instanceBufferId);
      if (!gb && this.core.getBufferBytes(g.instanceBufferId).byteLength > 0) {
        errors.push(err("VALIDATION_NO_BUFFER", "Instance buffer not uploaded on GPU", { instanceBufferId: g.instanceBufferId }));
      }

      return errors;
    }

    return errors;
  }

  // -------------------- draw implementations --------------------
  private drawPos2(di: DrawItem, g: VertexGeometry, mode: "triangles" | "lines" | "points") {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.vertexBufferId);
    const vcount = Math.floor(cpu.byteLength / g.strideBytes);

    let count = vcount;
    if (mode === "triangles") count = vcount - (vcount % 3);
    else if (mode === "lines") count = vcount - (vcount % 2);

    if (count <= 0) return;

    const gb = this.gpuBuffers.get(g.vertexBufferId);
    if (!gb) return;

    const p = this.progPos2!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));
    gl.uniform1f(p.u_pointSize!, mode === "points" ? 6.0 : 1.0);

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);
    gl.enableVertexAttribArray(p.a_pos!);
    gl.vertexAttribPointer(p.a_pos!, 2, gl.FLOAT, false, g.strideBytes, 0);
    gl.vertexAttribDivisor(p.a_pos!, 0);

    if (mode === "triangles") gl.drawArrays(gl.TRIANGLES, 0, count);
    else if (mode === "lines") gl.drawArrays(gl.LINES, 0, count);
    else gl.drawArrays(gl.POINTS, 0, count);

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

  private drawTextSdf(di: DrawItem, g: InstancedGeometry) {
    const gl = this.gl!;
    const cpu = this.core.getBufferBytes(g.instanceBufferId);
    const icount = Math.floor(cpu.byteLength / g.instanceStrideBytes);
    if (icount < 1) return;

    const gb = this.gpuBuffers.get(g.instanceBufferId);
    if (!gb) return;

    if (!this.atlasTex) {
      this.lastErrors.push(err("TEXT_NO_ATLAS", "No atlas texture for textSDF@1"));
      return;
    }

    // Enable blending for text (alpha coverage)
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    const p = this.progTextSdf!;
    gl.useProgram(p.prog);

    gl.uniformMatrix3fv(p.u_transform!, false, this.resolveTransformMat(di));

    gl.activeTexture(gl.TEXTURE0);
    gl.bindTexture(gl.TEXTURE_2D, this.atlasTex);
    if (p.u_atlas) gl.uniform1i(p.u_atlas, 0);

    if (p.u_color) gl.uniform4f(p.u_color, 0.92, 0.92, 0.95, 1.0);
    if (p.u_pxRange) gl.uniform1f(p.u_pxRange, this.atlas.sdfRange);

    gl.bindBuffer(gl.ARRAY_BUFFER, gb.gl);

    gl.enableVertexAttribArray(p.a_g0!);
    gl.vertexAttribPointer(p.a_g0!, 4, gl.FLOAT, false, g.instanceStrideBytes, 0);
    gl.vertexAttribDivisor(p.a_g0!, 1);

    gl.enableVertexAttribArray(p.a_g1!);
    gl.vertexAttribPointer(p.a_g1!, 4, gl.FLOAT, false, g.instanceStrideBytes, 16);
    gl.vertexAttribDivisor(p.a_g1!, 1);

    gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, icount);
    this.stats.drawCalls++;

    // Restore default state (keep engine predictable)
    gl.disable(gl.BLEND);
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

  // -------------------- Text atlas upload --------------------
  private initAtlasTexture() {
    const gl = this.gl!;
    const tex = gl.createTexture();
    if (!tex) throw new Error("Failed to create atlas texture");
    this.atlasTex = tex;

    gl.bindTexture(gl.TEXTURE_2D, tex);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

    const size = this.atlas.atlasSize;
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.R8, size, size, 0, gl.RED, gl.UNSIGNED_BYTE, null);
  }

  private ensureGlyphsUploaded(chars: string) {
    const gl = this.gl!;
    if (!this.atlasTex) this.initAtlasTexture();

    const added = this.atlas.ensureGlyphs(chars);
    if (added.length === 0) return;

    gl.bindTexture(gl.TEXTURE_2D, this.atlasTex);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

    for (const a of added) {
      if (a.g.ax + a.g.w > this.atlas.atlasSize || a.g.ay + a.g.h > this.atlas.atlasSize) {
        this.lastErrors.push(err("TEXT_ATLAS_FULL", "Glyph atlas full; cannot upload glyph", { codepoint: a.g.codepoint }));
        continue;
      }

      gl.texSubImage2D(gl.TEXTURE_2D, 0, a.g.ax, a.g.ay, a.g.w, a.g.h, gl.RED, gl.UNSIGNED_BYTE, a.sdfR8);
    }
  }

  // -------------------- HUD --------------------
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
      if (a === "a_g0") out.a_g0 = loc;
      if (a === "a_g1") out.a_g1 = loc;
    }

    for (const u of uniforms) {
      const loc = gl.getUniformLocation(prog, u);
      if (u === "u_transform") out.u_transform = loc;
      if (u === "u_pointSize") out.u_pointSize = loc;
      if (u === "u_atlas") out.u_atlas = loc;
      if (u === "u_color") out.u_color = loc;
      if (u === "u_pxRange") out.u_pxRange = loc;
    }

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
function toU32AllowZero(v: any): number {
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
  // Column-major array for GLSL:
  return new Float32Array([p.sx, 0, 0, 0, p.sy, 0, p.tx, p.ty, 1]);
}

function applyMat3ToPos2(M: Float32Array, x: number, y: number): { x: number; y: number } {
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
