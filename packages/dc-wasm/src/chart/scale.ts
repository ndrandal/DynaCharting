/* packages/dc-wasm/src/chart/scale.ts — ENC-699 (G4)
 *
 * Framework-agnostic, dependency-free PURE functions for chart scale / axis /
 * tick / gridline math. Lifted and generalized from the rich-chart research
 * harness (specs/2026-06-20-rich-chart-research/harness/rich_chart.html) where
 * the same algebra was hand-rolled per chart. Consumable by both that harness
 * and customer-layer so the boilerplate (and the sign/range bugs it hides)
 * lives in one tested place.
 *
 * ORIENTATION (read before "fixing" a sign): these helpers use the
 * mathematically correct convention — a HIGHER data value maps to a HIGHER
 * clip-Y, i.e. for a Y axis fitTransform returns a POSITIVE `sy = 2/range`,
 * matching embassy's RangeTracker (range_tracker.go emits sy = 2.0/rng > 0).
 * There IS a confirmed Y-inversion bug in the EngineHost→canvas blit path, but
 * it is fixed CENTRALLY in ENC-696 (G1b) — do NOT compensate for it here. Keep
 * this module orientation-correct.
 */

/** A closed numeric interval [min, max]. */
export interface Range {
  min: number;
  max: number;
}

/**
 * A data-space → clip-space affine map on one axis: clip = s * data + t.
 * These are exactly the `sx,tx` / `sy,ty` fields the engine's Transform
 * (`setTransform`) consumes, hence the field names.
 */
export interface Transform1D {
  /** scale (slope) */
  s: number;
  /** translate (offset) */
  t: number;
}

/**
 * A 2-D data-space → clip-space affine, matching the engine Transform record
 * (`{sx, tx, sy, ty}` from `setTransform`). clipX = sx*x + tx, clipY = sy*y + ty.
 */
export interface Transform2D {
  sx: number;
  tx: number;
  sy: number;
  ty: number;
}

/**
 * A linear scale from a data range to an output range, with `map` and its
 * `invert` inverse. General over the output range (clip space [-1,1], pixels,
 * whatever the caller passes). Pure; holds no state beyond the captured slope.
 *
 * If `dataRange` is degenerate (min === max) the slope is 0 and `map` returns
 * the midpoint of the output range (a sane, non-NaN fallback); `invert` then
 * cannot recover x and returns dataMin.
 */
export interface Scale {
  /** slope: (outMax-outMin)/(dataMax-dataMin), or 0 if data range is degenerate */
  readonly s: number;
  /** intercept: out = s*data + t */
  readonly t: number;
  /** data value → output value */
  map(value: number): number;
  /** output value → data value (inverse of map; returns dataMin if degenerate) */
  invert(output: number): number;
}

/**
 * Build a linear `Scale` mapping `dataRange` onto `outRange`.
 *
 * @param dataRange domain in data space, e.g. {min: priceLow, max: priceHigh}
 * @param outRange  codomain, e.g. clip {min: -1, max: 1} or pixels {min: 0, max: W}
 */
export function scale(dataRange: Range, outRange: Range): Scale {
  const dSpan = dataRange.max - dataRange.min;
  const oSpan = outRange.max - outRange.min;
  const degenerate = dSpan === 0;
  const s = degenerate ? 0 : oSpan / dSpan;
  // out = s*value + t  =>  t = outMin - s*dataMin.
  // When degenerate, map() returns the midpoint of the output range.
  const t = degenerate ? (outRange.min + outRange.max) / 2 : outRange.min - dataRange.min * s;
  return {
    s,
    t,
    map(value: number): number {
      return s * value + t;
    },
    invert(output: number): number {
      if (s === 0) return dataRange.min;
      return (output - t) / s;
    },
  };
}

/**
 * Nice-number tick selection: choose ~`count` "round" tick values spanning
 * [min, max]. Steps snap to 1/2/5 × 10ⁿ. Lifted verbatim (then hardened) from
 * the harness `niceTicks`. Returns ticks within [min, max] inclusive (with a
 * small epsilon on the upper bound), ascending.
 *
 * Edge cases:
 *  - count <= 0 is treated as 1.
 *  - a degenerate or inverted range ([min,max] with min >= max) returns [min].
 *  - non-finite inputs return [].
 */
export function niceTicks(min: number, max: number, count: number): number[] {
  if (!Number.isFinite(min) || !Number.isFinite(max)) return [];
  if (max <= min) return [min];
  const n = count > 0 ? count : 1;
  const span = max - min;
  const rawStep = span / n;
  const mag = Math.pow(10, Math.floor(Math.log10(rawStep)));
  const norm = rawStep / mag;
  const step = (norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10) * mag;
  const out: number[] = [];
  // round-to-6 guards float drift the same way the harness does.
  const start = Math.ceil(min / step) * step;
  for (let v = start; v <= max + 1e-9; v += step) {
    out.push(+v.toFixed(6));
  }
  return out;
}

/**
 * Evenly-spaced integer index ticks across [0, count-1], targeting ~`target`
 * ticks. Used for the time/x axis where the domain is bar indices, not values
 * (harness: `timeStep = max(1, round(N/8))`). Returns indices 0, step, 2·step…
 * within [0, count-1].
 */
export function indexTicks(count: number, target: number): number[] {
  if (count <= 0) return [];
  const t = target > 0 ? target : 1;
  const step = Math.max(1, Math.round(count / t));
  const out: number[] = [];
  for (let i = 0; i < count; i += step) out.push(i);
  return out;
}

/**
 * The clip-space convention used throughout: NDC-style [-1, 1] on both axes,
 * with +Y up. This is the default `clipRange` for `fitTransform`.
 */
export const CLIP_RANGE: Range = { min: -1, max: 1 };

/**
 * Fit a single data axis into a clip (target) range, returning the affine the
 * engine Transform consumes on that axis: clip = s*data + t.
 *
 * For the full NDC range [-1, 1], s = 2/dataSpan (POSITIVE) and t centers the
 * data — i.e. higher data → higher clip. See the ORIENTATION note at the top:
 * this is intentional and matches embassy RangeTracker; the Y-flip lives in the
 * blit (ENC-696), not here.
 */
export function fitAxis(dataRange: Range, clipRange: Range = CLIP_RANGE): Transform1D {
  const sc = scale(dataRange, clipRange);
  return { s: sc.s, t: sc.t };
}

/**
 * Fit a 2-D data range into a 2-D clip range, returning `{sx, tx, sy, ty}` —
 * the exact field shape of an engine `setTransform`. Independent linear fits per
 * axis. `sy` is positive for a normal (min→clipMin, max→clipMax) mapping.
 *
 * Pass distinct `clipRange.y` per pane to stack panes (e.g. price pane in the
 * upper clip band, volume pane in the lower band) while sharing `clipRange.x`.
 *
 * @param dataRange data-space rect {x: Range, y: Range}
 * @param clipRange clip-space target rect {x: Range, y: Range}, defaults to full NDC
 */
export function fitTransform(
  dataRange: { x: Range; y: Range },
  clipRange: { x: Range; y: Range } = { x: CLIP_RANGE, y: CLIP_RANGE },
): Transform2D {
  const fx = fitAxis(dataRange.x, clipRange.x);
  const fy = fitAxis(dataRange.y, clipRange.y);
  return { sx: fx.s, tx: fx.t, sy: fy.s, ty: fy.t };
}

/**
 * Build horizontal gridline segments (one per Y tick) as flat `rect4` floats —
 * `[x0, y0, x1, y1, …]` — in DATA space, to be drawn under a transform via the
 * `lineAA@1` / `instancedRect@1` pipelines (harness `hSeg`). Each line spans the
 * full x extent at a constant y = tick.
 *
 * @param yTicks   tick values (data space), e.g. from niceTicks
 * @param xExtent  the x span the lines cover, in data space
 */
export function horizontalGridSegments(yTicks: number[], xExtent: Range): number[] {
  const out: number[] = [];
  for (const y of yTicks) out.push(xExtent.min, y, xExtent.max, y);
  return out;
}

/**
 * Build vertical gridline segments (one per X tick) as flat `rect4` floats in
 * DATA space (harness `vSeg`). Each line spans the full y extent at a constant
 * x = tick.
 *
 * @param xTicks   tick positions (data space — index ticks or values)
 * @param yExtent  the y span the lines cover, in data space
 */
export function verticalGridSegments(xTicks: number[], yExtent: Range): number[] {
  const out: number[] = [];
  for (const x of xTicks) out.push(x, yExtent.min, x, yExtent.max);
  return out;
}

/**
 * Convenience: build both horizontal and vertical gridline `rect4` float arrays
 * for a data rect from its tick arrays. Returns the two flat arrays plus their
 * segment counts (each segment = 4 floats), which the draw call needs as
 * `count`.
 */
export function gridSegments(opts: {
  xTicks: number[];
  yTicks: number[];
  dataRange: { x: Range; y: Range };
}): {
  horizontal: number[];
  vertical: number[];
  horizontalCount: number;
  verticalCount: number;
} {
  const horizontal = horizontalGridSegments(opts.yTicks, opts.dataRange.x);
  const vertical = verticalGridSegments(opts.xTicks, opts.dataRange.y);
  return {
    horizontal,
    vertical,
    horizontalCount: horizontal.length / 4,
    verticalCount: vertical.length / 4,
  };
}

/**
 * Pad a data range outward by a fraction of its span (harness uses 6% on the
 * price axis so candles/overlays don't touch the pane edge). Returns a new
 * Range; does not mutate.
 */
export function padRange(range: Range, fraction: number): Range {
  const pad = (range.max - range.min) * fraction;
  return { min: range.min - pad, max: range.max + pad };
}
