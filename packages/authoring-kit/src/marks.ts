/* packages/authoring-kit/src/marks.ts — ENC-714
 *
 * The high-level "chart vocabulary": each function turns a semantic mark (line,
 * area, candles, bars, heat cells, scatter, pie wedge, gradients, grid) into a
 * framework-agnostic {@link Mark} descriptor — `{ pipeline, format, count,
 * floats, style, space }`. That descriptor is exactly the shape the engine
 * consumes (via dc-wasm's SceneBuilder in customer-layer, or an equivalent
 * recipe emitter in embassy): the caller allocates ids / drives the host; this
 * module owns the geometry + style math so it lives in one tested place.
 *
 * This is the pure, host-free counterpart to the corpus `createChart` marks:
 * same geometry, same pipelines/formats/styles, but returning DATA rather than
 * issuing `applyControl` calls. Scale/tick/transform/id/host-sequencing all come
 * from @repo/dc-wasm (re-exported by this package's index) — not duplicated here.
 */

import { horizontalGridSegments, verticalGridSegments, type Range } from "@repo/dc-wasm/chart";
import {
  polylineSegments,
  areaTriangles,
  triangulateFan,
  circleRing,
  wedgeTriangles,
  triGradientFloats,
  gradientRectVerts,
  gradientDiscVerts,
  areaGradVerts,
  rectsColoredVerts,
  scatterVerts,
  type Point,
  type Rect,
  type ColorVertex,
  type ScatterPoint,
} from "./geometry";
import type { RgbaLike } from "./color";

/**
 * A framework-agnostic draw descriptor. `count` is the engine element/vertex
 * count for `format` (segments for rect4 lines, instances for candle6/rect4
 * marks, vertices for pos2/pos2_color4 triangles). `space: "clip"` marks author
 * directly in clip space and must be drawn with NO transform attached.
 */
export interface Mark {
  pipeline: string;
  format: string;
  count: number;
  /** flat vertex floats in the pipeline's layout. */
  floats: number[];
  /** engine style command fields (colors, widths, dash, corner radius, …). */
  style?: Record<string, unknown>;
  /** "data" (draw under a transform) or "clip" (draw with no transform). */
  space: "data" | "clip";
}

/** RGB(A) color as a float array (alpha optional). */
type ColorArr = RgbaLike;

function solidStyle(color: ColorArr | undefined, dflt: [number, number, number, number]): {
  r: number;
  g: number;
  b: number;
  a: number;
} {
  return {
    r: color?.[0] ?? dflt[0],
    g: color?.[1] ?? dflt[1],
    b: color?.[2] ?? dflt[2],
    a: color?.[3] ?? dflt[3],
  };
}

// -------------------- OHLC candles --------------------
/** One OHLC row (candle6 layout: x, open, high, low, close, halfWidth). */
export interface CandleRow {
  x: number;
  open: number;
  high: number;
  low: number;
  close: number;
}

/**
 * Candlesticks (`instancedCandle@1`, candle6). `halfWidth` is in data units;
 * `colorUp`/`colorDown` default to the corpus green/red.
 */
export function candlesMark(
  rows: readonly CandleRow[],
  o: {
    halfWidth?: number;
    colorUp?: [number, number, number, number];
    colorDown?: [number, number, number, number];
  } = {},
): Mark {
  const hw = o.halfWidth ?? 0.32;
  const floats: number[] = [];
  for (const r of rows) floats.push(r.x, r.open, r.high, r.low, r.close, hw);
  return {
    pipeline: "instancedCandle@1",
    format: "candle6",
    count: rows.length,
    floats,
    style: {
      colorUp: o.colorUp ?? [0.16, 0.78, 0.45, 1],
      colorDown: o.colorDown ?? [0.92, 0.3, 0.33, 1],
    },
    space: "data",
  };
}

// -------------------- line / area --------------------
/** Connected anti-aliased polyline (`lineAA@1`, rect4 segments). */
export function lineMark(
  pts: readonly Point[],
  o: { color?: ColorArr; width?: number; dash?: [number, number] } = {},
): Mark {
  const st: Record<string, unknown> = {
    ...solidStyle(o.color, [0.4, 0.7, 1, 1]),
    lineWidth: o.width ?? 2,
  };
  if (o.dash) {
    st.dashLength = o.dash[0];
    st.gapLength = o.dash[1];
  }
  return {
    pipeline: "lineAA@1",
    format: "rect4",
    count: Math.max(0, pts.length - 1),
    floats: polylineSegments(pts),
    style: st,
    space: "data",
  };
}

/** Filled area under a curve to `baseline` (`triSolid@1`, pos2). */
export function areaMark(
  pts: readonly Point[],
  baseline: number,
  o: { color?: ColorArr } = {},
): Mark {
  return {
    pipeline: "triSolid@1",
    format: "pos2_clip",
    count: Math.max(0, pts.length - 1) * 6,
    floats: areaTriangles(pts, baseline),
    style: solidStyle(o.color, [0.3, 0.6, 1, 0.5]),
    space: "data",
  };
}

// -------------------- bars / heat --------------------
/** Single-color rectangles (`instancedRect@1`, rect4) — volume bars, bands. */
export function rectsMark(
  rects: readonly Rect[],
  o: { color?: ColorArr; cornerRadius?: number; blendMode?: string } = {},
): Mark {
  const st: Record<string, unknown> = solidStyle(o.color, [0.4, 0.6, 1, 1]);
  if (o.cornerRadius != null) st.cornerRadius = o.cornerRadius;
  if (o.blendMode != null) st.blendMode = o.blendMode;
  const floats: number[] = [];
  for (const r of rects) floats.push(r.x0, r.y0, r.x1, r.y1);
  return {
    pipeline: "instancedRect@1",
    format: "rect4",
    count: rects.length,
    floats,
    style: st,
    space: "data",
  };
}

/**
 * Per-cell colored rects ("heat") via `triGradient@1` (the corpus fallback for
 * the absent `instancedRectColor@1`): 2 tris/rect, flat per-rect color.
 */
export function heatMark(rects: readonly (Rect & { rgba: RgbaLike })[]): Mark {
  return triGradientMark(rectsColoredVerts(rects));
}

// -------------------- points --------------------
/** Simple points (`points@1`, pos2). */
export function dotsMark(
  pts: readonly Point[],
  o: { color?: ColorArr; size?: number } = {},
): Mark {
  const floats: number[] = [];
  for (const p of pts) floats.push(p.x, p.y);
  return {
    pipeline: "points@1",
    format: "pos2_clip",
    count: pts.length,
    floats,
    style: { ...solidStyle(o.color, [1, 1, 1, 1]), pointSize: o.size ?? 6 },
    space: "data",
  };
}

/**
 * Per-point colored+sized scatter bubbles, authored in CLIP space (round
 * regardless of data scale). Draw with NO transform. Pass the data→clip
 * transform params `t` and viewport `W`/`H`.
 */
export function scatterMark(
  pts: readonly ScatterPoint[],
  t: { sx: number; tx: number; sy: number; ty: number },
  o: { W?: number; H?: number; segs?: number } = {},
): Mark {
  return { ...triGradientMark(scatterVerts(pts, t, o)), space: "clip" };
}

// -------------------- polygons / circles / pie --------------------
/** Filled convex polygon via triangle-fan from `center` (`triSolid@1`, pos2). */
export function polygonMark(
  center: Point,
  ring: readonly Point[],
  o: { color?: ColorArr; blendMode?: string } = {},
): Mark {
  const st: Record<string, unknown> = solidStyle(o.color, [0.5, 0.5, 0.9, 1]);
  if (o.blendMode != null) st.blendMode = o.blendMode;
  return {
    pipeline: "triSolid@1",
    format: "pos2_clip",
    count: ring.length * 3,
    floats: triangulateFan(center, ring),
    style: st,
    space: "data",
  };
}

/** Filled circle/ellipse. `aspect` scales the x-radius (round in clip space). */
export function circleMark(
  cx: number,
  cy: number,
  r: number,
  o: { segs?: number; aspect?: number; color?: ColorArr; blendMode?: string } = {},
): Mark {
  const ring = circleRing(cx, cy, r, o.segs ?? 48, o.aspect ?? 1);
  return polygonMark({ x: cx, y: cy }, ring, o);
}

/** Pie/gauge wedge from `a0`..`a1` radians (`triSolid@1`, pos2). */
export function wedgeMark(
  cx: number,
  cy: number,
  r: number,
  a0: number,
  a1: number,
  o: { segs?: number; aspect?: number; color?: ColorArr; blendMode?: string } = {},
): Mark {
  const segs = Math.max(2, o.segs ?? 32);
  const st: Record<string, unknown> = solidStyle(o.color, [0.5, 0.5, 0.9, 1]);
  if (o.blendMode != null) st.blendMode = o.blendMode;
  return {
    pipeline: "triSolid@1",
    format: "pos2_clip",
    count: segs * 3,
    floats: wedgeTriangles(cx, cy, r, a0, a1, segs, o.aspect ?? 1),
    style: st,
    space: "data",
  };
}

// -------------------- gradients (pos2_color4) --------------------
/** Per-vertex gradient triangles (`triGradient@1`, pos2_color4). */
export function triGradientMark(
  verts: readonly ColorVertex[],
  o: { style?: Record<string, unknown> } = {},
): Mark {
  return {
    pipeline: "triGradient@1",
    format: "pos2_color4",
    count: verts.length,
    floats: triGradientFloats(verts),
    style: o.style,
    space: "data",
  };
}

/** Linear-gradient rect over `rect` from `c0`→`c1` at `angle` degrees. */
export function gradientRectMark(
  rect: Rect,
  o: { angle?: number; c0: RgbaLike; c1: RgbaLike; style?: Record<string, unknown> },
): Mark {
  return triGradientMark(gradientRectVerts(rect, o), { style: o.style });
}

/** Radial-gradient disc: `c0` at center → `c1` at rim. */
export function gradientDiscMark(
  cx: number,
  cy: number,
  r: number,
  o: {
    c0: RgbaLike;
    c1: RgbaLike;
    segs?: number;
    aspect?: number;
    style?: Record<string, unknown>;
  },
): Mark {
  return triGradientMark(gradientDiscVerts(cx, cy, r, o), { style: o.style });
}

/** Vertical-gradient area under a curve (`cTop` at curve, `cBot` at baseline). */
export function areaGradMark(
  pts: readonly Point[],
  baseline: number,
  o: { cTop: RgbaLike; cBot: RgbaLike; style?: Record<string, unknown> },
): Mark {
  return triGradientMark(areaGradVerts(pts, baseline, o), { style: o.style });
}

// -------------------- grid --------------------
/**
 * Grid lines: horizontal at `ys`, vertical at `xs`, spanning the given data
 * ranges (`lineAA@1`, rect4). Uses dc-wasm's grid-segment helpers so the
 * segment algebra lives in one place.
 */
export function gridMark(o: {
  xs?: number[];
  ys?: number[];
  xRange: Range;
  yRange: Range;
  color?: ColorArr;
  width?: number;
  dash?: [number, number];
}): Mark {
  const horizontal = horizontalGridSegments(o.ys ?? [], o.xRange);
  const vertical = verticalGridSegments(o.xs ?? [], o.yRange);
  const floats = horizontal.concat(vertical);
  const st: Record<string, unknown> = {
    ...solidStyle(o.color, [0.18, 0.2, 0.26, 0.8]),
    lineWidth: o.width ?? 1,
  };
  if (o.dash) {
    st.dashLength = o.dash[0];
    st.gapLength = o.dash[1];
  }
  return {
    pipeline: "lineAA@1",
    format: "rect4",
    count: floats.length / 4,
    floats,
    style: st,
    space: "data",
  };
}
