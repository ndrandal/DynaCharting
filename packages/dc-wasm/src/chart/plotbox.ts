/* packages/dc-wasm/src/chart/plotbox.ts — ENC-1256 (chart-quality-bar SPEC D1 tier 2, §1.0)
 *
 * THE PLOT BOX: the frame the data is fitted into, and the gutters that are NOT
 * the data's to draw in.
 *
 * Before this module the engine had no plot-box or margin concept at all.
 * Geometry was projected into the *whole* of clip space, there was no reserved
 * band for axis furniture, and "the plot area" and "the canvas" were the same
 * rectangle. Two consequences, both measured on the live product
 * (`specs/2026-09-19-chart-quality-bar/harness/live-nexo-candles-60s.png`,
 * 1337x688 canvas, nvidia/ampere):
 *
 *   - Nothing framed the series. 66.4% of the canvas was dead margin (the ink
 *     bounding box covered 33.6% of it) because the transform was a literal
 *     typed into a view file rather than a function of the data.
 *   - Nothing reserved clearance, so the series ran into the clip boundary:
 *     SEVEN independent candles shared an identical top row (canvas row 17) —
 *     not seven equal highs, a flat cut.
 *
 * ENC-1252 supplied the missing input — `DomainTracker` measures the domain
 * from the streamed records. This module supplies the missing output: the
 * rectangle that domain is fitted INTO, and the transform that does it.
 *
 * ── THE CONTRACT ────────────────────────────────────────────────────────────
 *
 * ENC-1253 (engine-rendered ticks, gridlines and spine) positions against this,
 * so the semantics are stated rather than implied:
 *
 *   1. CLIP SPACE IS THE CANVAS. `[-1,1]` on both axes covers the whole render
 *      target, +Y up (the repo ORIENTATION note in scale.ts; the blit owns the
 *      Y flip, ENC-696/DC-L05). Nothing here compensates for it.
 *
 *   2. THE PLOT BOX IS A STRICT SUB-RECTANGLE OF CLIP SPACE. `PlotBox` is a
 *      pair of clip-space `Range`s. Data marks live inside it. Everything
 *      between the box and the clip boundary is a GUTTER and belongs to axis
 *      furniture — spine, ticks, tick labels, axis titles.
 *
 *   3. GUTTERS ARE SPECIFIED IN CSS PIXELS, THE BOX IN CLIP UNITS. A tick label
 *      is ~11px tall whatever the chart's size, so a gutter expressed as a clip
 *      fraction would shrink the labels on a small canvas and strand them on a
 *      large one. `plotBox()` is the ONLY place that conversion happens; a
 *      consumer that needs another pixel length in clip units calls
 *      `pxToClipX`/`pxToClipY` rather than re-deriving `2/width`.
 *
 *   4. THE DOMAIN IS INK EXTENT, NOT MARK CENTRES. `DomainTracker` folds
 *      candle6's `halfWidth`, so its X domain is the span of the BARS, not of
 *      their centres, and its Y domain spans low..high, not open..close. Fitting
 *      that domain to the box therefore puts the outermost INK on the box
 *      boundary — which is what makes "no geometry intersects the frame edge"
 *      follow from a positive inset rather than from hope. Do not feed this a
 *      centres-only range and then add padding to cover the difference.
 *
 *   5. PADDING DEFAULTS TO ZERO, AND THAT IS NOT AN OVERSIGHT. Because of (4)
 *      the ink already ends where the domain ends. Padding would buy clearance
 *      that the gutter already provides, and pay for it in dead margin — the
 *      exact tier-2 measure this ticket exists to lower. `paddingFrac` is there
 *      for a caller with a reason (headroom for an annotation), not as a
 *      default cushion.
 *
 *   6. A GRIDLINE SPANS THE BOX, NOT THE CANVAS. `gutters()` returns the four
 *      furniture rectangles explicitly so ENC-1253 never has to guess whether a
 *      tick at `box.y.min` sits on the spine or under it.
 *
 * ── WHAT THIS MODULE IS NOT ─────────────────────────────────────────────────
 *
 * It does not measure the domain (ENC-1252, `domain.ts`), draw anything
 * (ENC-1253), size a bar (ENC-1257, merged), or know about panes. It is pure,
 * DOM-free and engine-free: it turns numbers into numbers, and every one of
 * them is checkable.
 */

import { fitTransform, padRange, CLIP_RANGE, type Range, type Transform2D } from "./scale";

/** Canvas size in CSS pixels — the render target the clip box maps onto. */
export interface CanvasSize {
  width: number;
  height: number;
}

/**
 * Reserved gutter widths in CSS pixels, one per side. `top`/`bottom` are in
 * SCREEN orientation (top = the visually upper edge), which is clip +Y; the
 * conversion in `plotBox()` is the only place that has to care.
 */
export interface PlotInsets {
  top: number;
  right: number;
  bottom: number;
  left: number;
}

/**
 * A clip-space rectangle: `x` and `y` are ranges within `[-1,1]`, +Y up.
 * `PlotBox` is the data's rectangle; `gutters()` returns the furniture's.
 */
export interface PlotBox {
  x: Range;
  y: Range;
}

/** The four furniture rectangles around a plot box, in clip space. */
export interface PlotGutters {
  /** Above the box: clip y in [box.y.max, 1]. Chart title / headroom. */
  top: PlotBox;
  /** Right of the box: clip x in [box.x.max, 1]. */
  right: PlotBox;
  /** Below the box: clip y in [-1, box.y.min]. X spine, ticks, time labels. */
  bottom: PlotBox;
  /** Left of the box: clip x in [-1, box.x.min]. Y spine, ticks, price labels. */
  left: PlotBox;
}

/**
 * Default gutters, in CSS pixels. Sized for what ENC-1253 has to put in them,
 * not chosen for looks:
 *
 *   left   64 — a price label at 11px, 7 glyphs ("418.25"), plus a 6px tick and
 *                4px of breathing room. The live product jams `$408.00` against
 *                the frame today (SPEC §1.1) because this band does not exist.
 *   bottom 28 — one line of 11px time labels ("14:32:05") plus a 6px tick.
 *   top    12 — no furniture; clearance so a high wick does not abut the edge.
 *   right  16 — no furniture; clearance for the last bar's half-width and the
 *                lineAA cap of any overlay that ends at the newest sample.
 *
 * On a 1280x800 canvas these cost 6.3% of the width and 5.0% of the height, so
 * the box covers 89.1% of the canvas — comfortably inside TIER2_FRAMING_BOUNDS.
 */
export const DEFAULT_PLOT_INSETS: Readonly<PlotInsets> = {
  top: 12,
  right: 16,
  bottom: 28,
  left: 64,
};

/** Clip-space length of `px` horizontal CSS pixels on a `canvas`-wide target. */
export function pxToClipX(px: number, canvas: CanvasSize): number {
  return (2 * px) / canvas.width;
}

/** Clip-space length of `px` vertical CSS pixels on a `canvas`-tall target. */
export function pxToClipY(px: number, canvas: CanvasSize): number {
  return (2 * px) / canvas.height;
}

/** CSS-pixel length of a clip-space horizontal distance. Inverse of pxToClipX. */
export function clipXToPx(clip: number, canvas: CanvasSize): number {
  return (clip * canvas.width) / 2;
}

/** CSS-pixel length of a clip-space vertical distance. Inverse of pxToClipY. */
export function clipYToPx(clip: number, canvas: CanvasSize): number {
  return (clip * canvas.height) / 2;
}

/**
 * Thrown when the requested insets leave no plot box — a 700px left gutter on a
 * 600px canvas. Silently clamping would hand back an inverted or zero-width box
 * and let a downstream `fitAxis` produce a negative or infinite scale, i.e. a
 * chart that is mirrored or blank for a reason nobody can find.
 */
export class PlotBoxError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "PlotBoxError";
  }
}

/**
 * The clip-space plot box for `canvas` with `insets` reserved for furniture.
 *
 * Throws `PlotBoxError` if the gutters do not leave a positive-area box, or if
 * any input is non-finite/negative. This is a programming error, not a data
 * condition: the domain can be empty, but the frame cannot be nonsense.
 */
export function plotBox(canvas: CanvasSize, insets: PlotInsets = DEFAULT_PLOT_INSETS): PlotBox {
  const { width, height } = canvas;
  if (!(Number.isFinite(width) && Number.isFinite(height) && width > 0 && height > 0)) {
    throw new PlotBoxError(`plotBox: canvas must be positive and finite, got ${width}x${height}`);
  }
  for (const side of ["top", "right", "bottom", "left"] as const) {
    const v = insets[side];
    if (!(Number.isFinite(v) && v >= 0)) {
      throw new PlotBoxError(`plotBox: inset.${side} must be finite and >= 0, got ${v}`);
    }
  }
  if (insets.left + insets.right >= width) {
    throw new PlotBoxError(
      `plotBox: horizontal insets (${insets.left}+${insets.right}) leave no plot box in ${width}px`,
    );
  }
  if (insets.top + insets.bottom >= height) {
    throw new PlotBoxError(
      `plotBox: vertical insets (${insets.top}+${insets.bottom}) leave no plot box in ${height}px`,
    );
  }
  return {
    x: {
      min: CLIP_RANGE.min + pxToClipX(insets.left, canvas),
      max: CLIP_RANGE.max - pxToClipX(insets.right, canvas),
    },
    // Screen-top is clip +Y: `insets.top` comes off `y.max`.
    y: {
      min: CLIP_RANGE.min + pxToClipY(insets.bottom, canvas),
      max: CLIP_RANGE.max - pxToClipY(insets.top, canvas),
    },
  };
}

/**
 * The four furniture rectangles around `box`, in clip space. Each is a full
 * band out to the clip boundary, so the corners are shared between a horizontal
 * and a vertical gutter — ENC-1253 decides which one owns a corner, this does
 * not decide it for it.
 */
export function gutters(box: PlotBox): PlotGutters {
  return {
    top: { x: box.x, y: { min: box.y.max, max: CLIP_RANGE.max } },
    right: { x: { min: box.x.max, max: CLIP_RANGE.max }, y: box.y },
    bottom: { x: box.x, y: { min: CLIP_RANGE.min, max: box.y.min } },
    left: { x: { min: CLIP_RANGE.min, max: box.x.min }, y: box.y },
  };
}

/** Framing policy for `fitToPlotBox`. */
export interface FitPolicy {
  /**
   * Symmetric headroom added to the X domain before fitting, as a fraction of
   * its span. Default 0 — see contract note (5).
   */
  paddingFracX?: number;
  /** Symmetric headroom added to the Y domain. Default 0. */
  paddingFracY?: number;
}

/**
 * The data→clip transform that maps `domain` onto `box`. This is the whole of
 * "fit the series to the viewport, with margins": the margins are the gutters
 * `box` already excludes, and the fit is `fitTransform` against the box's
 * ranges rather than against all of clip space.
 *
 * `domain` is `DomainTracker.domain()`-shaped: either axis may be `null` when
 * nothing has streamed. A null axis is mapped by `identity` for that axis (the
 * caller keeps whatever it had) rather than by an invented range — a chart with
 * no data should not claim a frame.
 */
export function fitToPlotBox(
  domain: { x: Range | null; y: Range | null },
  box: PlotBox,
  policy: FitPolicy = {},
  identity: Transform2D = { sx: 1, tx: 0, sy: 1, ty: 0 },
): Transform2D {
  const px = policy.paddingFracX ?? 0;
  const py = policy.paddingFracY ?? 0;
  const dx = domain.x ? (px > 0 ? padRange(domain.x, px) : domain.x) : null;
  const dy = domain.y ? (py > 0 ? padRange(domain.y, py) : domain.y) : null;

  const fitted = fitTransform(
    { x: dx ?? CLIP_RANGE, y: dy ?? CLIP_RANGE },
    { x: box.x, y: box.y },
  );
  return {
    sx: dx ? fitted.sx : identity.sx,
    tx: dx ? fitted.tx : identity.tx,
    sy: dy ? fitted.sy : identity.sy,
    ty: dy ? fitted.ty : identity.ty,
  };
}

// ── Tier-2 measures (SPEC D1) ───────────────────────────────────────────────

/**
 * What a framing actually achieved, in the units D1's tier-2 row asks for.
 * Every field is derived from the projected ink extent, so this scores a
 * transform against a domain — it does not need a screenshot, and it gives the
 * same answer a raster does when the raster's ink is the domain's ink.
 */
export interface FramingMetrics {
  /** Projected ink extent in clip space (the domain through the transform). */
  ink: PlotBox;
  /** 1 − (ink bbox area ÷ CANVAS area). D1's "dead-margin %", as a fraction. */
  deadMarginFrac: number;
  /** Ink bbox area ÷ PLOT BOX area. "Data fills the frame", as a fraction. */
  fillRatio: number;
  /** Per-side clearance from the ink to the clip boundary, in CSS pixels. */
  edgeClearancePx: PlotInsets;
  /** The smallest of the four. Negative means geometry left the frame. */
  minEdgeClearancePx: number;
  /** Ink extent in CSS pixels, for a raster-side cross-check. */
  inkPx: { width: number; height: number };
}

/**
 * Score a framing. `transform` is what the engine will be given, `domain` what
 * the data actually spans, `box` the plot box that was aimed at, `canvas` the
 * render target. Nothing is assumed to agree: passing a baked literal transform
 * here is exactly how you find out it does not fit the data.
 */
export function framingMetrics(
  domain: { x: Range; y: Range },
  transform: Transform2D,
  box: PlotBox,
  canvas: CanvasSize,
): FramingMetrics {
  const project = (v: number, s: number, t: number) => v * s + t;
  const xs = [
    project(domain.x.min, transform.sx, transform.tx),
    project(domain.x.max, transform.sx, transform.tx),
  ].sort((a, b) => a - b);
  const ys = [
    project(domain.y.min, transform.sy, transform.ty),
    project(domain.y.max, transform.sy, transform.ty),
  ].sort((a, b) => a - b);
  const ink: PlotBox = { x: { min: xs[0], max: xs[1] }, y: { min: ys[0], max: ys[1] } };

  const inkW = ink.x.max - ink.x.min;
  const inkH = ink.y.max - ink.y.min;
  const canvasArea = (CLIP_RANGE.max - CLIP_RANGE.min) ** 2; // 4, in clip units
  const boxArea = (box.x.max - box.x.min) * (box.y.max - box.y.min);

  const edgeClearancePx: PlotInsets = {
    left: clipXToPx(ink.x.min - CLIP_RANGE.min, canvas),
    right: clipXToPx(CLIP_RANGE.max - ink.x.max, canvas),
    bottom: clipYToPx(ink.y.min - CLIP_RANGE.min, canvas),
    top: clipYToPx(CLIP_RANGE.max - ink.y.max, canvas),
  };

  return {
    ink,
    deadMarginFrac: 1 - (inkW * inkH) / canvasArea,
    fillRatio: boxArea > 0 ? (inkW * inkH) / boxArea : Number.POSITIVE_INFINITY,
    edgeClearancePx,
    minEdgeClearancePx: Math.min(
      edgeClearancePx.top,
      edgeClearancePx.right,
      edgeClearancePx.bottom,
      edgeClearancePx.left,
    ),
    inkPx: { width: clipXToPx(inkW, canvas), height: clipYToPx(inkH, canvas) },
  };
}

/**
 * The stated tier-2 thresholds. D1 requires the numbers be stated, not felt;
 * these are the numbers, with the derivation beside each so the next person
 * argues with the reasoning rather than with the digit.
 */
export const TIER2_FRAMING_BOUNDS = {
  /**
   * Dead margin ceiling. `DEFAULT_PLOT_INSETS` on 1280x800 spends 10.9% of the
   * canvas on gutters, and a fitted box spends nothing else; 25% leaves room
   * for a tall narrow canvas (where a fixed 64px left gutter is a larger
   * fraction) while still condemning the live chart's measured 66.4%.
   */
  maxDeadMarginFrac: 0.25,
  /**
   * Fill band. A correct fit puts the ink bbox exactly on the box, i.e. 1.0.
   * The floor is what makes this a check rather than a tautology: a baked
   * literal transform (`candles-aapl`, DC-L13) fills ~0.56 of its box and
   * fails. The ceiling catches a fit that overflowed the box.
   */
  minFillRatio: 0.9,
  maxFillRatio: 1.0 + 1e-9,
  /**
   * Edge clearance floor. 8px is half the smallest default gutter, so a
   * consumer may halve `DEFAULT_PLOT_INSETS` and still pass, while a series
   * touching the clip boundary — the live chart's seven flat-topped candles —
   * cannot.
   */
  minEdgeClearancePx: 8,
} as const;

/** A tier-2 verdict: `pass`, plus every bound that was missed and by how much. */
export interface Tier2Verdict {
  pass: boolean;
  failures: string[];
}

/** Score `FramingMetrics` against `TIER2_FRAMING_BOUNDS`. */
export function checkTier2Framing(
  m: FramingMetrics,
  bounds: typeof TIER2_FRAMING_BOUNDS = TIER2_FRAMING_BOUNDS,
): Tier2Verdict {
  const failures: string[] = [];
  if (!(m.deadMarginFrac <= bounds.maxDeadMarginFrac)) {
    failures.push(
      `dead margin ${(m.deadMarginFrac * 100).toFixed(1)}% > ${(bounds.maxDeadMarginFrac * 100).toFixed(1)}%`,
    );
  }
  if (!(m.fillRatio >= bounds.minFillRatio && m.fillRatio <= bounds.maxFillRatio)) {
    failures.push(
      `fill ratio ${m.fillRatio.toFixed(3)} outside [${bounds.minFillRatio}, ${bounds.maxFillRatio.toFixed(3)}]`,
    );
  }
  if (!(m.minEdgeClearancePx >= bounds.minEdgeClearancePx)) {
    failures.push(
      `edge clearance ${m.minEdgeClearancePx.toFixed(1)}px < ${bounds.minEdgeClearancePx}px ` +
        `(top ${m.edgeClearancePx.top.toFixed(1)}, right ${m.edgeClearancePx.right.toFixed(1)}, ` +
        `bottom ${m.edgeClearancePx.bottom.toFixed(1)}, left ${m.edgeClearancePx.left.toFixed(1)})`,
    );
  }
  return { pass: failures.length === 0, failures };
}

/**
 * The one-call path: measured domain + canvas → the transform to hand the
 * engine, the box it was fitted to, and the tier-2 score of the result.
 * Returns `null` for the transform when the domain states neither axis.
 */
export function frameSeries(
  domain: { x: Range | null; y: Range | null },
  canvas: CanvasSize,
  insets: PlotInsets = DEFAULT_PLOT_INSETS,
  policy: FitPolicy = {},
): { box: PlotBox; transform: Transform2D | null; metrics: FramingMetrics | null } {
  const box = plotBox(canvas, insets);
  if (!domain.x && !domain.y) return { box, transform: null, metrics: null };
  const transform = fitToPlotBox(domain, box, policy);
  const metrics =
    domain.x && domain.y
      ? framingMetrics({ x: domain.x, y: domain.y }, transform, box, canvas)
      : null;
  return { box, transform, metrics };
}
