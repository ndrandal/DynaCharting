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
};

type PickableTri = {
  drawItemId: number;
  // 3 vertices in clip/NDC space: [x0,y0,x1,y1,x2,y2]
  verts: Float32Array;
};

export class EngineHost {
  private canvas: HTMLCanvasElement | null = null;
  private gl: WebGL2RenderingContext | null = null;

  private running = false;
  private raf = 0;

  private frames = 0;
  private lastFpsT = 0;

  // Basic stats (not required for D1.4, but harmless)
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

  private pendingUploadBytes = 0;

  private buffers = new Map<number, WebGLBuffer>();

  // ---- D1.4 picking + minimal draw ----
  private tris: PickableTri[] = [];
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

    // Build a minimal shader for triangles in clip space
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
         outColor = vec4(0.85, 0.15, 0.20, 1.0); // red accent
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

    if (this.gl) {
      for (const b of this.buffers.values()) this.gl.deleteBuffer(b);
      if (this.prog) this.gl.deleteProgram(this.prog);
    }

    this.buffers.clear();
    this.tris = [];
    this.prog = null;

    this.gl = null;
    this.canvas = null;
  }

  // ---------------- D1.4 public API ----------------

  /**
   * Register/update a pickable triangle in CLIP SPACE.
   * vertsClip = Float32Array([x0,y0,x1,y1,x2,y2]) where each is in [-1..1]
   */
  setPickableTriangle(drawItemId: number, vertsClip: Float32Array) {
    if (vertsClip.length !== 6) throw new Error("setPickableTriangle: vertsClip must have length 6");
    // Keep CPU copy for picking
    const existing = this.tris.find(t => t.drawItemId === drawItemId);
    if (existing) {
      existing.verts = new Float32Array(vertsClip);
    } else {
      this.tris.push({ drawItemId, verts: new Float32Array(vertsClip) });
    }

    // Upload to GPU buffer keyed by drawItemId (simple mapping for v1)
    this.uploadBuffer(drawItemId, vertsClip);
  }

  /**
   * pick(x,y) in *canvas CSS pixels* (use event coords relative to canvas).
   * Returns {drawItemId} if hit else null.
   */
  pick(x: number, y: number): PickResult {
    const canvas = this.canvas;
    if (!canvas) return null;

    const rect = canvas.getBoundingClientRect();
    const w = rect.width || 1;
    const h = rect.height || 1;

    // Convert to NDC/clip: x in [-1..1], y in [-1..1] with +y up
    const ndcX = (x / w) * 2 - 1;
    const ndcY = 1 - (y / h) * 2;

    for (let i = this.tris.length - 1; i >= 0; i--) {
      const t = this.tris[i];
      if (pointInTri(ndcX, ndcY, t.verts)) {
        return { drawItemId: t.drawItemId };
      }
    }
    return null;
  }

  // ---------------- existing buffer plumbing ----------------

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

  // ---------------- render loop ----------------

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

    // Minimal triangle render: draw each registered tri
    if (this.prog && this.aPosLoc >= 0) {
      gl.useProgram(this.prog);

      for (const tri of this.tris) {
        const b = this.buffers.get(tri.drawItemId);
        if (!b) continue;

        gl.bindBuffer(gl.ARRAY_BUFFER, b);
        gl.enableVertexAttribArray(this.aPosLoc);
        gl.vertexAttribPointer(this.aPosLoc, 2, gl.FLOAT, false, 0, 0);

        gl.drawArrays(gl.TRIANGLES, 0, 3);
        this.stats.drawCalls++;
      }
    }

    const t1 = performance.now();
    this.stats.frameMs = t1 - t0;
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

// -------- Picking helpers (CPU) --------

// Barycentric / sign test in 2D
function pointInTri(px: number, py: number, v: Float32Array): boolean {
  const x0 = v[0], y0 = v[1];
  const x1 = v[2], y1 = v[3];
  const x2 = v[4], y2 = v[5];

  const b0 = sign(px, py, x0, y0, x1, y1) < 0;
  const b1 = sign(px, py, x1, y1, x2, y2) < 0;
  const b2 = sign(px, py, x2, y2, x0, y0) < 0;

  return (b0 === b1) && (b1 === b2);
}

function sign(px: number, py: number, ax: number, ay: number, bx: number, by: number): number {
  return (px - bx) * (ay - by) - (ax - bx) * (py - by);
}
