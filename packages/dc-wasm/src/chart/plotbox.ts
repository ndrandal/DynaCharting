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
 *      `pxSpanToClipX`/`pxSpanToClipY` rather than re-deriving `2/width`.
 *      Those are SPAN conversions; `text.ts`'s `pxToClipX`/`pxToClipY` are
 *      POSITION conversions and are not interchangeable — see the note above
 *      them.
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
 *   7. THE BOX IS ALSO THE DATA PANE'S `PaneRegion`, AND THE TWO MUST NOT
 *      DISAGREE. The engine already has a clip-space rectangle per pane
 *      (`dc::PaneRegion`, `core/include/dc/layout/PaneLayout.hpp`), applied as a
 *      scissor by `DawnSceneRenderer` and settable at runtime via the
 *      `setPaneRegion` command. What it has never had is anything tying that
 *      rectangle to the transform projecting into it — `CHART_AUTHORING.md`
 *      §"Do not reserve layout margins…" describes authors shrinking the two by
 *      hand, independently, which is the same class of mistake as a hand-typed
 *      axis. `paneRegionFor(box)` derives the pane rectangle from the SAME box
 *      the transform was fitted to, so they cannot drift.
 *
 *      Consequence ENC-1253 must plan for: with the data pane scissored to the
 *      box, axis furniture drawn in the gutters needs a pane whose region is
 *      wider than the box (`FULL_CLIP_REGION` is provided for exactly that).
 *      Furniture in the data pane is scissored away, silently.
 *
 *      Second consequence, worth having on purpose: the scissor BOUNDS the
 *      damage of a stale or wrong domain. Geometry can be mis-scaled, but it
 *      cannot paint over the axis furniture or run off the canvas. That is a
 *      floor, not the fix — a fit that needs the scissor is already failing
 *      `checkTier2Framing`.
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

/*
 * SPAN helpers, not POSITION helpers — and `text.ts` exports the other pair
 * under names one letter away, so read this before importing both (ENC-1253
 * will).
 *
 *   plotbox.ts  pxSpanToClipX(64, canvas)  -> 0.1   a LENGTH: 64px is 0.1 clip
 *   text.ts     pxToClipX(64, width)       -> -0.9  a POINT: pixel column 64
 *
 * A span has no origin and no Y flip; a position has both. Using one where the
 * other belongs is off by exactly `-1` on X and mirrored on Y, which renders as
 * "the axis is in the wrong half of the chart" rather than as an error.
 */

/** Clip-space length of `px` horizontal CSS pixels on a `canvas`-wide target. */
export function pxSpanToClipX(px: number, canvas: CanvasSize): number {
  return (2 * px) / canvas.width;
}

/** Clip-space length of `px` vertical CSS pixels on a `canvas`-tall target. */
export function pxSpanToClipY(px: number, canvas: CanvasSize): number {
  return (2 * px) / canvas.height;
}

/** CSS-pixel length of a clip-space horizontal SPAN. Inverse of pxSpanToClipX. */
export function clipSpanToPxX(clip: number, canvas: CanvasSize): number {
  return (clip * canvas.width) / 2;
}

/** CSS-pixel length of a clip-space vertical SPAN. Inverse of pxSpanToClipY. */
export function clipSpanToPxY(clip: number, canvas: CanvasSize): number {
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

/** Why a canvas has no plot box. One per condition `tryPlotBox` checks. */
export type PlotBoxRefusal =
  /** `width`/`height` non-finite, zero or negative. */
  | "canvas-not-positive"
  /** An inset is non-finite or negative. */
  | "inset-not-finite"
  /** `left + right >= width`: the price band alone is wider than the canvas. */
  | "horizontal-insets-exceed-canvas"
  /** `top + bottom >= height`. */
  | "vertical-insets-exceed-canvas";

/**
 * `tryPlotBox`'s answer: the box, or the named reason there is none.
 *
 * Shaped like `apps/showcase/src/views/framing.ts`'s `FramingResolution` on
 * purpose — this repo already has an idiom for "declined, and here is why", and
 * a `reason` a caller can switch on plus a `detail` a human can read is it. It
 * is deliberately NOT `PlotBox | null`: a null is a refusal a caller can forget
 * to handle and then render nothing for, which is the failure mode this whole
 * file's contract notes exist to prevent one layer down.
 */
export type PlotBoxResolution =
  | { fits: true; box: PlotBox }
  | { fits: false; reason: PlotBoxRefusal; detail: string };

/**
 * The clip-space plot box for `canvas`, or the reason it has none.
 *
 * CALL THIS — not `plotBox()` — FROM ANYWHERE THAT DOES NOT CHOOSE ITS OWN
 * CANVAS SIZE. A React effect, a `ResizeObserver` callback and a resize handler
 * are all handed whatever the DOM currently reports, and what the DOM reports
 * during mount is **1x1**: a canvas's backing store is bootstrapped at
 * `Math.max(1, round(clientWidth * dpr))` before layout has run. `64 + 16 >= 1`,
 * so `plotBox()` throws on it — and a throw out of a passive effect is a React
 * UNMOUNT of everything up to the nearest error boundary. At DynaCharting
 * `1e125e3` that took the entire showcase down on a deep link to **11 of its 22
 * views**, 3/3 cold loads each, and the only visible symptom was a white page
 * (ENC-1313; LIMITATIONS.md DC-L-1313; re-check
 * `specs/2026-09-19-chart-quality-bar/harness/deeplink-crash.mjs`).
 *
 * A degenerate canvas is a normal transient during mount, not a programming
 * error, and this function is the half of the contract that says so. The other
 * half — `plotBox()` — is unchanged and still throws, because a caller that
 * BUILT its canvas asking for a box it cannot have IS a programming error. The
 * distinction is not "throw vs null", it is **who chose the size**.
 *
 * Whichever you call, DECLINE VISIBLY. A `fits: false` that is silently turned
 * into "draw nothing" is indistinguishable from "there was nothing to draw",
 * and that is how §1.3's caption axes went unnoticed for six months.
 */
export function tryPlotBox(
  canvas: CanvasSize,
  insets: PlotInsets = DEFAULT_PLOT_INSETS,
): PlotBoxResolution {
  const { width, height } = canvas;
  if (!(Number.isFinite(width) && Number.isFinite(height) && width > 0 && height > 0)) {
    return {
      fits: false,
      reason: "canvas-not-positive",
      detail: `plotBox: canvas must be positive and finite, got ${width}x${height}`,
    };
  }
  for (const side of ["top", "right", "bottom", "left"] as const) {
    const v = insets[side];
    if (!(Number.isFinite(v) && v >= 0)) {
      return {
        fits: false,
        reason: "inset-not-finite",
        detail: `plotBox: inset.${side} must be finite and >= 0, got ${v}`,
      };
    }
  }
  if (insets.left + insets.right >= width) {
    return {
      fits: false,
      reason: "horizontal-insets-exceed-canvas",
      detail: `plotBox: horizontal insets (${insets.left}+${insets.right}) leave no plot box in ${width}px`,
    };
  }
  if (insets.top + insets.bottom >= height) {
    return {
      fits: false,
      reason: "vertical-insets-exceed-canvas",
      detail: `plotBox: vertical insets (${insets.top}+${insets.bottom}) leave no plot box in ${height}px`,
    };
  }
  return {
    fits: true,
    box: {
      x: {
        min: CLIP_RANGE.min + pxSpanToClipX(insets.left, canvas),
        max: CLIP_RANGE.max - pxSpanToClipX(insets.right, canvas),
      },
      // Screen-top is clip +Y: `insets.top` comes off `y.max`.
      y: {
        min: CLIP_RANGE.min + pxSpanToClipY(insets.bottom, canvas),
        max: CLIP_RANGE.max - pxSpanToClipY(insets.top, canvas),
      },
    },
  };
}

/**
 * The clip-space plot box for `canvas` with `insets` reserved for furniture.
 *
 * Throws `PlotBoxError` if the gutters do not leave a positive-area box, or if
 * any input is non-finite/negative. That is the contract for a caller that OWNS
 * its canvas — a builder, a test, a fixed-size export — and it is unchanged:
 * same four conditions, same messages. Silently clamping would hand back an
 * inverted or zero-width box and let a downstream `fitAxis` produce a negative
 * or infinite scale.
 *
 * If the size was handed to you rather than chosen by you, call `tryPlotBox`
 * instead and read its header for why (ENC-1313).
 *
 * This is `tryPlotBox` plus a throw, and nothing else, so the two can never
 * disagree about which canvases are legal.
 */
export function plotBox(canvas: CanvasSize, insets: PlotInsets = DEFAULT_PLOT_INSETS): PlotBox {
  const resolution = tryPlotBox(canvas, insets);
  if (!resolution.fits) throw new PlotBoxError(resolution.detail);
  return resolution.box;
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

/**
 * A pane rectangle in the shape the engine's `setPaneRegion` command and
 * `SceneBuilder.pane({ region })` take (`dc::PaneRegion`,
 * `core/include/dc/layout/PaneLayout.hpp`). Clip space, +Y up.
 */
export interface PaneRegion {
  clipXMin: number;
  clipXMax: number;
  clipYMin: number;
  clipYMax: number;
}

/** The engine's default pane region: the whole clip space. Furniture panes. */
export const FULL_CLIP_REGION: Readonly<PaneRegion> = {
  clipXMin: CLIP_RANGE.min,
  clipXMax: CLIP_RANGE.max,
  clipYMin: CLIP_RANGE.min,
  clipYMax: CLIP_RANGE.max,
};

/**
 * The data pane's region for `box` — contract note (7). Give this to
 * `setPaneRegion` on the same pane whose draw items carry the transform
 * `fitToPlotBox` returned, and the scissor and the fit describe one rectangle
 * instead of two that happen to agree today.
 */
export function paneRegionFor(box: PlotBox): PaneRegion {
  return {
    clipXMin: box.x.min,
    clipXMax: box.x.max,
    clipYMin: box.y.min,
    clipYMax: box.y.max,
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
  /**
   * 1 − (plot box area ÷ CANVAS area) — the share of `deadMarginFrac` that is
   * RESERVED gutter rather than unfilled plot. Reported, not gated: it is a
   * function of the canvas size and the insets, not of the framing.
   */
  gutterFrac: number;
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
    left: clipSpanToPxX(ink.x.min - CLIP_RANGE.min, canvas),
    right: clipSpanToPxX(CLIP_RANGE.max - ink.x.max, canvas),
    bottom: clipSpanToPxY(ink.y.min - CLIP_RANGE.min, canvas),
    top: clipSpanToPxY(CLIP_RANGE.max - ink.y.max, canvas),
  };

  return {
    ink,
    deadMarginFrac: 1 - (inkW * inkH) / canvasArea,
    gutterFrac: 1 - boxArea / canvasArea,
    fillRatio: boxArea > 0 ? (inkW * inkH) / boxArea : Number.POSITIVE_INFINITY,
    edgeClearancePx,
    minEdgeClearancePx: Math.min(
      edgeClearancePx.top,
      edgeClearancePx.right,
      edgeClearancePx.bottom,
      edgeClearancePx.left,
    ),
    inkPx: { width: clipSpanToPxX(inkW, canvas), height: clipSpanToPxY(inkH, canvas) },
  };
}

/**
 * The stated tier-2 thresholds. D1 requires the numbers be stated, not felt;
 * these are the numbers, with the derivation beside each so the next person
 * argues with the reasoning rather than with the digit.
 */
export const TIER2_FRAMING_BOUNDS = {
  /**
   * Dead-margin ceiling, measured against the canvas.
   *
   * DERIVED, and the derivation is the interesting part: because gutters are a
   * fixed number of PIXELS, what they cost as a FRACTION depends on the canvas.
   * `DEFAULT_PLOT_INSETS` spends 10.9% of a 1280x800 canvas and 30.7% of a
   * 400x300 one — and on a 400px-wide chart, spending 20% of the width on the
   * price labels is correct, not wasteful. A ceiling tight enough for the big
   * canvas would therefore condemn a perfectly framed small one.
   *
   * So the ceiling is set just above the small-canvas cost: 0.37 admits a
   * correctly fitted chart anywhere from 400x300 up, and still condemns the
   * live chart's measured 66.4% by a wide margin (and a half-scale fit, which
   * scores 77.7%). `gutterFrac` is reported alongside so a reading near this
   * bound can be attributed to the canvas rather than to the framing — and
   * `minFillRatio` is the size-independent half of the same question.
   *
   * An earlier draft of this constant was 0.25 and was wrong: it failed a
   * correctly fitted 400x300 chart at 30.7%. The test that found it is
   * `frameSeries > scores tier-2 green for any canvas ...`.
   */
  maxDeadMarginFrac: 0.37,
  /**
   * Fill band — the canvas-size-independent half. A correct fit puts the ink
   * bbox exactly on the box, i.e. 1.0. The floor is what makes this a check
   * rather than a tautology: a baked literal transform (`candles-aapl`,
   * DC-L13) fills ~0.56 of its box and fails; the live NEXO framing fills 0.38.
   * The ceiling catches a fit that overflowed the box.
   */
  minFillRatio: 0.9,
  maxFillRatio: 1.0 + 1e-9,
  /**
   * Edge-clearance floor, in CSS pixels. 4px is two 2px-wide antialiased line
   * caps: below it a mark reads as resting on the frame rather than inside it.
   * It is deliberately BELOW the smallest default gutter (top, 12px) so a
   * caller may halve `DEFAULT_PLOT_INSETS` — gutters of 6/8/14/32 — and still
   * pass, while a series touching the clip boundary (the live chart's seven
   * flat-topped candles, clearance 0) cannot.
   *
   * An earlier draft said 8px and justified it as "half the smallest default
   * gutter", which is arithmetic nobody checked: half of 12 is 6, and the
   * halve-the-defaults case failed. `insets are a knob, not a constant` is the
   * test that found it.
   */
  minEdgeClearancePx: 4,
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

/** Everything a caller needs to frame one series. See `frameSeries`. */
export interface FramedSeries {
  /** The clip-space plot box the data was fitted into. */
  box: PlotBox;
  /** The data pane's region — contract note (7). Always present. */
  paneRegion: PaneRegion;
  /** The transform to `setTransform`, or null when the domain states nothing. */
  transform: Transform2D | null;
  /** The tier-2 score, or null when either axis is unstated. */
  metrics: FramingMetrics | null;
}

/**
 * The one-call path: measured domain + canvas → the transform to hand the
 * engine, the pane region to hand it alongside, the box both were derived
 * from, and the tier-2 score of the result.
 *
 * ```ts
 * const framed = frameSeries(tracker.domain(), { width: canvas.clientWidth, height: canvas.clientHeight });
 * if (framed.transform) host.applyControl({ cmd: 'setTransform', id: TRANSFORM, ...framed.transform });
 * host.applyControl({ cmd: 'setPaneRegion', id: PANE, ...framed.paneRegion });
 * ```
 *
 * `transform` is null when the domain states neither axis — a chart with no
 * data does not get a frame invented for it.
 */
export function frameSeries(
  domain: { x: Range | null; y: Range | null },
  canvas: CanvasSize,
  insets: PlotInsets = DEFAULT_PLOT_INSETS,
  policy: FitPolicy = {},
): FramedSeries {
  const box = plotBox(canvas, insets);
  const paneRegion = paneRegionFor(box);
  if (!domain.x && !domain.y) return { box, paneRegion, transform: null, metrics: null };
  const transform = fitToPlotBox(domain, box, policy);
  const metrics =
    domain.x && domain.y
      ? framingMetrics({ x: domain.x, y: domain.y }, transform, box, canvas)
      : null;
  return { box, paneRegion, transform, metrics };
}
