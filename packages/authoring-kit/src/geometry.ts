/* packages/authoring-kit/src/geometry.ts — ENC-714
 *
 * Pure geometry generators: turn high-level shapes (polylines, areas, polygons,
 * circles, wedges, gradient fills) into the flat vertex-float arrays the engine
 * pipelines consume. NO host, NO DOM — just shape → floats.
 *
 * Two output layouts appear here:
 *  - `pos2` / `rect4` (plain positions):  flat [x,y, …] or [x0,y0,x1,y1, …].
 *  - `pos2_color4` (per-vertex gradient): flat [x,y, r,g,b,a, …] via
 *    {@link triGradientFloats}, from a list of {x,y,rgba} vertices.
 *
 * Ported from the authoring-corpus runner marks (line/area/polygon/circle/wedge/
 * triGradient/gradientRect/gradientDisc/areaGrad/rectsColored/scatter geometry).
 */

import { lerp } from "./math";
import { mixColor } from "./color";
import type { Rgba, RgbaLike } from "./color";

/** A 2-D point in data or clip space. */
export interface Point {
  x: number;
  y: number;
}

/** A per-vertex colored vertex for the `pos2_color4` (triGradient) format. */
export interface ColorVertex {
  x: number;
  y: number;
  rgba: RgbaLike;
}

/** An axis-aligned rect in data/clip space. */
export interface Rect {
  x0: number;
  y0: number;
  x1: number;
  y1: number;
}

// -------------------- plain-position (pos2 / rect4) --------------------

/**
 * Connect an ordered point list into `rect4` segments `[x0,y0,x1,y1, …]` — one
 * segment per adjacent pair — for the `lineAA@1` pipeline. N points → N-1
 * segments.
 */
export function polylineSegments(pts: readonly Point[]): number[] {
  const out: number[] = [];
  for (let i = 0; i < pts.length - 1; i++) {
    out.push(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
  }
  return out;
}

/**
 * Fill under a curve down to a horizontal `baseline` as `pos2` triangles (2 tris
 * per gap) for the `triSolid@1` pipeline. N points → (N-1)*6 vertices.
 */
export function areaTriangles(pts: readonly Point[], baseline: number): number[] {
  const out: number[] = [];
  for (let i = 0; i < pts.length - 1; i++) {
    const p = pts[i];
    const q = pts[i + 1];
    out.push(p.x, p.y, p.x, baseline, q.x, q.y, q.x, q.y, p.x, baseline, q.x, baseline);
  }
  return out;
}

/**
 * Triangulate a convex polygon as a fan from `center` to each ring edge, as
 * `pos2` triangles for `triSolid@1`. A ring of N vertices → N triangles
 * (N*3 vertices), closing back to ring[0]. The ring is treated as CLOSED.
 */
export function triangulateFan(center: Point, ring: readonly Point[]): number[] {
  const out: number[] = [];
  const n = ring.length;
  for (let i = 0; i < n; i++) {
    const p = ring[i];
    const q = ring[(i + 1) % n];
    out.push(center.x, center.y, p.x, p.y, q.x, q.y);
  }
  return out;
}

/**
 * Sample a circle/ellipse ring of `segs` points. `aspect` scales the x-radius
 * (pass H/W to keep a clip-space circle round despite non-square viewports).
 * Returns the ring points (feed to {@link triangulateFan} with center {cx,cy}).
 */
export function circleRing(
  cx: number,
  cy: number,
  r: number,
  segs = 48,
  aspect = 1,
): Point[] {
  const ring: Point[] = [];
  for (let i = 0; i < segs; i++) {
    const t = (2 * Math.PI * i) / segs;
    ring.push({ x: cx + r * aspect * Math.cos(t), y: cy + r * Math.sin(t) });
  }
  return ring;
}

/**
 * A pie/gauge wedge from angle `a0` to `a1` (radians) as `pos2` triangles for
 * `triSolid@1`. `aspect` scales the x-radius (keep round in clip space).
 * `segs` triangles → segs*3 vertices.
 */
export function wedgeTriangles(
  cx: number,
  cy: number,
  r: number,
  a0: number,
  a1: number,
  segs = 32,
  aspect = 1,
): number[] {
  const n = Math.max(2, segs);
  const ax = aspect * r;
  const ay = r;
  const out: number[] = [];
  for (let i = 0; i < n; i++) {
    const t0 = lerp(a0, a1, i / n);
    const t1 = lerp(a0, a1, (i + 1) / n);
    out.push(
      cx,
      cy,
      cx + ax * Math.cos(t0),
      cy + ay * Math.sin(t0),
      cx + ax * Math.cos(t1),
      cy + ay * Math.sin(t1),
    );
  }
  return out;
}

// -------------------- per-vertex colored (pos2_color4) --------------------

/**
 * Flatten `{x,y,rgba}` vertices into the `pos2_color4` layout
 * `[x, y, r, g, b, a, …]` for the `triGradient@1` pipeline. Alpha defaults to 1.
 */
export function triGradientFloats(verts: readonly ColorVertex[]): number[] {
  const out: number[] = [];
  for (const v of verts) {
    out.push(v.x, v.y, v.rgba[0], v.rgba[1], v.rgba[2], v.rgba[3] ?? 1);
  }
  return out;
}

/**
 * Two colored triangles filling `rect` with a linear gradient from `c0` to `c1`
 * along `angle` degrees. Returns 6 {@link ColorVertex} (feed to
 * {@link triGradientFloats}). Matches the corpus `gradientRect`.
 */
export function gradientRectVerts(
  rect: Rect,
  opts: { angle?: number; c0: RgbaLike; c1: RgbaLike },
): ColorVertex[] {
  const ang = ((opts.angle ?? 0) * Math.PI) / 180;
  const dx = Math.cos(ang);
  const dy = Math.sin(ang);
  const { x0, y0, x1, y1 } = rect;
  const corners: [number, number][] = [
    [x0, y0],
    [x1, y0],
    [x1, y1],
    [x0, y1],
  ];
  const proj = corners.map(
    ([x, y]) => ((x - x0) / (x1 - x0 || 1)) * dx + ((y - y0) / (y1 - y0 || 1)) * dy,
  );
  const mn = Math.min(...proj);
  const mx = Math.max(...proj);
  const col = (i: number): Rgba => mixColor(opts.c0, opts.c1, (proj[i] - mn) / (mx - mn || 1));
  return [
    { x: x0, y: y0, rgba: col(0) },
    { x: x1, y: y0, rgba: col(1) },
    { x: x1, y: y1, rgba: col(2) },
    { x: x0, y: y0, rgba: col(0) },
    { x: x1, y: y1, rgba: col(2) },
    { x: x0, y: y1, rgba: col(3) },
  ];
}

/**
 * A radial-gradient disc: `c0` at center → `c1` at the rim, as a colored
 * triangle fan of `segs` wedges. `aspect` scales the x-radius. Matches the
 * corpus `gradientDisc`.
 */
export function gradientDiscVerts(
  cx: number,
  cy: number,
  r: number,
  opts: { c0: RgbaLike; c1: RgbaLike; segs?: number; aspect?: number },
): ColorVertex[] {
  const segs = opts.segs ?? 48;
  const ax = opts.aspect ?? 1;
  const v: ColorVertex[] = [];
  for (let i = 0; i < segs; i++) {
    const a0 = (2 * Math.PI * i) / segs;
    const a1 = (2 * Math.PI * (i + 1)) / segs;
    v.push(
      { x: cx, y: cy, rgba: opts.c0 },
      { x: cx + r * ax * Math.cos(a0), y: cy + r * Math.sin(a0), rgba: opts.c1 },
      { x: cx + r * ax * Math.cos(a1), y: cy + r * Math.sin(a1), rgba: opts.c1 },
    );
  }
  return v;
}

/**
 * Vertical-gradient area under a curve: `cTop` at the curve, `cBot` at the
 * `baseline`, as colored triangles (2 per gap). Matches the corpus `areaGrad`.
 */
export function areaGradVerts(
  pts: readonly Point[],
  baseline: number,
  opts: { cTop: RgbaLike; cBot: RgbaLike },
): ColorVertex[] {
  const cT = opts.cTop;
  const cB = opts.cBot;
  const v: ColorVertex[] = [];
  for (let i = 0; i < pts.length - 1; i++) {
    const p = pts[i];
    const q = pts[i + 1];
    v.push(
      { x: p.x, y: p.y, rgba: cT },
      { x: p.x, y: baseline, rgba: cB },
      { x: q.x, y: q.y, rgba: cT },
      { x: q.x, y: q.y, rgba: cT },
      { x: p.x, y: baseline, rgba: cB },
      { x: q.x, y: baseline, rgba: cB },
    );
  }
  return v;
}

/**
 * Flat-per-rect colored quads (2 tris each) — the `heat`/per-cell fallback that
 * the corpus routes through `triGradient@1` because `instancedRectColor@1` is
 * absent from the WASM build. Each rect carries its own `rgba`.
 */
export function rectsColoredVerts(
  rects: readonly (Rect & { rgba: RgbaLike })[],
): ColorVertex[] {
  const v: ColorVertex[] = [];
  for (const r of rects) {
    const c = r.rgba;
    v.push(
      { x: r.x0, y: r.y0, rgba: c },
      { x: r.x1, y: r.y0, rgba: c },
      { x: r.x1, y: r.y1, rgba: c },
      { x: r.x0, y: r.y0, rgba: c },
      { x: r.x1, y: r.y1, rgba: c },
      { x: r.x0, y: r.y1, rgba: c },
    );
  }
  return v;
}

/** A scatter point: position (data space), pixel `size`, and `rgba`. */
export interface ScatterPoint {
  x: number;
  y: number;
  size?: number;
  rgba: RgbaLike;
}

/**
 * Per-point colored bubbles drawn as CLIP-space triangle fans (so the radius is
 * in pixels and bubbles stay round regardless of x/y data scale) — the corpus
 * `scatter` fallback for the absent `instancedPointColor@1`. Pass the transform
 * PARAMS `t = {sx,tx,sy,ty}` (data→clip) plus viewport `W`,`H`. The returned
 * vertices are in CLIP space, so draw them with NO transform attached.
 */
export function scatterVerts(
  pts: readonly ScatterPoint[],
  t: { sx: number; tx: number; sy: number; ty: number },
  opts: { W?: number; H?: number; segs?: number } = {},
): ColorVertex[] {
  const W = opts.W ?? 960;
  const H = opts.H ?? 600;
  const segs = opts.segs ?? 18;
  const v: ColorVertex[] = [];
  for (const p of pts) {
    const cx = p.x * t.sx + t.tx;
    const cy = p.y * t.sy + t.ty;
    const rx = (p.size ?? 6) / (W / 2);
    const ry = (p.size ?? 6) / (H / 2);
    for (let i = 0; i < segs; i++) {
      const a0 = (2 * Math.PI * i) / segs;
      const a1 = (2 * Math.PI * (i + 1)) / segs;
      v.push(
        { x: cx, y: cy, rgba: p.rgba },
        { x: cx + rx * Math.cos(a0), y: cy + ry * Math.sin(a0), rgba: p.rgba },
        { x: cx + rx * Math.cos(a1), y: cy + ry * Math.sin(a1), rgba: p.rgba },
      );
    }
  }
  return v;
}
