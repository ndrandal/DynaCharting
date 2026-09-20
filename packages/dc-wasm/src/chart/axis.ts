/* packages/dc-wasm/src/chart/axis.ts — ENC-1253 (chart-quality-bar SPEC D7, D1 tier 1, §1.3)
 *
 * THE AXIS, DRAWN BY THE ENGINE.
 *
 * Until this module the axis in every chart this repo has ever shipped was an
 * HTML/SVG overlay composited on top of the WebGPU canvas
 * (`apps/showcase/src/chrome/AxisOverlay.tsx`), and the product surface has no
 * overlay at all — so the product had no axis whatsoever. Measured, not
 * inferred: the engine's own canvas raster for the reference chart contains
 * exactly THREE distinct colours at 1800x1200 (background plus the two candle
 * colours), i.e. there is no ink anywhere that could be a gridline, a tick, a
 * spine or a label. `specs/2026-09-19-chart-quality-bar/harness/scenes/
 * live-nexo-candles.scene.json`, and SPEC §1.0 / §1.3.
 *
 * This module emits the furniture as ENGINE GEOMETRY: `lineAA@1` rect4 segments
 * for the gridlines, tick marks and spine, and `textSDF@1` glyph runs for the
 * labels, all into the same scene, the same canvas and the same raster as the
 * data marks (SPEC D6/D10).
 *
 * ── WHAT IT IS NOT ──────────────────────────────────────────────────────────
 *
 * It does not choose ticks (ENC-1254, `time.ts` / `scale.ts` — a time axis ticks
 * on the calendar ladder, a value axis on `niceTicks`), measure the domain
 * (ENC-1252, `domain.ts`), or decide the frame (ENC-1256, `plotbox.ts`). It
 * takes all three as inputs and turns them into marks. Reimplementing any of
 * them here is how the label and the mark end up disagreeing.
 *
 * ── FIVE THINGS THAT ARE LOAD-BEARING ───────────────────────────────────────
 *
 * 1. FURNITURE IS AUTHORED DIRECTLY IN CLIP SPACE, AND CARRIES NO TRANSFORM.
 *    The tick VALUES are in data space; this module projects them through the
 *    SAME `Transform2D` the data marks carry and emits clip-space geometry. The
 *    draw items then have no `attachTransform`. Attaching the data transform
 *    instead would be shorter and wrong: a gridline is a full-width line at one
 *    data value, not a data-space rectangle, and the tick marks and labels live
 *    in the gutter where the data transform does not reach.
 *
 * 2. THE FURNITURE PANE MUST BE WIDER THAN THE PLOT BOX. `plotbox.ts` contract
 *    note (7): the data pane's `PaneRegion` IS the plot box, applied as a
 *    scissor. Ticks and labels are drawn in the GUTTERS, outside that box, so
 *    they need their own pane at `FULL_CLIP_REGION` — put them in the data pane
 *    and the scissor eats them silently, with no rejection and no warning.
 *
 * 3. `pxSpanToClipX/Y` (plotbox.ts) ARE SPANS; `pxToClipX/Y` (text.ts) ARE
 *    POSITIONS. This file imports both, which is precisely the collision
 *    plotbox.ts warned about. A 6px tick LENGTH is a span; the left edge of a
 *    label's bounding box is a position. Swapping them is off by exactly -1 on
 *    X and mirrored on Y, which renders as "the axis is in the wrong half of
 *    the chart" rather than as an error.
 *
 * 4. LABELS ARE MEASURED, NOT ESTIMATED. `AxisTextMeasurer` is backed by the
 *    core's own `measureText` (ENC-1253's binding), which runs the SAME
 *    `dc::layoutText` loop `setTextGeometry` runs. So a label's reported
 *    bounding box is where its glyphs will actually land — which is what makes
 *    right-aligning a price, centring a time, and D1's tier-1 "label boxes are
 *    disjoint and inside the frame" assertion checkable rather than hopeful.
 *    Estimating a width from the glyph count gives a chart whose labels overlap
 *    for some strings and not others.
 *
 * 5. COLLIDING LABELS ARE DROPPED, NOT DRAWN. When two ticks are closer than
 *    their labels are wide, this emits the tick MARK for both and the LABEL for
 *    one. D1 tier 1 forbids overlapping labels; it does not require every tick
 *    to be labelled. `AxisPlan.droppedLabels` reports how many and why, so a
 *    sparse axis is a stated measurement rather than a silent thinning — the
 *    same discipline `AxisOverlay` applies to off-frame ticks (DC-L13 §2).
 */

import {
  CLIP_RANGE,
  type Range,
  type Transform2D,
} from "./scale";
import {
  FULL_CLIP_REGION,
  gutters,
  pxSpanToClipX,
  pxSpanToClipY,
  type CanvasSize,
  type PlotBox,
} from "./plotbox";
import { clipXToPx, clipYToPx, type TextTarget } from "./text";
import { createIdAllocator, type IdAllocator } from "./ids";
import { encodeAppendRecord, type Rgba } from "./SceneBuilder";
import { defaultAxisTheme, gridRgba, rgba, type AxisTheme } from "./theme";
import { parsesAsTimestamp } from "./time";

// ── Inputs ──────────────────────────────────────────────────────────────────

/** One tick, in DATA space — the shape `axisTicks`/`timeTicks` already emit. */
export interface AxisTick {
  /** Position in data space (a price, or a record index on a time axis). */
  value: number;
  /** The label to draw. An empty string draws a tick with no label. */
  label: string;
}

/** Which gutter an axis lives in, and therefore how its labels are aligned. */
export type AxisSide = "left" | "bottom";

/** One axis's marks and how much room they get. */
export interface AxisSideSpec {
  /** The ticks, in data space. Ticks outside the plot box are dropped. */
  ticks: readonly AxisTick[];
  /** Draw a gridline across the plot box at each tick. Default true. */
  grid?: boolean;
  /** Tick-mark length in CSS pixels, measured outward from the box. Default 6. */
  tickLengthPx?: number;
  /** Gap in CSS pixels between the tick's outer end and the label. Default 4. */
  labelGapPx?: number;
  /** Label size in CSS pixels (ascent-to-descent). Default 11. */
  fontPx?: number;
  /** Axis title, drawn once in the gutter's far corner. Optional. */
  title?: string;
}

/**
 * A measured text run, in CSS pixels relative to the layout origin
 * (baseline-left). `+x` is right and `+y` is UP, matching clip space rather than
 * screen space — the conversion to a top-down raster box happens once, in
 * `labelBoxPx`.
 */
export interface TextMetrics {
  /** Cursor advance for the whole string — the width to align against. */
  advanceWidthPx: number;
  /** Ink bounds, relative to the baseline-left origin. */
  inkMinXPx: number;
  inkMaxXPx: number;
  inkMinYPx: number;
  inkMaxYPx: number;
  /** Glyphs the layout produced (whitespace advances but emits none). */
  glyphCount: number;
}

/**
 * Measures a string as the engine will lay it out. `createHostMeasurer` wraps
 * the real core; a test can pass a deterministic stub.
 */
export interface AxisTextMeasurer {
  measure(text: string, fontPx: number): TextMetrics;
}

/** Everything needed to plan one chart's axis furniture. */
export interface AxisSpec {
  /** The plot box the data was fitted into (`frameSeries().box`). */
  box: PlotBox;
  /** Canvas size in CSS pixels — the render target the clip box maps onto. */
  canvas: CanvasSize;
  /** The data→clip transform the DATA MARKS carry. Ticks project through it. */
  transform: Transform2D;
  /** The value axis, drawn in the left gutter. Omit for no y axis. */
  y?: AxisSideSpec;
  /** The time/index axis, drawn in the bottom gutter. Omit for no x axis. */
  x?: AxisSideSpec;
  /** Colours and line widths. Defaults to the mirrored `midnightTheme()`. */
  theme?: AxisTheme;
  /** Draw the left + bottom spines along the plot box edges. Default true. */
  spine?: boolean;
  /** Measures labels. Omit to plan geometry only and emit no labels. */
  measurer?: AxisTextMeasurer;
  /**
   * Where the GRIDLINES are drawn, when they must go BEHIND the data (ENC-1316).
   *
   * By default every piece of furniture lives in this axis's own pane, which is
   * created after the chart's and therefore renders ON TOP of it — panes render
   * in id order (`Scene::paneIds()` sorts ascending) and so do layers. For the
   * spine, the ticks and the labels that is exactly right: they live in the
   * gutters, outside the data pane's scissor, and nothing may cover them.
   *
   * For a gridline it is exactly wrong. A gridline's whole job is to recede
   * behind the marks, which is also what D11's band-3 CEILING measures — and
   * `score.py`'s `line_sample` says so in as many words: *"A gridline runs
   * behind the marks, so wherever a candle covers it both samples are the same
   * candle pixel"*. Drawn on top instead, a 1px `(51,51,64)` line across a cyan
   * waveform is not a gridline at all, it is a mark cutting the data: measured
   * at 6.25 : 1 against a 2.0 : 1 ceiling (ENC-1262, `audio-waveform`).
   *
   * So a caller that has framed its data — and therefore knows the pane whose
   * region IS the plot box — passes that pane here together with a layer id
   * BELOW every layer the chart's own manifest creates. The gridlines are then
   * issued into the data pane, after its clear quad and before its data, and
   * the scissor that would eat a tick in the gutter is harmless to a line that
   * spans exactly the box.
   *
   * Omit it and the grid stays in the furniture pane, unchanged.
   */
  gridTarget?: AxisGridTarget;
}

/**
 * The pane and layer a caller wants the GRIDLINES drawn into — see
 * `AxisSpec.gridTarget`. Both ids belong to the caller: `EngineAxis` creates the
 * layer (and deletes it on `dispose`), and never touches the pane.
 */
export interface AxisGridTarget {
  /** The data pane — the one whose `PaneRegion` is the plot box. */
  paneId: number;
  /** A layer id LOWER than every layer the chart's manifest creates. */
  layerId: number;
}

// ── Outputs ─────────────────────────────────────────────────────────────────

/** A placed label: where the engine is told to draw it, and where it lands. */
export interface AxisLabel {
  /** Which axis it belongs to. */
  axis: "x" | "y";
  /** `xTickLabel` / `yTickLabel` / `axisTitle` — the scorer's role vocabulary. */
  role: "xTickLabel" | "yTickLabel" | "axisTitle";
  /** The string. */
  text: string;
  /** Clip-space baseline origin handed to `setTextGeometry`. */
  clipX: number;
  clipY: number;
  /** Label size in CSS pixels, and the clip `fontSize` that produces it. */
  fontPx: number;
  fontSizeClip: number;
  /** Horizontal scale handed to `setTextGeometryX` (`height/width`). */
  xScale: number;
  /** The ink's bounding box in RASTER pixels: `[x, y, w, h]`, y top-down. */
  bbox: [number, number, number, number];
}

/** A gridline, as the tier-1 scorer wants it declared. */
export interface AxisGridLine {
  orientation: "horizontal" | "vertical";
  /** The raster row (horizontal) or column (vertical) the line's centre is on. */
  centre: number;
  /** The data value it marks. */
  value: number;
}

/** The complete furniture plan. Pure — nothing here has touched an engine. */
export interface AxisPlan {
  /** Gridline segments, flat rect4 `[x0,y0,x1,y1, …]` in CLIP space. */
  gridSegments: number[];
  /** Tick-mark segments, flat rect4 in CLIP space. */
  tickSegments: number[];
  /** Spine segments, flat rect4 in CLIP space. */
  spineSegments: number[];
  /** Labels that will be drawn, already collision-pruned. */
  labels: AxisLabel[];
  /** Gridlines, in the raster coordinates the scorer samples. */
  gridLines: AxisGridLine[];
  /** Labels that were planned and then dropped, with the reason. */
  droppedLabels: { text: string; axis: "x" | "y"; reason: string }[];
  /** The plot box in RASTER pixels: `[x0, y0, x1, y1]`, y top-down, inclusive. */
  plotPx: [number, number, number, number];
}

// ── Defaults ────────────────────────────────────────────────────────────────

/**
 * Furniture sizes in CSS pixels. These are the numbers `DEFAULT_PLOT_INSETS`
 * was sized for (plotbox.ts: "a price label at 11px, 7 glyphs, plus a 6px tick
 * and 4px of breathing room"), so the two files agree by construction rather
 * than by coincidence. Change one and `axis.test.ts` fails on the other.
 */
export const AXIS_DEFAULTS = {
  tickLengthPx: 6,
  labelGapPx: 4,
  fontPx: 11,
} as const;

// ── Pure helpers ────────────────────────────────────────────────────────────

/**
 * The clip `fontSize` that renders text `px` CSS pixels tall on `canvas`.
 *
 * `setTextGeometry`'s `fontSize` is the ascent-to-descent height in CLIP units
 * (the atlas rasterizes the em box at `glyphPx`, and every metric is scaled by
 * `fontSize / glyphPx`), and clip Y spans 2 over the target's height.
 */
export function fontSizeForPx(px: number, canvas: CanvasSize): number {
  return (2 * px) / canvas.height;
}

/**
 * The horizontal scale that undoes clip space's aspect distortion.
 *
 * One clip unit is `width/2` pixels across and `height/2` pixels down, so a
 * layout that scales X and Y identically renders every string stretched by
 * `width/height` — 1.5x on a 1800x1200 target. Passing this to
 * `setTextGeometryX` / `measureText` gives glyphs the font's own proportions.
 */
export function textXScale(canvas: CanvasSize): number {
  return canvas.height / canvas.width;
}

/** Project a data value through `t` onto clip X. */
function toClipX(value: number, t: Transform2D): number {
  return value * t.sx + t.tx;
}

/** Project a data value through `t` onto clip Y. */
function toClipY(value: number, t: Transform2D): number {
  return value * t.sy + t.ty;
}

/** Inclusive-with-epsilon containment, so a tick exactly on the box is kept. */
function within(v: number, r: Range, eps = 1e-9): boolean {
  return Number.isFinite(v) && v >= Math.min(r.min, r.max) - eps && v <= Math.max(r.min, r.max) + eps;
}

/**
 * How close to a box edge a gridline may be drawn, in CSS pixels (ENC-1316).
 *
 * A gridline AT an edge is the frame drawn a second time, and it measures as
 * one: on `ohlc-bars` — a FITTED view, so its first x tick lands exactly on the
 * domain minimum, which is exactly `box.x.min` — the leftmost vertical gridline
 * is drawn on top of the y spine, the two 1px lines composite to `(102,102,115)`
 * where each alone is `(51,51,64)`, and D11 band 3 reads **3.58 : 1** against a
 * 2.0 : 1 ceiling. Nothing is wrong with either line; there are just two of
 * them in one place.
 *
 * The same rule covers the far edges, where the data pane's scissor cuts the
 * line in half and halves its per-channel delta with it. One pixel of tolerance
 * is the width of the lines involved (`gridLineWidth` and `tickLineWidth` are
 * both 1) and absorbs the rounding in `clipXToPx`; it is deliberately NOT wide
 * enough to drop a tick that merely lands NEAR the spine, which is a different
 * (and real) legibility problem with a different fix.
 *
 * The tick MARK and the LABEL are unaffected — they are drawn in the gutter and
 * are the axis's statement about the value. Only the redundant line is dropped.
 */
export const GRID_EDGE_TOLERANCE_PX = 1;

/** Is `v` within `tol` of either end of `r`? (`r` may be inverted.) */
function onEdge(v: number, r: Range, tol: number): boolean {
  return Math.abs(v - r.min) <= tol || Math.abs(v - r.max) <= tol;
}

/** Do two `[x,y,w,h]` boxes overlap? Abutting does not count (matches score.py). */
export function boxesOverlap(
  a: readonly [number, number, number, number],
  b: readonly [number, number, number, number],
): boolean {
  return a[0] < b[0] + b[2] && b[0] < a[0] + a[2] && a[1] < b[1] + b[3] && b[1] < a[1] + a[3];
}

/**
 * A measured run at a clip baseline origin → its raster bounding box,
 * `[x, y, w, h]` with y measured DOWN from the top row.
 *
 * Rounding is outward (floor the near edges, ceil the far ones) so the declared
 * box always contains every lit pixel. An inward-rounded box can exclude the
 * antialiased edge of its own glyph, which reads to a scorer as "the label is
 * not in the raster" — a false negative on a label that is plainly there.
 */
export function labelBoxPx(
  clipX: number,
  clipY: number,
  m: TextMetrics,
  canvas: CanvasSize,
): [number, number, number, number] {
  const originXPx = clipXToPx(clipX, canvas.width);
  const originYPx = clipYToPx(clipY, canvas.height);
  const x0 = originXPx + m.inkMinXPx;
  const x1 = originXPx + m.inkMaxXPx;
  // Clip +Y is UP, raster +y is DOWN: the ink's MAX y is its TOP row.
  const y0 = originYPx - m.inkMaxYPx;
  const y1 = originYPx - m.inkMinYPx;
  const left = Math.floor(x0);
  const top = Math.floor(y0);
  return [left, top, Math.max(1, Math.ceil(x1) - left), Math.max(1, Math.ceil(y1) - top)];
}

// ── The planner ─────────────────────────────────────────────────────────────

/**
 * Plan one chart's axis furniture. Pure: no engine, no DOM, no ids allocated.
 *
 * Ticks outside the plot box are dropped (the frame is the truth about what is
 * visible — never widen the frame or synthesise a tick to make a count come
 * out). Labels whose boxes would collide, or would leave the frame, are dropped
 * with a reason; their tick marks are kept.
 */
export function planAxis(spec: AxisSpec): AxisPlan {
  const { box, canvas, transform } = spec;
  const theme = spec.theme ?? defaultAxisTheme;
  const g = gutters(box);
  const gridSegments: number[] = [];
  const tickSegments: number[] = [];
  const spineSegments: number[] = [];
  const gridLines: AxisGridLine[] = [];
  const droppedLabels: AxisPlan["droppedLabels"] = [];
  const placed: AxisLabel[] = [];
  const xScale = textXScale(canvas);
  // A gridline on the frame is the frame — see GRID_EDGE_TOLERANCE_PX.
  const edgeTolX = pxSpanToClipX(GRID_EDGE_TOLERANCE_PX, canvas);
  const edgeTolY = pxSpanToClipY(GRID_EDGE_TOLERANCE_PX, canvas);

  // Furniture whose own colour is used by the caller; the theme is carried
  // through `drawAxis`, not baked into the geometry.
  void theme;

  /** Accept `label` unless it leaves the frame or hits an already-placed box. */
  const tryPlace = (label: AxisLabel): boolean => {
    const [x, y, w, h] = label.bbox;
    if (x < 0 || y < 0 || x + w > canvas.width || y + h > canvas.height) {
      droppedLabels.push({ text: label.text, axis: label.axis, reason: "outside the frame" });
      return false;
    }
    for (const other of placed) {
      if (boxesOverlap(label.bbox, other.bbox)) {
        droppedLabels.push({
          text: label.text,
          axis: label.axis,
          reason: `would overlap ${JSON.stringify(other.text)}`,
        });
        return false;
      }
    }
    placed.push(label);
    return true;
  };

  // ── the value axis: gridlines across the box, ticks + labels in the left gutter
  if (spec.y) {
    const s = spec.y;
    const tickLen = pxSpanToClipX(s.tickLengthPx ?? AXIS_DEFAULTS.tickLengthPx, canvas);
    const gap = pxSpanToClipX(s.labelGapPx ?? AXIS_DEFAULTS.labelGapPx, canvas);
    const fontPx = s.fontPx ?? AXIS_DEFAULTS.fontPx;
    const fontSizeClip = fontSizeForPx(fontPx, canvas);
    const wantGrid = s.grid ?? true;

    for (const t of s.ticks) {
      const clipY = toClipY(t.value, transform);
      if (!within(clipY, box.y)) continue;
      if (wantGrid && !onEdge(clipY, box.y, edgeTolY)) {
        gridSegments.push(box.x.min, clipY, box.x.max, clipY);
        gridLines.push({
          orientation: "horizontal",
          centre: Math.round(clipYToPx(clipY, canvas.height)),
          value: t.value,
        });
      }
      tickSegments.push(box.x.min - tickLen, clipY, box.x.min, clipY);

      if (!t.label || !spec.measurer) continue;
      const m = spec.measurer.measure(t.label, fontPx);
      if (m.glyphCount <= 0) continue;
      // Right-align: the ink's right edge sits `gap` inside the tick's outer end.
      const rightEdge = box.x.min - tickLen - gap;
      const clipX = rightEdge - pxSpanToClipX(m.inkMaxXPx, canvas);
      // Centre the ink vertically on the tick.
      const clipYBaseline =
        clipY - pxSpanToClipY((m.inkMaxYPx + m.inkMinYPx) / 2, canvas);
      tryPlace({
        axis: "y",
        role: "yTickLabel",
        text: t.label,
        clipX,
        clipY: clipYBaseline,
        fontPx,
        fontSizeClip,
        xScale,
        bbox: labelBoxPx(clipX, clipYBaseline, m, canvas),
      });
    }
  }

  // ── the time axis: gridlines down the box, ticks + labels in the bottom gutter
  if (spec.x) {
    const s = spec.x;
    const tickLen = pxSpanToClipY(s.tickLengthPx ?? AXIS_DEFAULTS.tickLengthPx, canvas);
    const gap = pxSpanToClipY(s.labelGapPx ?? AXIS_DEFAULTS.labelGapPx, canvas);
    const fontPx = s.fontPx ?? AXIS_DEFAULTS.fontPx;
    const fontSizeClip = fontSizeForPx(fontPx, canvas);
    const wantGrid = s.grid ?? true;

    for (const t of s.ticks) {
      const clipX = toClipX(t.value, transform);
      if (!within(clipX, box.x)) continue;
      if (wantGrid && !onEdge(clipX, box.x, edgeTolX)) {
        gridSegments.push(clipX, box.y.min, clipX, box.y.max);
        gridLines.push({
          orientation: "vertical",
          centre: Math.round(clipXToPx(clipX, canvas.width)),
          value: t.value,
        });
      }
      tickSegments.push(clipX, box.y.min - tickLen, clipX, box.y.min);

      if (!t.label || !spec.measurer) continue;
      const m = spec.measurer.measure(t.label, fontPx);
      if (m.glyphCount <= 0) continue;
      // Centre on the tick, and hang the ink's TOP `gap` below the tick's end.
      const clipXBaseline =
        clipX - pxSpanToClipX((m.inkMaxXPx + m.inkMinXPx) / 2, canvas);
      const inkTop = box.y.min - tickLen - gap;
      const clipYBaseline = inkTop - pxSpanToClipY(m.inkMaxYPx, canvas);
      tryPlace({
        axis: "x",
        role: "xTickLabel",
        text: t.label,
        clipX: clipXBaseline,
        clipY: clipYBaseline,
        fontPx,
        fontSizeClip,
        xScale,
        bbox: labelBoxPx(clipXBaseline, clipYBaseline, m, canvas),
      });
    }
  }

  // ── the spines: the two edges of the box the furniture hangs off
  if (spec.spine ?? true) {
    if (spec.y) spineSegments.push(box.x.min, box.y.min, box.x.min, box.y.max);
    if (spec.x) spineSegments.push(box.x.min, box.y.min, box.x.max, box.y.min);
  }

  // ── axis titles, in the far corner of each gutter (drawn last: lowest priority)
  if (spec.measurer) {
    const titleFontPx = (spec.y?.fontPx ?? spec.x?.fontPx ?? AXIS_DEFAULTS.fontPx);
    const titleFontSize = fontSizeForPx(titleFontPx, canvas);
    if (spec.y?.title) {
      const m = spec.measurer.measure(spec.y.title, titleFontPx);
      if (m.glyphCount > 0) {
        // Top-left of the left gutter, hung under the clip ceiling.
        const clipX = g.left.x.min + pxSpanToClipX(2, canvas);
        const clipY = CLIP_RANGE.max - pxSpanToClipY(2 + m.inkMaxYPx, canvas);
        tryPlace({
          axis: "y",
          role: "axisTitle",
          text: spec.y.title,
          clipX,
          clipY,
          fontPx: titleFontPx,
          fontSizeClip: titleFontSize,
          xScale,
          bbox: labelBoxPx(clipX, clipY, m, canvas),
        });
      }
    }
    if (spec.x?.title) {
      const m = spec.measurer.measure(spec.x.title, titleFontPx);
      if (m.glyphCount > 0) {
        // Bottom-right of the bottom gutter, right-aligned on the box edge.
        const clipX = box.x.max - pxSpanToClipX(m.inkMaxXPx + 2, canvas);
        const clipY = CLIP_RANGE.min + pxSpanToClipY(2 - m.inkMinYPx, canvas);
        tryPlace({
          axis: "x",
          role: "axisTitle",
          text: spec.x.title,
          clipX,
          clipY,
          fontPx: titleFontPx,
          fontSizeClip: titleFontSize,
          xScale,
          bbox: labelBoxPx(clipX, clipY, m, canvas),
        });
      }
    }
  }

  const plotPx: [number, number, number, number] = [
    Math.round(clipXToPx(box.x.min, canvas.width)),
    Math.round(clipYToPx(box.y.max, canvas.height)),
    Math.round(clipXToPx(box.x.max, canvas.width)) - 1,
    Math.round(clipYToPx(box.y.min, canvas.height)) - 1,
  ];

  return {
    gridSegments,
    tickSegments,
    spineSegments,
    labels: placed,
    gridLines,
    droppedLabels,
    plotPx,
  };
}

// ── D1's tier-1 label assertions, runnable without a raster ─────────────────

/** A tier-1 verdict over a plan's labels. */
export interface Tier1LabelVerdict {
  pass: boolean;
  failures: string[];
  /** Counts, so a PASS on an empty axis is visible rather than flattering. */
  counts: { x: number; y: number; titles: number };
}

/**
 * Check D1's tier-1 label row against a plan: label boxes disjoint, inside the
 * frame, and — when the x axis states time — every x label parsing as a
 * timestamp (`parsesAsTimestamp`, ENC-1254).
 *
 * This is the same question `score.py`'s T1.1/T1.4/T1.5 ask of the delivered
 * raster, asked of the plan instead, so a collision is a unit-test failure
 * rather than something found by rendering. It does NOT replace scoring the
 * raster: only the raster can say the glyphs actually landed.
 */
export function checkTier1Labels(
  plan: AxisPlan,
  canvas: CanvasSize,
  opts: { xIsTime?: boolean } = {},
): Tier1LabelVerdict {
  const failures: string[] = [];
  const labels = plan.labels;
  const x = labels.filter((l) => l.role === "xTickLabel");
  const y = labels.filter((l) => l.role === "yTickLabel");

  if (x.length === 0) failures.push("no engine-drawn x tick labels");
  if (y.length === 0) failures.push("no engine-drawn y tick labels");

  for (const l of labels) {
    const [bx, by, bw, bh] = l.bbox;
    if (bx < 0 || by < 0 || bx + bw > canvas.width || by + bh > canvas.height) {
      failures.push(`${JSON.stringify(l.text)} is clipped by the frame`);
    }
  }
  for (let i = 0; i < labels.length; i++) {
    for (let j = i + 1; j < labels.length; j++) {
      if (boxesOverlap(labels[i].bbox, labels[j].bbox)) {
        failures.push(
          `${JSON.stringify(labels[i].text)} overlaps ${JSON.stringify(labels[j].text)}`,
        );
      }
    }
  }
  if (opts.xIsTime ?? true) {
    for (const l of x) {
      if (!parsesAsTimestamp(l.text)) {
        failures.push(`x tick label ${JSON.stringify(l.text)} does not parse as a timestamp`);
      }
    }
  }
  return {
    pass: failures.length === 0,
    failures,
    counts: { x: x.length, y: y.length, titles: labels.filter((l) => l.role === "axisTitle").length },
  };
}

// ── The scene descriptor the tier scorer consumes ───────────────────────────

/**
 * The scene-JSON fragment describing what this axis DREW, in the vocabulary
 * `specs/2026-09-19-chart-quality-bar/harness/score.py` reads.
 *
 * The scorer grades a scene declaration corroborated by pixel probes — it looks
 * for ink inside each declared label box, samples each declared gridline's row
 * or column, and classifies the raster's colours against the declared palette.
 * `scenes/README.md` states the intended source explicitly: "a scene dumped by
 * the renderer cannot describe an axis the renderer did not draw." This is that
 * dump. It is emitted from the PLAN — the same numbers that were handed to the
 * engine — so it cannot describe a label the engine was not asked to draw.
 *
 * It is deliberately a FRAGMENT: `tier0`, `series` and `raster` belong to the
 * capture and the data path, not to the axis, and inventing them here is how a
 * scene ends up asserting something nobody measured.
 */
export interface AxisSceneFragment {
  text: {
    role: "xTickLabel" | "yTickLabel" | "axisTitle";
    text: string;
    bbox: [number, number, number, number];
    fontPx: number;
    source: "engine";
  }[];
  grid: { orientation: "horizontal" | "vertical"; centre: number; pad: number }[];
  axis: { spines: { orientation: "left" | "bottom" }[] };
  plot: [number, number, number, number];
  palette: { rgb: [number, number, number]; role: string; what: string }[];
  theme: { name: string; palette: [number, number, number][] };
}

/** 0..1 float → the 0..255 integer the raster quantises it to. */
function to255(c: number): number {
  return Math.round(Math.max(0, Math.min(1, c)) * 255);
}

/** An `Rgba4`-ish triple at 8-bit, for declaring a colour to the scorer. */
function rgb255(c: readonly number[]): [number, number, number] {
  return [to255(c[0]), to255(c[1]), to255(c[2])];
}

/**
 * Describe a drawn axis for the tier scorer.
 *
 * `gridPad` is the half-width of the neighbourhood `score.py` compares each
 * gridline pixel against; 3 is its default and matches a 1px line with an
 * antialiased fringe.
 *
 * The declared gridline colour is the theme's gridline COMPOSITED over the
 * background it is drawn on, because that is what the raster contains. A theme
 * whose gridline carries alpha (midnight, pastel, neon, bloomberg all do)
 * declares a colour that appears nowhere in the frame otherwise.
 */
export function axisSceneFragment(
  plan: AxisPlan,
  theme: AxisTheme,
  background: readonly [number, number, number],
  gridPad = 3,
): AxisSceneFragment {
  const g = gridRgba(theme);
  const a = g.a ?? 1;
  const gridComposited: [number, number, number] = [
    g.r * a + background[0] * (1 - a),
    g.g * a + background[1] * (1 - a),
    g.b * a + background[2] * (1 - a),
  ];
  const palette = [
    { rgb: rgb255(gridComposited), role: "gridline", what: `${theme.name} gridColor over the pane` },
    { rgb: rgb255(theme.tickColor), role: "tick", what: `${theme.name} tickColor` },
    { rgb: rgb255(theme.tickColor), role: "spine", what: `${theme.name} tickColor (the spine)` },
    { rgb: rgb255(theme.labelColor), role: "label", what: `${theme.name} labelColor` },
  ];
  return {
    text: plan.labels.map((l) => ({
      role: l.role,
      text: l.text,
      bbox: l.bbox,
      fontPx: l.fontPx,
      source: "engine" as const,
    })),
    grid: plan.gridLines.map((l) => ({
      orientation: l.orientation,
      centre: l.centre,
      pad: gridPad,
    })),
    axis: {
      spines: Array.from({ length: plan.spineSegments.length / 4 }, (_, i) => ({
        orientation: i === 0 ? ("left" as const) : ("bottom" as const),
      })),
    },
    plot: plan.plotPx,
    palette,
    theme: { name: theme.name, palette: palette.map((p) => p.rgb) },
  };
}

// ── Driving a live engine ───────────────────────────────────────────────────

/** The engine surface `EngineAxis` drives. `EngineHost` satisfies it. */
export interface AxisTarget extends TextTarget {
  applyDataBatch(batch: ArrayBuffer): void;
  /**
   * `setTextGeometry` with the horizontal scale that undoes clip space's aspect
   * distortion (ENC-1253). Optional: a host without it falls back to the
   * isotropic `setTextGeometry`, and the labels come out stretched by the
   * canvas's aspect ratio rather than not at all.
   */
  setTextGeometryX?(
    bufferId: number,
    geometryId: number,
    text: string,
    clipX: number,
    clipY: number,
    fontSize: number,
    xScale: number,
  ): number;
  /** The core's text measurer, when the host exposes one. */
  measureText?(
    text: string,
    fontSize: number,
    xScale: number,
  ): {
    advanceWidth: number;
    inkMinX: number;
    inkMaxX: number;
    inkMinY: number;
    inkMaxY: number;
    glyphCount: number;
    glyphPx: number;
  };
}

/**
 * An `AxisTextMeasurer` backed by the core's own layout loop.
 *
 * Returns null when the host has no `measureText` — an older committed wasm.
 * A caller that gets null must draw no labels rather than estimate them: an
 * estimated width is how labels overlap for some strings and not others, which
 * is a tier-1 failure that only shows up on the data that produced it.
 */
export function createHostMeasurer(
  target: AxisTarget,
  canvas: CanvasSize,
): AxisTextMeasurer | null {
  if (typeof target.measureText !== "function") return null;
  const xScale = textXScale(canvas);
  const toPxX = (clip: number) => (clip * canvas.width) / 2;
  const toPxY = (clip: number) => (clip * canvas.height) / 2;
  return {
    measure(text: string, fontPx: number): TextMetrics {
      const m = target.measureText!(text, fontSizeForPx(fontPx, canvas), xScale);
      return {
        advanceWidthPx: toPxX(m.advanceWidth),
        inkMinXPx: toPxX(m.inkMinX),
        inkMaxXPx: toPxX(m.inkMaxX),
        inkMinYPx: toPxY(m.inkMinY),
        inkMaxYPx: toPxY(m.inkMaxY),
        glyphCount: m.glyphCount,
      };
    },
  };
}

/** Which furniture class a draw item belongs to — also the scorer's roles. */
export type AxisInkRole = "gridline" | "tick" | "spine" | "label";

/** Ids and state for one furniture class. */
interface FurnitureSlot {
  bufferId: number;
  geometryId: number;
  drawItemId: number;
  bound: boolean;
  count: number;
}

/** One text slot in the label pool. */
interface LabelSlot extends FurnitureSlot {
  text: string;
}

/**
 * The live axis: owns its ids, draws the furniture once, and re-syncs it in
 * place when the domain, the ticks or the canvas move.
 *
 * WHY A POOL RATHER THAN A REBUILD. Re-authoring the furniture every time the
 * measured domain moves would allocate fresh buffer/geometry/drawItem ids on
 * every replayed batch — an unbounded scene. So each furniture class owns ONE
 * buffer, rewritten in place with an `updateRange` record and re-counted with
 * `setGeometryVertexCount`, and the labels own a fixed pool of text slots. A
 * slot whose string is empty lays out zero glyphs and draws nothing, so the
 * label count can fall as well as rise without any resource churn.
 *
 * Bind-on-first-use is not an optimisation either: `bindDrawItem` rejects a
 * geometry whose `vertexCount` is 0 (`VALIDATION_BAD_VERTEX_COUNT`), so a slot
 * cannot be bound until it has something in it.
 */
export class EngineAxis {
  private readonly target: AxisTarget;
  private readonly ids: IdAllocator;
  private readonly maxLabels: number;
  private paneId = 0;
  private gridLayerId = 0;
  /** Set when the grid layer lives in a CALLER's pane (`AxisSpec.gridTarget`). */
  private foreignGridPaneId = 0;
  private furnitureLayerId = 0;
  private labelLayerId = 0;
  private scaffolded = false;

  private grid: FurnitureSlot | null = null;
  private ticks: FurnitureSlot | null = null;
  private spine: FurnitureSlot | null = null;
  private labelSlots: LabelSlot[] = [];
  private lastPlan: AxisPlan | null = null;

  /**
   * @param target the engine surface (an `EngineHost`, or a capture mock).
   * @param ids    id allocator — inject the chart's own so ids cannot collide.
   * @param maxLabels size of the text pool. 24 covers a dense axis on both
   *   sides (D1 tier 1 wants readable ticks, not every tick labelled).
   */
  constructor(target: AxisTarget, ids: IdAllocator = createIdAllocator(), maxLabels = 24) {
    this.target = target;
    this.ids = ids;
    this.maxLabels = maxLabels;
  }

  /** The last plan this axis drew, or null before the first `sync`. */
  plan(): AxisPlan | null {
    return this.lastPlan;
  }

  /** The pane the furniture lives in (0 before the first `sync`). */
  furniturePaneId(): number {
    return this.paneId;
  }

  /**
   * Draw (or re-draw) the furniture for `spec`.
   *
   * Safe to call on every domain change: it rewrites the existing buffers
   * rather than creating new ones. Call it when the host is READY and NOT
   * mid-render — the WASM core is single-async-op (the same discipline
   * `SceneBuilder` and `drawText` document).
   */
  sync(spec: AxisSpec): AxisPlan {
    const theme = spec.theme ?? defaultAxisTheme;
    const plan = planAxis(spec);
    this.ensureScaffold(theme, spec.gridTarget);

    this.grid = this.syncLines(
      this.grid,
      this.gridLayerId,
      plan.gridSegments,
      gridRgba(theme),
      theme.gridLineWidth,
      { dashLength: theme.gridDashLength, gapLength: theme.gridGapLength },
    );
    this.ticks = this.syncLines(
      this.ticks,
      this.furnitureLayerId,
      plan.tickSegments,
      rgba(theme.tickColor),
      theme.tickLineWidth,
    );
    this.spine = this.syncLines(
      this.spine,
      this.furnitureLayerId,
      plan.spineSegments,
      rgba(theme.tickColor),
      theme.tickLineWidth,
    );
    const drawn = this.syncLabels(plan.labels, rgba(theme.labelColor));

    // A label the core REFUSED to lay out is not in the raster, so it must not
    // be in the plan either — the plan is what the scene dump and the tier-1
    // check are built from, and a declared-but-undrawn label is precisely the
    // caption failure this ticket exists to remove. `EngineHost` returns -1
    // from `setTextGeometry` while a render is in flight; the refused slot keeps
    // whatever it last held, so the honest report is "fewer labels", and the
    // caller retries.
    if (drawn < plan.labels.length) {
      plan.droppedLabels.push(
        ...plan.labels.slice(drawn).map((l) => ({
          text: l.text,
          axis: l.axis,
          reason: "the core refused the layout (busy or no font)",
        })),
      );
      plan.labels = plan.labels.slice(0, drawn);
    }

    this.lastPlan = plan;
    return plan;
  }

  /**
   * Remove every resource this axis owns.
   *
   * The engine's teardown verb is `{cmd:'delete', id}` — one command that
   * resolves the kind from the registry (`CommandProcessor::cmdDelete`), not a
   * per-kind `destroyX`. Deleting the PANE cascades to its layers and their
   * draw items; geometries and buffers are top-level resources and the cascade
   * does not reach them, so they are deleted explicitly. Same order and same
   * reasoning as the showcase's `resetScene`.
   */
  dispose(): void {
    if (this.paneId) this.ctrl({ cmd: "delete", id: this.paneId });
    // A grid layer in the CALLER's pane is not reached by that cascade, and the
    // caller's pane is torn down and rebuilt on every manifest re-apply — so the
    // layer has to be dropped by name. Deleting an id the scene no longer has is
    // a rejection, not a fault (DC-L06): the pane may already have taken it.
    if (this.foreignGridPaneId && this.gridLayerId) {
      this.ctrl({ cmd: "delete", id: this.gridLayerId });
    }
    const dropData = (slot: FurnitureSlot | null) => {
      if (!slot) return;
      this.ctrl({ cmd: "delete", id: slot.geometryId });
      this.ctrl({ cmd: "delete", id: slot.bufferId });
    };
    dropData(this.grid);
    dropData(this.ticks);
    dropData(this.spine);
    for (const s of this.labelSlots) dropData(s);
    this.grid = this.ticks = this.spine = null;
    this.labelSlots = [];
    this.paneId = this.gridLayerId = this.furnitureLayerId = this.labelLayerId = 0;
    this.foreignGridPaneId = 0;
    this.scaffolded = false;
    this.lastPlan = null;
  }

  // ---- internals ----------------------------------------------------------

  /**
   * The furniture pane, at `FULL_CLIP_REGION`.
   *
   * This is plotbox.ts contract note (7)'s consequence, and it is the one that
   * fails silently: the DATA pane's region is the plot box, applied as a
   * scissor, so ticks and labels drawn there — in the gutters, outside the box
   * — are clipped away with no rejection and nothing in the error list. Three
   * layers so the paint order is grid (under the data's siblings), then the
   * spine and ticks, then the labels on top.
   */
  private ensureScaffold(theme: AxisTheme, gridTarget?: AxisGridTarget): void {
    if (this.scaffolded) return;
    void theme;
    this.paneId = this.ids.nextFor("pane");
    this.ctrl({ cmd: "createPane", id: this.paneId, name: "axis" });
    this.ctrl({
      cmd: "setPaneRegion",
      id: this.paneId,
      clipXMin: FULL_CLIP_REGION.clipXMin,
      clipXMax: FULL_CLIP_REGION.clipXMax,
      clipYMin: FULL_CLIP_REGION.clipYMin,
      clipYMax: FULL_CLIP_REGION.clipYMax,
    });
    // THE GRID GOES BEHIND THE DATA WHEN THE CALLER SAYS WHERE (ENC-1316). Its
    // layer is created in the caller's data pane with the caller's id, which is
    // below every layer that pane already holds, so it is drawn after the pane's
    // clear quad and before the marks. Read ONCE, here: a change of target means
    // a different pane, which means the whole scaffold has to be rebuilt —
    // `dispose()` then `sync()`.
    if (gridTarget) {
      this.foreignGridPaneId = gridTarget.paneId;
      this.gridLayerId = gridTarget.layerId;
      this.ctrl({
        cmd: "createLayer",
        id: this.gridLayerId,
        paneId: gridTarget.paneId,
        name: "axis-grid",
      });
    } else {
      this.gridLayerId = this.ids.nextFor("layer");
      this.ctrl({ cmd: "createLayer", id: this.gridLayerId, paneId: this.paneId, name: "axis-grid" });
    }
    this.furnitureLayerId = this.ids.nextFor("layer");
    this.ctrl({
      cmd: "createLayer",
      id: this.furnitureLayerId,
      paneId: this.paneId,
      name: "axis-marks",
    });
    this.labelLayerId = this.ids.nextFor("layer");
    this.ctrl({
      cmd: "createLayer",
      id: this.labelLayerId,
      paneId: this.paneId,
      name: "axis-labels",
    });
    this.scaffolded = true;
  }

  /** Create-or-rewrite one `lineAA@1` rect4 buffer in place. */
  private syncLines(
    slot: FurnitureSlot | null,
    layerId: number,
    segments: readonly number[],
    color: Rgba,
    lineWidth: number,
    dash?: { dashLength: number; gapLength: number },
  ): FurnitureSlot | null {
    const count = Math.floor(segments.length / 4);
    if (!slot) {
      if (count === 0) return null; // nothing to draw and nothing to keep
      slot = {
        bufferId: this.ids.nextFor("buffer"),
        geometryId: this.ids.nextFor("geometry"),
        drawItemId: this.ids.nextFor("drawItem"),
        bound: false,
        count: 0,
      };
      this.ctrl({
        cmd: "createBuffer",
        id: slot.bufferId,
        byteLength: segments.length * 4,
      });
      this.ctrl({
        cmd: "createGeometry",
        id: slot.geometryId,
        vertexBufferId: slot.bufferId,
        format: "rect4",
        vertexCount: 0,
      });
      this.ctrl({ cmd: "createDrawItem", id: slot.drawItemId, layerId });
    }

    // Replace the bytes at offset 0 rather than appending: an append record
    // GROWS the buffer (IngestProcessor OP_APPEND), so re-syncing the axis on
    // every domain change would leave every previous frame's gridlines behind
    // it, still counted if the vertex count ever rose.
    this.target.applyDataBatch(encodeUpdateRecord(slot.bufferId, segments));
    this.ctrl({
      cmd: "setGeometryVertexCount",
      geometryId: slot.geometryId,
      vertexCount: count,
    });

    if (!slot.bound && count > 0) {
      this.ctrl({
        cmd: "bindDrawItem",
        drawItemId: slot.drawItemId,
        pipeline: "lineAA@1",
        geometryId: slot.geometryId,
      });
      slot.bound = true;
    }
    if (slot.bound) {
      const style: Record<string, unknown> = {
        cmd: "setDrawItemStyle",
        drawItemId: slot.drawItemId,
        r: color.r,
        g: color.g,
        b: color.b,
        a: color.a ?? 1,
        lineWidth,
      };
      if (dash && dash.dashLength > 0) {
        style.dashLength = dash.dashLength;
        style.gapLength = dash.gapLength;
      }
      this.ctrl(style);
    }
    slot.count = count;
    return slot;
  }

  /**
   * Lay the pooled label slots out, emptying the ones this plan does not use.
   * Returns how many were actually laid out — a prefix count, because the run
   * stops at the first refusal rather than leaving a hole in the middle.
   */
  private syncLabels(labels: readonly AxisLabel[], color: Rgba): number {
    const n = Math.min(labels.length, this.maxLabels);
    let drawn = 0;
    for (let i = 0; i < n; i++) {
      const slot = this.labelSlot(i);
      const l = labels[i];
      const glyphs = this.layout(slot, l.text, l.clipX, l.clipY, l.fontSizeClip, l.xScale);
      // -1 means the core refused (busy, or no font). Stop: continuing would
      // leave slot i showing its PREVIOUS label — a stale number beside a fresh
      // gridline, which is worse than a missing one.
      if (glyphs < 0) break;
      drawn++;
      slot.text = l.text;
      slot.count = glyphs;
      if (!slot.bound && glyphs > 0) {
        this.ctrl({
          cmd: "bindDrawItem",
          drawItemId: slot.drawItemId,
          pipeline: "textSDF@1",
          geometryId: slot.geometryId,
        });
        slot.bound = true;
      }
      if (slot.bound) {
        this.ctrl({
          cmd: "setDrawItemColor",
          drawItemId: slot.drawItemId,
          r: color.r,
          g: color.g,
          b: color.b,
          a: color.a ?? 1,
        });
      }
    }
    // Empty every slot this plan does not use: an empty string lays out zero
    // glyphs, so the slot survives with nothing in it rather than being
    // destroyed and re-created on the next tick that needs it. A refusal here
    // leaves the slot's old glyphs on screen, so it is reported as a shortfall
    // too — `drawn` is a PREFIX count and the caller retries the whole sync.
    for (let i = drawn; i < this.labelSlots.length; i++) {
      const slot = this.labelSlots[i];
      if (slot.text === "") continue;
      if (this.layout(slot, "", 0, 0, 0.01, 1) < 0) return Math.min(drawn, i);
      slot.text = "";
      slot.count = 0;
    }
    return drawn;
  }

  private labelSlot(i: number): LabelSlot {
    let slot = this.labelSlots[i];
    if (slot) return slot;
    slot = {
      bufferId: this.ids.nextFor("buffer"),
      geometryId: this.ids.nextFor("geometry"),
      drawItemId: this.ids.nextFor("drawItem"),
      bound: false,
      count: 0,
      text: "",
    };
    this.ctrl({ cmd: "createBuffer", id: slot.bufferId, byteLength: 0 });
    this.ctrl({
      cmd: "createGeometry",
      id: slot.geometryId,
      vertexBufferId: slot.bufferId,
      vertexCount: 0,
      format: "glyph8",
    });
    this.ctrl({ cmd: "createDrawItem", id: slot.drawItemId, layerId: this.labelLayerId });
    this.labelSlots[i] = slot;
    return slot;
  }

  private layout(
    slot: LabelSlot,
    text: string,
    clipX: number,
    clipY: number,
    fontSize: number,
    xScale: number,
  ): number {
    if (typeof this.target.setTextGeometryX === "function") {
      return this.target.setTextGeometryX(
        slot.bufferId,
        slot.geometryId,
        text,
        clipX,
        clipY,
        fontSize,
        xScale,
      );
    }
    return this.target.setTextGeometry(
      slot.bufferId,
      slot.geometryId,
      text,
      clipX,
      clipY,
      fontSize,
    );
  }

  private ctrl(command: object): void {
    this.target.applyControl(command);
  }
}

/**
 * Encode an `updateRange` (op 2) record: REPLACE `bufferId`'s bytes from offset
 * 0 rather than appending to them.
 *
 * `encodeAppendRecord` (SceneBuilder) is op 1 and grows the buffer — correct for
 * a streaming series, wrong for furniture that is rewritten whenever the domain
 * moves. `IngestProcessor::OP_UPDATE_RANGE` resizes the buffer up when the new
 * bytes are longer, and leaves any tail beyond them in place; the accompanying
 * `setGeometryVertexCount` is what stops a stale tail from being drawn.
 */
export function encodeUpdateRecord(bufferId: number, floats: ArrayLike<number>): ArrayBuffer {
  const payload = new Float32Array(floats);
  const out = new ArrayBuffer(13 + payload.byteLength);
  const dv = new DataView(out);
  dv.setUint8(0, 2); // OP_UPDATE_RANGE
  dv.setUint32(1, bufferId, true);
  dv.setUint32(5, 0, true); // offset
  dv.setUint32(9, payload.byteLength, true);
  new Uint8Array(out, 13).set(new Uint8Array(payload.buffer));
  return out;
}

// Re-exported so a caller assembling a chart does not have to import
// SceneBuilder just to push a series alongside the axis it just drew.
export { encodeAppendRecord };
