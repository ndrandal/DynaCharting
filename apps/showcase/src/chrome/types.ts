/* apps/showcase/src/chrome/types.ts
 *
 * The CHROME SCHEMA (ENC-562). The optional `chrome` block on a view's
 * `view.json` declares its 'logical chart' chrome — axes (gridlines + tick
 * labels), a legend, and (for heatmaps) a colorbar. The showcase renders these
 * as a crisp HTML/SVG OVERLAY positioned over the WebGPU canvas (never in-engine
 * text). The overlay maps data→clip→pixel the SAME way the engine does, driven
 * by the view's BAKED `transform` (sx/sy/tx/ty) plus the axis DOMAIN, so
 * ticks/gridlines align with the rendered geometry.
 *
 * WHERE THE DOMAIN COMES FROM (ENC-1252, chart-quality-bar SPEC D7).
 * An axis's `min`/`max` are OPTIONAL. Omit them and the domain is MEASURED from
 * the records the view streams, by a `DomainTracker` folding the same dataplane
 * bytes the engine ingests (see `deriveAxes.ts` and the view manifest's
 * `axisDomain` export). A view that still hard-codes them renders a caption, not
 * a measurement — if its geometry drifted, the number would not move (SPEC
 * §1.3). The views that still hard-code a range are the backlog, not the
 * pattern.
 *
 * ── Per-view agents: how to add chrome ────────────────────────────────────────
 * Add a `chrome` object to your view.json. Set whichever sub-blocks apply:
 *   • cartesian charts (candles, lines, bars, scatter) → `axes` (+ `legend`)
 *   • heatmaps / textured quads (correlation, density, spectrogram) → `colorbar`
 *   • categorical fills (treemap, sankey) → `legend` only
 * Every sub-block is optional. An axis's domain is DATA-SPACE; the overlay maps
 * it through the baked `transform` to land ticks exactly where the engine draws
 * that value. Prefer to OMIT `min`/`max` and export an `axisDomain` from your
 * manifest.ts instead, so the domain is measured from your own streamed records
 * (D7). See the candles-aapl (derived axes + legend) and correlation-heatmap
 * (colorbar) reference views.
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
 * One axis (x or y). Its DOMAIN is a pair of data-space bounds (e.g. price
 * 405..421, or record-index 4..162). The overlay maps them through the view's
 * baked `transform` to pixels, then lays `ticks` evenly across the domain.
 * Gridlines (when `grid`) and tick labels are drawn at those data values, so
 * they align with the rendered geometry.
 *
 * `min`/`max` are OPTIONAL (ENC-1252 / D7): leave them out and the domain is
 * measured from the streamed data instead. Setting them pins the axis to a
 * literal — which is the thing D7 removes — so do it only for a view whose
 * `axisDomain` cannot yet be declared.
 */
export interface AxisSpec {
  /** Axis title (e.g. "Price", "Time"). Optional. */
  label?: string;
  /** Data-space lower bound. Omit to measure it from the streamed data. */
  min?: number;
  /** Data-space upper bound. Omit to measure it from the streamed data. */
  max?: number;
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
