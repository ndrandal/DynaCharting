/* packages/dc-wasm/src/chart/index.ts — ENC-714
 *
 * Pure, DOM-free barrel over dc-wasm's client-side chart helpers (scale/tick/
 * transform/grid math — G4/ENC-699, the unified id allocator — ENC-700, and the
 * SceneBuilder command sequencer — G2/ENC-703). These modules touch no DOM and
 * no WASM/WebGPU; this barrel exposes them as a framework-agnostic subpath
 * (`@repo/dc-wasm/chart`) so consumers that only want the authoring vocabulary
 * (e.g. @repo/authoring-kit) can import them WITHOUT pulling in EngineHost / the
 * WASM loader / thumbnail (which reference DOM globals) via the package root.
 *
 * The package root (`@repo/dc-wasm`) still re-exports all of these for existing
 * consumers (customer-layer) — this subpath is purely additive.
 */

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
} from "./scale";
export type { Range, Transform1D, Transform2D, Scale } from "./scale";

export { createIdAllocator } from "./ids";
export type { IdAllocator, ResourceKind } from "./ids";

export { SceneBuilder, encodeAppendRecord } from "./SceneBuilder";
export type {
  SceneTarget,
  PaneHandle,
  LayerHandle,
  TransformHandle,
  DrawHandle,
  PaneOptions,
  LayerOptions,
  TransformParams,
  Rgba,
  LineStyle,
  RectStyle,
  CandleStyle,
  Candle,
  Rect,
} from "./SceneBuilder";
