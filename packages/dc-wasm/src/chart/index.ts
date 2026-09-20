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
  composeTransform,
  horizontalGridSegments,
  verticalGridSegments,
  gridSegments,
  padRange,
  CLIP_RANGE,
} from "./scale";
export type { Range, Transform1D, Transform2D, Scale } from "./scale";

// ENC-1252 (chart-quality-bar D7): the visible domain, MEASURED from the streamed
// dataplane records rather than typed into a view file. The client-side mirror of
// embassy's RangeTracker.
export {
  DomainTracker,
  applyDomainPolicy,
  RECORD_LAYOUTS,
} from "./domain";
export type {
  RecordLayout,
  DomainSource,
  DomainPolicy,
  ObservedDomain,
} from "./domain";

// ENC-1254 (chart-quality-bar D1 tier 1): TIME RENDERS AS TIME. The step
// ladder, the label grammar, D1's `parsesAsTimestamp` predicate, the D12-derived
// decimal rule, and the measured recordIndex→instant basis. Pure and DOM-free,
// so the engine-side axis (ENC-1253) reuses the same formatting the DOM overlay
// uses today rather than reimplementing it.
export {
  TIME_STEPS,
  chooseTimeStep,
  timeTicks,
  timeLabelStyle,
  formatTimeTick,
  parsesAsTimestamp,
  decimalsForStep,
  decimalsForTicks,
  MAX_DERIVED_DECIMALS,
  indexToTime,
  timeToIndex,
  timeDomainFor,
  IndexTimeTracker,
  timeBasisFromWire,
  transmittedBasisFromSceneInit,
} from "./time";
export type {
  TimeUnit,
  TimeStep,
  TimeZoneMode,
  TimeTick,
  TimeTickSet,
  TimeTicksOptions,
  TimeLabelStyle,
  TimeBasis,
  TimeBasisSource,
  IndexTimeSource,
  WireTimeBasis,
} from "./time";

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

// ENC-1256 (chart-quality-bar D1 tier 2): THE PLOT BOX — the clip-space
// rectangle the measured domain is fitted into, and the gutters reserved for
// the axis furniture ENC-1253 draws. The output side of ENC-1252's domain.
export {
  DEFAULT_PLOT_INSETS,
  FULL_CLIP_REGION,
  TIER2_FRAMING_BOUNDS,
  PlotBoxError,
  plotBox,
  tryPlotBox,
  gutters,
  paneRegionFor,
  pxSpanToClipX,
  pxSpanToClipY,
  clipSpanToPxX,
  clipSpanToPxY,
  fitToPlotBox,
  framingMetrics,
  checkTier2Framing,
  frameSeries,
  fitRegionToBox,
} from "./plotbox";
export type {
  CanvasSize,
  PlotInsets,
  PlotBox,
  PlotBoxRefusal,
  PlotBoxResolution,
  PlotGutters,
  PaneRegion,
  FitPolicy,
  FramingMetrics,
  Tier2Verdict,
  FramedSeries,
} from "./plotbox";

// ENC-1253 (chart-quality-bar D7, D1 tier 1, §1.3): THE ENGINE DRAWS ITS OWN
// AXIS. Gridlines, tick marks and the spine as `lineAA@1` clip-space geometry,
// tick labels as `textSDF@1` glyph runs, all in the SAME raster as the data —
// not an HTML/SVG overlay. `planAxis` is pure; `EngineAxis` drives a live host;
// `checkTier1Labels` is D1's tier-1 label row without a raster; and
// `axisSceneFragment` dumps what was drawn in the tier scorer's vocabulary.
export {
  AXIS_DEFAULTS,
  EngineAxis,
  axisSceneFragment,
  boxesOverlap,
  checkTier1Labels,
  createHostMeasurer,
  encodeUpdateRecord,
  fontSizeForPx,
  labelBoxPx,
  planAxis,
  textXScale,
} from "./axis";
export type {
  AxisTick,
  AxisSide,
  AxisSideSpec,
  AxisSpec,
  AxisLabel,
  AxisGridLine,
  AxisPlan,
  AxisTarget,
  AxisTextMeasurer,
  AxisInkRole,
  AxisSceneFragment,
  TextMetrics,
  Tier1LabelVerdict,
} from "./axis";

// ENC-1253 (SPEC D1 tier 3, D4): the axis furniture's colours, TRANSCRIBED from
// `dc::Theme` rather than invented — `theme.test.ts` parses the C++ and fails if
// the two drift. Also D11's contrast bands, as a function instead of as prose.
export {
  AXIS_THEMES,
  CANVAS_CLEAR_BLACK,
  D11_BANDS,
  axisThemeByName,
  bloombergAxisTheme,
  checkD11AxisBands,
  contrastRatio,
  darkAxisTheme,
  defaultAxisTheme,
  gridRgba,
  lightAxisTheme,
  midnightAxisTheme,
  neonAxisTheme,
  pastelAxisTheme,
  relativeLuminance,
  rgba,
} from "./theme";
export type { AxisTheme, Rgba4, D11Verdict } from "./theme";
