/* packages/authoring-kit/src/index.ts — ENC-714
 *
 * @repo/authoring-kit — the framework-agnostic "chart vocabulary" over the
 * DynaCharting engine's raw draw-command surface. Pure ESM, no DOM/WebGPU/host
 * deps: turns semantic marks (line/area/candles/bars/heat/scatter/pie/gradients/
 * grid) into the flat vertex buffers, colors, and {@link Mark} descriptors the
 * engine consumes. Consumable by customer-layer (TS/React, feed a Mark's
 * `floats`/`style` into dc-wasm's SceneBuilder) and, conceptually, by embassy
 * recipes (Go) which produce the same OUTPUT shapes.
 *
 * ANTI-DUPLICATION: the scale/tick/transform/grid math (ENC-699 G4), the id
 * allocator (ENC-700), and the host command-sequencer SceneBuilder (ENC-703 G2)
 * already live — pure and tested — in @repo/dc-wasm. This package DEPENDS on
 * them and RE-EXPORTS them below so authors have a single import surface, rather
 * than reimplementing them.
 */

// ---- net-new authoring vocabulary (this package) ----
export { lerp, clamp, extent, rng } from "./math";
export {
  rgba,
  hsl,
  toByte,
  packColorBytes,
  mixColor,
} from "./color";
export type { Rgba, RgbaLike } from "./color";
export { f32, packMixed } from "./packing";
export type { MixedRecord } from "./packing";
export {
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
} from "./geometry";
export type { Point, ColorVertex, Rect, ScatterPoint } from "./geometry";
export {
  candlesMark,
  lineMark,
  areaMark,
  rectsMark,
  heatMark,
  dotsMark,
  scatterMark,
  polygonMark,
  circleMark,
  wedgeMark,
  triGradientMark,
  gradientRectMark,
  gradientDiscMark,
  areaGradMark,
  gridMark,
} from "./marks";
export type { Mark, CandleRow } from "./marks";

// ---- re-exported shared primitives (canonical home: @repo/dc-wasm) ----
// Scale/tick/transform/grid math (G4), the unified id allocator, and the
// host-driving SceneBuilder. Re-exported so authoring-kit is a one-stop import.
export {
  scale,
  niceTicks,
  indexTicks,
  fitAxis,
  fitTransform,
  horizontalGridSegments,
  verticalGridSegments,
  gridSegments,
  padRange,
  CLIP_RANGE,
  createIdAllocator,
  SceneBuilder,
  encodeAppendRecord,
} from "@repo/dc-wasm/chart";
export type {
  Range,
  Transform1D,
  Transform2D,
  Scale,
  IdAllocator,
  ResourceKind,
  SceneTarget,
  PaneHandle,
  LayerHandle,
  TransformHandle,
  DrawHandle,
} from "@repo/dc-wasm/chart";
