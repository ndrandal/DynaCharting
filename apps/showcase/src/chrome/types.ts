/* apps/showcase/src/chrome/types.ts
 *
 * The CHROME SCHEMA (ENC-562). The optional `chrome` block on a view's
 * `view.json` declares its 'logical chart' chrome — axes (gridlines + tick
 * labels), a legend, and (for heatmaps) a colorbar. The showcase renders these
 * as a crisp HTML/SVG OVERLAY positioned over the WebGPU canvas (never in-engine
 * text). The overlay maps data→clip→pixel the SAME way the engine does, driven
 * by the view's BAKED `transform` (sx/sy/tx/ty) plus the static data ranges
 * declared here, so ticks/gridlines align with the rendered geometry.
 *
 * The showcase-explicit replay path has no RangeTracker, so framing is baked per
 * view → the axis ranges below are static and the static chrome is correct.
 *
 * ── Per-view agents: how to add chrome ────────────────────────────────────────
 * Add a `chrome` object to your view.json. Set whichever sub-blocks apply:
 *   • cartesian charts (candles, lines, bars, scatter) → `axes` (+ `legend`)
 *   • heatmaps / textured quads (correlation, density, spectrogram) → `colorbar`
 *   • categorical fills (treemap, sankey) → `legend` only
 * Every sub-block is optional. The axis `min`/`max` are DATA-SPACE values; the
 * overlay maps them through the baked `transform` to land ticks exactly where
 * the engine draws that value. See the candles-aapl (axes + legend) and
 * correlation-heatmap (colorbar) reference views.
 */

/** RGBA in 0..1 floats (matches the engine's setDrawItemStyle colors) OR a CSS
 *  hex string ('#3ddc84'). Either is accepted everywhere a color is taken. */
export type RGBA = [number, number, number, number];
export type ColorInput = RGBA | string;

/** How an axis's tick VALUES are formatted into labels. */
export type AxisFormat =
  | 'price' /** "$418.00" — currency, 2dp */
  | 'time' /** "0:12" — elapsed mm:ss (value = seconds) */
  | 'index' /** "162" — integer record index */
  | 'number' /** "1.25" — plain number, adaptive precision */
  | 'percent' /** "+42%" — value*100 with sign */;

/**
 * One axis (x or y). `min`/`max` are DATA-SPACE bounds (e.g. price 405..421, or
 * record-index 4..162). The overlay maps them through the view's baked
 * `transform` to pixels, then lays `ticks` evenly across [min,max]. Gridlines
 * (when `grid`) and tick labels are drawn at those data values, so they align
 * with the rendered geometry.
 */
export interface AxisSpec {
  /** Axis title (e.g. "Price", "Time"). Optional. */
  label?: string;
  /** Data-space lower bound (maps via the transform to a pixel). */
  min: number;
  /** Data-space upper bound. */
  max: number;
  /** Tick-label formatting. */
  format: AxisFormat;
  /** Number of tick intervals (ticks = this+1 labels). Default 5. */
  ticks?: number;
  /** Draw gridlines across the plot at each tick. Default true. */
  grid?: boolean;
}

/** What a legend swatch depicts (drives the swatch glyph shape). */
export type LegendKind = 'line' | 'area' | 'candle' | 'bar' | 'point' | 'swatch';

/** One legend entry: a labeled colored swatch. */
export interface LegendItem {
  label: string;
  color: ColorInput;
  /** Swatch glyph; default 'swatch' (a filled square). */
  kind?: LegendKind;
}

/** A single colorbar gradient stop. `at` is 0..1 along the bar (0 = min). */
export interface ColorbarStop {
  at: number;
  color: ColorInput;
}

/**
 * A colorbar for heatmaps / textured-quad views: a vertical gradient strip with
 * a min/max scale + label. `stops` define the gradient (matching the colormap
 * baked into the texture); `min`/`max` are the scale's data values.
 */
export interface ColorbarSpec {
  label?: string;
  min: number;
  max: number;
  stops: ColorbarStop[];
  /** Optional explicit categorical tick labels along the heatmap's own axes
   *  (e.g. symbol names for a correlation matrix). Rendered as evenly-spaced
   *  labels along the bottom (x) / left (y) of the plot. */
  categories?: { x?: string[]; y?: string[] };
}

/** The full `chrome` block on a view.json. All fields optional. */
export interface ChromeSpec {
  axes?: { x?: AxisSpec; y?: AxisSpec };
  legend?: LegendItem[];
  colorbar?: ColorbarSpec;
}
