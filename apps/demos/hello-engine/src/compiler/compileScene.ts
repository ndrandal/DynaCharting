// apps/demos/hello-engine/src/compiler/compileScene.ts
import type { CompiledPlan, SceneSpecV0, ViewTransform } from "./SceneSpec";
import type { WorkerSubscription } from "../protocol";

const OP_APPEND = 1;

function mustFinite(n: any, name: string) {
  if (!Number.isFinite(n)) throw new Error(`compileScene: ${name} must be finite`);
}

function makeAppendBatch(bufferId: number, payload: Uint8Array): ArrayBuffer {
  // record: u8 op, u32 bufferId, u32 offsetBytes, u32 len, payload
  const headerBytes = 1 + 4 + 4 + 4;
  const out = new ArrayBuffer(headerBytes + payload.byteLength);
  const dv = new DataView(out);

  let p = 0;
  dv.setUint8(p, OP_APPEND); p += 1;
  dv.setUint32(p, bufferId >>> 0, true); p += 4;
  dv.setUint32(p, 0, true); p += 4; // offsetBytes unused for append
  dv.setUint32(p, payload.byteLength >>> 0, true); p += 4;

  new Uint8Array(out, p).set(payload);
  return out;
}

// Compose affine transforms (scale+translate only)
// result = a ∘ b  (apply b first, then a)
function compose(a: ViewTransform, b: ViewTransform): ViewTransform {
  return {
    sx: a.sx * b.sx,
    sy: a.sy * b.sy,
    tx: a.tx + a.sx * b.tx,
    ty: a.ty + a.sy * b.ty
  };
}

function viewRectToClipTransform(rect: { x: number; y: number; w: number; h: number }): ViewTransform {
  // rect in relative units [0..1], origin top-left
  // map layer coords in clip [-1..1] into the view sub-rect in clip.
  const x = rect.x, y = rect.y, w = rect.w, h = rect.h;

  mustFinite(x, "view.rect.x"); mustFinite(y, "view.rect.y");
  mustFinite(w, "view.rect.w"); mustFinite(h, "view.rect.h");

  // clip rect:
  const left = -1 + 2 * x;
  const top  =  1 - 2 * y;
  // width/height in clip:
  // widthClip = 2*w, heightClip = 2*h
  const cx = left + w;      // center x in clip
  const cy = top - h;       // center y in clip

  // mapping from full clip [-1..1] to sub-clip:
  // x' = w*x + cx
  // y' = h*y + cy
  return { tx: cx, ty: cy, sx: w, sy: h };
}

function spaceToClipTransform(space: any): ViewTransform {
  if (space.type === "clip") {
    return { tx: 0, ty: 0, sx: 1, sy: 1 };
  }

  if (space.type === "screen01") {
    // layer coords are 0..1 within view; origin top-left
    // map to full-clip:
    // xClip = -1 + 2*x
    // yClip =  1 - 2*y
    return { tx: -1, ty: 1, sx: 2, sy: -2 };
  }

  if (space.type === "domain2d") {
    const d = space.domain;
    mustFinite(d.xMin, "domain2d.xMin");
    mustFinite(d.xMax, "domain2d.xMax");
    mustFinite(d.yMin, "domain2d.yMin");
    mustFinite(d.yMax, "domain2d.yMax");

    const dx = d.xMax - d.xMin;
    const dy = d.yMax - d.yMin;
    if (dx === 0 || dy === 0) throw new Error("compileScene: domain2d range cannot be 0");

    // xClip = 2*(x-xMin)/dx - 1  => sx=2/dx, tx= -1 -2*xMin/dx
    // yClip = 2*(y-yMin)/dy - 1  => sx=2/dy, ty= -1 -2*yMin/dy
    return {
      sx: 2 / dx,
      tx: -1 - (2 * d.xMin) / dx,
      sy: 2 / dy,
      ty: -1 - (2 * d.yMin) / dy
    };
  }

  throw new Error(`compileScene: unknown space.type '${space?.type}'`);
}

export function compileScene(spec: SceneSpecV0, opts: { idBase: number }): CompiledPlan {
  if (!spec || spec.version !== 0) {
    throw new Error(`compileScene: unsupported spec version: ${(spec as any)?.version}`);
  }

  const idBase = opts.idBase | 0;
  let nextLocal = 1;
  const alloc = () => (idBase + nextLocal++) >>> 0;

  const spaces = new Map(spec.spaces.map((s) => [s.id, s] as const));
  const views = new Map(spec.views.map((v) => [v.id, v] as const));

  const commands: any[] = [];
  const dispose: any[] = [];
  const subscriptions: WorkerSubscription[] = [];
  const initialBatches: ArrayBuffer[] = [];

  // For runtime view updates: we store layer transform ids and each layer’s “base” transform.
  const layerTransformIds: number[] = [];
  const layerBase: ViewTransform[] = [];

  // Scene view transform starts identity; runtime will recompose on updates
  let sceneView: ViewTransform = { tx: 0, ty: 0, sx: 1, sy: 1 };

  for (const layer of spec.layers) {
    const view = views.get(layer.viewId);
    if (!view) throw new Error(`compileScene: layer '${layer.id}' references missing view '${layer.viewId}'`);

    const space = spaces.get(layer.spaceId);
    if (!space) throw new Error(`compileScene: layer '${layer.id}' references missing space '${layer.spaceId}'`);

    if (view.rect.units !== "relative") throw new Error("compileScene: only view.rect.units=relative supported in v0");

    const buf = alloc();
    const geo = alloc();
    const di = alloc();
    const tr = alloc();

    // Create buffer + transform
    commands.push({ cmd: "createBuffer", id: buf });
    commands.push({ cmd: "createTransform", id: tr });

    // Dispose in reverse-safe order
    dispose.unshift({ cmd: "delete", kind: "buffer", id: buf });
    dispose.unshift({ cmd: "delete", kind: "transform", id: tr });

    // Geometry + draw item based on mark kind
    if (layer.mark.kind === "lineStrip" || layer.mark.kind === "points") {
      commands.push({
        cmd: "createGeometry",
        id: geo,
        vertexBufferId: buf,
        format: "pos2_clip",
        strideBytes: 8
      });
      dispose.unshift({ cmd: "delete", kind: "geometry", id: geo });

      const pipeline = layer.mark.pipeline ?? (layer.mark.kind === "points" ? "points@1" : "line2d@1");

      commands.push({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      dispose.unshift({ cmd: "delete", kind: "drawItem", id: di });

      commands.push({ cmd: "attachTransform", targetId: di, transformId: tr });

      // Point size is a uniform; EngineHost supports u_pointSize internally for points@1
      // We can wire it later with a "setUniform" command if you add one.
    } else if (layer.mark.kind === "instancedRect") {
      commands.push({
        cmd: "createInstancedGeometry",
        id: geo,
        instanceBufferId: buf,
        instanceFormat: "rect4",
        instanceStrideBytes: 16
      });
      dispose.unshift({ cmd: "delete", kind: "geometry", id: geo });

      const pipeline = layer.mark.pipeline ?? "instancedRect@1";

      commands.push({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      dispose.unshift({ cmd: "delete", kind: "drawItem", id: di });

      commands.push({ cmd: "attachTransform", targetId: di, transformId: tr });
    } else if (layer.mark.kind === "textSDF") {
      // Supports glyph8 (8 floats / 32 bytes): x0,y0,x1,y1,u0,v0,u1,v1
      commands.push({
        cmd: "createInstancedGeometry",
        id: geo,
        instanceBufferId: buf,
        instanceFormat: "glyph8",
        instanceStrideBytes: 32
      });
      dispose.unshift({ cmd: "delete", kind: "geometry", id: geo });

      const pipeline = layer.mark.pipeline ?? "textSDF@1";

      commands.push({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      dispose.unshift({ cmd: "delete", kind: "drawItem", id: di });

      commands.push({ cmd: "attachTransform", targetId: di, transformId: tr });

      // NOTE: for text to render correctly, the caller must also ensure glyphs are uploaded
      // and provide correct UVs in glyph8 instances. We'll make that ergonomic next sprint.
    } else {
      const _exhaustive: never = layer.mark;
      throw new Error(`compileScene: unsupported mark kind`);
    }

    // Data bindings
    if (layer.data.kind === "workerStream") {
      subscriptions.push({ kind: "workerStream", stream: layer.data.stream, bufferId: buf });
    } else if (layer.data.kind === "staticAppend") {
      initialBatches.push(makeAppendBatch(buf, layer.data.bytes));
    } else {
      const _exhaustive: never = layer.data;
      throw new Error("compileScene: unsupported data kind");
    }

    // ---- Compute base transform = viewRect ∘ spaceMapping
    const viewMap = viewRectToClipTransform(view.rect);
    const spaceMap = spaceToClipTransform(space);
    const base = compose(viewMap, spaceMap);

    layerTransformIds.push(tr);
    layerBase.push(base);

    // Initialize transform = sceneView ∘ base
    const initial = compose(sceneView, base);
    commands.push({ cmd: "setTransform", id: tr, tx: initial.tx, ty: initial.ty, sx: initial.sx, sy: initial.sy });
  }

  return {
    commands,
    subscriptions,
    initialBatches,
    dispose,
    runtime: {
      setView(hostApply, t) {
        sceneView = { tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy };

        for (let i = 0; i < layerTransformIds.length; i++) {
          const trId = layerTransformIds[i];
          const base = layerBase[i];
          const final = compose(sceneView, base);
          const r = hostApply({ cmd: "setTransform", id: trId, tx: final.tx, ty: final.ty, sx: final.sx, sy: final.sy });
          if (!r.ok) throw new Error(r.error);
        }
      }
    }
  };
}
