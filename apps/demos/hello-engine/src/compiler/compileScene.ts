// apps/demos/hello-engine/src/compiler/compileScene.ts
import type { SceneSpecV0, ViewTransform, SpaceSpecV0 } from "./SceneSpec";
import { PlanHandle } from "./PlanHandle";

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

function spaceToClipTransform(space: SpaceSpecV0, domainOverride?: { xMin: number; xMax: number; yMin: number; yMax: number }): ViewTransform {
  if (space.type === "clip") return { tx: 0, ty: 0, sx: 1, sy: 1 };

  if (space.type === "screen01") {
    // (0,0) top-left -> (-1, 1); (1,1) bottom-right -> (1,-1)
    return { tx: -1, ty: 1, sx: 2, sy: -2 };
  }

  if (space.type === "domain2d") {
    const d = domainOverride ?? space.domain;

    mustFinite(d.xMin, "domain2d.xMin");
    mustFinite(d.xMax, "domain2d.xMax");
    mustFinite(d.yMin, "domain2d.yMin");
    mustFinite(d.yMax, "domain2d.yMax");

    const dx = d.xMax - d.xMin;
    const dy = d.yMax - d.yMin;
    if (dx === 0 || dy === 0) throw new Error("compileScene: domain2d range cannot be 0");

    // maps domain -> clip
    return {
      sx: 2 / dx,
      tx: -1 - (2 * d.xMin) / dx,
      sy: 2 / dy,
      ty: -1 - (2 * d.yMin) / dy
    };
  }

  throw new Error(`compileScene: unknown space.type '${(space as any)?.type}'`);
}

// -------------------- axis geometry generation --------------------
// Produces pos2 floats (x,y) in *space coordinates* (screen01), drawn with LINES.
// IMPORTANT: keep vertex count constant to avoid “tail garbage” because we only append once.
function buildAxisLinesScreen01(opts: { side: "bottom" | "top" | "left" | "right"; ticks: number; tickLen: number; inset: number }): Float32Array {
  const ticks = Math.max(2, opts.ticks | 0);
  const tickLen = Math.max(0, opts.tickLen);
  const inset = Math.max(0, Math.min(0.2, opts.inset));

  // Constant topology:
  // - one main axis line segment (2 verts)
  // - ticks segments: ticks (each 2 verts) => 2*ticks verts
  // Total verts = 2 + 2*ticks
  // Total floats = (2 + 2*ticks)*2
  const verts = 2 + 2 * ticks;
  const out = new Float32Array(verts * 2);

  let w0 = inset;
  let w1 = 1 - inset;

  if (opts.side === "bottom" || opts.side === "top") {
    const y = opts.side === "bottom" ? (1 - inset) : inset;

    // axis main line
    out[0] = w0; out[1] = y;
    out[2] = w1; out[3] = y;

    // ticks
    for (let i = 0; i < ticks; i++) {
      const t = ticks === 1 ? 0.5 : i / (ticks - 1);
      const x = w0 + (w1 - w0) * t;
      const up = (opts.side === "bottom") ? -tickLen : tickLen;

      const base = 4 + i * 4;
      out[base + 0] = x; out[base + 1] = y;
      out[base + 2] = x; out[base + 3] = y + up;
    }

    return out;
  } else {
    const x = opts.side === "left" ? inset : (1 - inset);

    // axis main line
    out[0] = x; out[1] = w0;
    out[2] = x; out[3] = w1;

    // ticks
    for (let i = 0; i < ticks; i++) {
      const t = ticks === 1 ? 0.5 : i / (ticks - 1);
      const y = w0 + (w1 - w0) * t;
      const right = (opts.side === "left") ? tickLen : -tickLen;

      const base = 4 + i * 4;
      out[base + 0] = x; out[base + 1] = y;
      out[base + 2] = x + right; out[base + 3] = y;
    }

    return out;
  }
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

  // Track live domain overrides (D5.1 runtime updates)
  const domainOverrides = new Map<string, { xMin: number; xMax: number; yMin: number; yMax: number }>();
  for (const s of spec.spaces) {
    if (s.type === "domain2d") domainOverrides.set(s.id, { ...s.domain });
  }

  const plan = new PlanHandle();

  // Runtime support per layer
  const layerTransformIds: number[] = [];
  const layerViewId: string[] = [];
  const layerSpaceId: string[] = [];

  let sceneView: ViewTransform = { tx: 0, ty: 0, sx: 1, sy: 1 };

  // Compile layers
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

    // Dispose in reverse dependency order
    plan.addDispose({ cmd: "delete", kind: "drawItem", id: di });
    plan.addDispose({ cmd: "delete", kind: "geometry", id: geo });
    plan.addDispose({ cmd: "delete", kind: "transform", id: tr });
    plan.addDispose({ cmd: "delete", kind: "buffer", id: buf });

    // Geometry/drawItem
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
    } else if (layer.mark.kind === "axis2d") {
      plan.addCommand({
        cmd: "createGeometry",
        id: geo,
        vertexBufferId: buf,
        format: "pos2_clip",
        strideBytes: 8
      });

      const pipeline = layer.mark.pipeline ?? "line2d@1";
      plan.addCommand({ cmd: "createDrawItem", id: di, geometryId: geo, pipeline });
      plan.addCommand({ cmd: "attachTransform", targetId: di, transformId: tr });

      // Compiler-generated static bytes (axis line segments)
      const ticks = layer.mark.axis.ticks ?? 6;
      const tickLen = layer.mark.axis.tickLen ?? 0.03;
      const inset = layer.mark.axis.inset ?? 0.02;

      const lines = buildAxisLinesScreen01({ side: layer.mark.axis.side, ticks, tickLen, inset });
      plan.addInitialBatch(makeAppendBatch(buf, new Uint8Array(lines.buffer)));
    } else {
      const _exhaustive: never = layer.mark;
      throw new Error("compileScene: unsupported mark kind");
    }

    // Data (skip if compiler generated axis data)
    if (layer.data.kind === "workerStream") {
      plan.addSubscription({ kind: "workerStream", stream: layer.data.stream, bufferId: buf });
    } else if (layer.data.kind === "staticAppend") {
      plan.addInitialBatch(makeAppendBatch(buf, layer.data.bytes));
    } else if (layer.data.kind === "none") {
      // ok
    } else {
      const _exhaustive: never = layer.data;
      throw new Error("compileScene: unsupported data kind");
    }

    // Track layer for runtime transform updates
    layerTransformIds.push(tr);
    layerViewId.push(layer.viewId);
    layerSpaceId.push(layer.spaceId);

    // Initialize transform now
    const viewMap = viewRectToClipTransform(view.rect);
    const dom = space.type === "domain2d" ? domainOverrides.get(space.id) : undefined;
    const spaceMap = spaceToClipTransform(space, dom);
    const base = compose(viewMap, spaceMap);
    const initial = compose(sceneView, base);

    plan.addCommand({ cmd: "setTransform", id: tr, tx: initial.tx, ty: initial.ty, sx: initial.sx, sy: initial.sy });
  }

  function recomputeAll(apply: (cmd: any) => { ok: true } | { ok: false; error: string }) {
    for (let i = 0; i < layerTransformIds.length; i++) {
      const trId = layerTransformIds[i];
      const vId = layerViewId[i];
      const sId = layerSpaceId[i];

      const view = views.get(vId);
      const space = spaces.get(sId);
      if (!view || !space) continue;

      const viewMap = viewRectToClipTransform(view.rect);
      const dom = space.type === "domain2d" ? domainOverrides.get(space.id) : undefined;
      const spaceMap = spaceToClipTransform(space, dom);
      const base = compose(viewMap, spaceMap);
      const final = compose(sceneView, base);

      const r = apply({ cmd: "setTransform", id: trId, tx: final.tx, ty: final.ty, sx: final.sx, sy: final.sy });
      if (!r.ok) {
        // eslint-disable-next-line no-console
        console.warn("compileScene runtime: setTransform failed", r.error, { trId, vId, sId });
      }
    }
  }

  return {
    plan,
    runtime: {
      setView(apply: (cmd: any) => { ok: true } | { ok: false; error: string }, t: ViewTransform) {
        sceneView = { tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy };
        recomputeAll(apply);
      },

      setDomain(
        apply: (cmd: any) => { ok: true } | { ok: false; error: string },
        spaceId: string,
        domain: { xMin: number; xMax: number; yMin: number; yMax: number }
      ) {
        const s = spaces.get(spaceId);
        if (!s || s.type !== "domain2d") {
          // eslint-disable-next-line no-console
          console.warn("setDomain ignored: not a domain2d space:", spaceId);
          return;
        }
        domainOverrides.set(spaceId, { ...domain });
        recomputeAll(apply);
      }
    }
  };
}
