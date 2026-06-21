/* packages/dc-wasm/src/index.ts — ENC-506 (P6.5)
 *
 * Public surface of @repo/dc-wasm. MIRRORS @repo/engine-host's index.ts exactly
 * (the EngineHost class + the same types + PIPELINES) so customer-layer (ENC-507)
 * can swap the import path with no other change. The extra dc-wasm-only exports
 * (EngineHostOptions, pickAsync via the class, the WASM loader) are additive and
 * do not conflict with the engine-host surface.
 */

export { EngineHost, TextureFormat } from "./EngineHost";
export type {
  EngineHostHudSink,
  EngineStats,
  PickResult,
  TransformParams,
  EngineHostOptions,
  ControlRejection,
} from "./EngineHost";

export { PIPELINES } from "./pipelines";
export type {
  PipelineId,
  PipelineSpec,
  AttrSpec,
  AttrType,
  UniformType,
} from "./pipelines";

// Client-builder helpers (ENC-700): id allocator over the unified namespace.
export { createIdAllocator } from "./chart/ids";
export type { IdAllocator, ResourceKind } from "./chart/ids";

// ENC-699 (G4): framework-agnostic chart scale/axis/tick/grid math (pure fns).
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
} from "./chart/scale";
export type { Range, Transform1D, Transform2D, Scale } from "./chart/scale";

// ENC-703 (G2): client-side scene/chart builder over applyControl. Encapsulates
// the per-element create→bind→attach→style→data command sequence + id mgmt.
export { SceneBuilder, encodeAppendRecord } from "./chart/SceneBuilder";
export type {
  SceneTarget,
  PaneHandle,
  LayerHandle,
  TransformHandle,
  DrawHandle,
  PaneOptions,
  LayerOptions,
  TransformParams as SceneTransformParams,
  Rgba,
  LineStyle,
  RectStyle,
  CandleStyle,
  Candle,
  Rect,
} from "./chart/SceneBuilder";

// dc-wasm-specific: the WASM loader + module types (for advanced/test wiring).
export { loadDcEngineHost } from "./wasm";
export type {
  DcEngineHostModule,
  DcEngineHostInstance,
  DcEngineHostFactory,
  DcControlResult,
  DcEngineStatsRaw,
} from "./wasm";
