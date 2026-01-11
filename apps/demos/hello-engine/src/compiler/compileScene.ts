// apps/demos/hello-engine/src/compiler/compileScene.ts
import type { SceneSpecV0, ViewTransform } from "./SceneSpec";
import { PlanHandle } from "./PlanHandle";
import type { WorkerSubscription } from "../protocol";

const OP_APPEND = 1;

function mustFinite(n: any, name: string) {
  if (!Number.isFinite(n)) throw new Error(`compileScene: ${name} must be finite`);
}

function makeAppendBatch(bufferId: number, payload: Uint8Array): ArrayBuffer {
  const headerBytes = 1 + 4 + 4 + 4;
  const out = new ArrayBuffer(headerBytes + payload.byteLength);
  const dv = new DataView(out);

  let p = 0;
  dv.setUint8(p, OP_APPEND); p += 1;
  dv.setUint32(p, bufferId >>> 0, true); p += 4;
  dv.setUint32(p, 0, true); p += 4;
  dv.setUint32(p, payload.byteLength >>> 0, true); p += 4;
  new Uint8Array(out, p).set(payload);

  return out;
}

// result = a ∘ b  (apply b first)
function compose(a: ViewTransform, b: ViewTransform): ViewTransform {
  return {
    sx: a.sx * b.sx,
    sy: a.sy * b.sy,
    tx: a.tx + a.sx * b.tx,
    ty: a.ty + a.sy * b.ty
  };
}

function viewRectToClipTransform(rect: { x: number; y: number; w: number; h: number }): ViewTransform {
  const x = rect.x, y = rect.y, w = rect.w, h = rect.h;
  mustFinite(x, "view.rect.x"); mustFinite(y, "view.rect.y");
  mustFinite(w, "view.rect.w"); mustFinite(h, "view.rect.h");

  const left = -1 + 2 * x;
  const top  =  1 - 2 * y;
  const cx = left + w;
  const cy = top - h;

  return { tx: cx, ty: cy, sx: w, sy: h };
}

function spaceToClipTransform(space: any): ViewTransform {
  if (space.type === "clip") return { tx: 0, ty: 0, sx: 1, sy: 1 };

  if (space.type === "screen01") {
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

    return {
      sx: 2 / dx,
      tx: -1 - (2 * d.xMin) / dx,
      sy: 2 / dy,
      ty: -1 - (2 * d.yMin) / dy
    };
  }

  throw new Error(`compileScene: unknown space.type '${space?.type}'`);
}

export function compileScene(spec: SceneSpecV0, opts: { idBase: number }) {
  if (!spec || spec.version !== 0) {
    throw new Error(`compileScene: unsupported spec version: ${(spec as any)?.version}`);
  }

  const idBase = opts.idBase | 0;
  let nextLocal = 1;
  const alloc = () => (idBase + nextLocal++) >>> 0;

  const spaces = new Map(spec.spaces.map((s) => [s.id, s] as const));
  const views = new Map(spec.views.map((v) => [v.id, v] as const));

  const plan = new PlanHandle();

  // Layer runtime support: store (transformId, baseTransform)
  const layerTransformIds: number[] = [];
  const layerBase: ViewTransform[] = [];
  let sceneView: ViewTransform = { tx: 0, ty: 0, sx: 1, sy: 1 };

  for (const layer of spec.layers) {
    const view = views.get(layer.viewId);
    if (!view) throw new Error(`compileScene: layer '${layer.id}' missing view '${layer.viewId}'`);
    const space = spaces.get(layer.spaceId);
    if (!space) throw new Error(`compileScene: layer '${layer.id}' missing space '${layer.spaceId}'`);

    if (view.rect.units !== "relative") throw new Error("compileScene: only relative view rect supported in v0");

    const buf = alloc();
    const geo = alloc();
    const di  = alloc();
    const tr  = alloc();

    plan.created.buffers.add(buf);
    plan.created.geometries.add(geo);
    plan.created.drawItems.add(di);
    plan.created.transforms.add(tr);

    // Create in forward dependency order
    plan.addCommand({ cmd: "createBuffer", id: buf });
    plan.addCommand({ cmd: "createTransform", id: tr });

    // Dispose in reverse dependency order (push in that order)
    plan.addDispose({ cmd: "delete", kind: "drawItem", id: di });
    plan.addDispose({ cmd: "delete", kind: "geometry", id: geo });
    plan.addDispose({ cmd: "delete", kind: "transform", id: tr });
    plan.addDispose({ cmd: "delete", kind: "buffer", id: buf });

    // Geometry/drawItem based on mark kind
    if (layer.mark.kind === "lineStrip" || layer.mark.kind === "points") {
      plan.addCommand({
        cmd: "createGeometry",
        id: geo,
        vertexBufferId: buf,
        format: "pos2_clip",
        strideBytes: 8
      });

      const pipeline = layer.mark.pipeline ?? (layer.mark.kind === "points" ? "points@1" : "line2d@1");
      plan.addCommand({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      plan.addCommand({ cmd: "attachTransform", targetId: di, transformId: tr });
    } else if (layer.mark.kind === "instancedRect") {
      plan.addCommand({
        cmd: "createInstancedGeometry",
        id: geo,
        instanceBufferId: buf,
        instanceFormat: "rect4",
        instanceStrideBytes: 16
      });

      const pipeline = layer.mark.pipeline ?? "instancedRect@1";
      plan.addCommand({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      plan.addCommand({ cmd: "attachTransform", targetId: di, transformId: tr });
    } else if (layer.mark.kind === "textSDF") {
      plan.addCommand({
        cmd: "createInstancedGeometry",
        id: geo,
        instanceBufferId: buf,
        instanceFormat: "glyph8",
        instanceStrideBytes: 32
      });

      const pipeline = layer.mark.pipeline ?? "textSDF@1";
      plan.addCommand({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      plan.addCommand({ cmd: "attachTransform", targetId: di, transformId: tr });
    } else {
      const _exhaustive: never = layer.mark;
      throw new Error("compileScene: unsupported mark kind");
    }

    // Data
    if (layer.data.kind === "workerStream") {
      plan.addSubscription({ kind: "workerStream", stream: layer.data.stream, bufferId: buf });
    } else if (layer.data.kind === "staticAppend") {
      plan.addInitialBatch(makeAppendBatch(buf, layer.data.bytes));
    } else {
      const _exhaustive: never = layer.data;
      throw new Error("compileScene: unsupported data kind");
    }

    // Base transform = viewRect ∘ spaceMapping
    const viewMap = viewRectToClipTransform(view.rect);
    const spaceMap = spaceToClipTransform(space);
    const base = compose(viewMap, spaceMap);

    layerTransformIds.push(tr);
    layerBase.push(base);

    // Initialize transform now (sceneView ∘ base)
    const initial = compose(sceneView, base);
    plan.addCommand({ cmd: "setTransform", id: tr, tx: initial.tx, ty: initial.ty, sx: initial.sx, sy: initial.sy });
  }

  return {
    plan,
    runtime: {
      setView(apply: (cmd: any) => { ok: true } | { ok: false; error: string }, t: ViewTransform) {
        sceneView = { tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy };
        for (let i = 0; i < layerTransformIds.length; i++) {
          const trId = layerTransformIds[i];
          const base = layerBase[i];
          const final = compose(sceneView, base);
          const r = apply({ cmd: "setTransform", id: trId, tx: final.tx, ty: final.ty, sx: final.sx, sy: final.sy });
          if (!r.ok) throw new Error(r.error);
        }
      }
    }
  };
}
