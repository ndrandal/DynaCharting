/* apps/showcase/src/chrome/engineAxis.ts — ENC-1253 (chart-quality-bar D7, §1.3)
 *
 * THE SEAM WHERE THE AXIS STOPS BEING DOM.
 *
 * `AxisOverlay.tsx` takes the resolved axes (ENC-1252's measured domain) and
 * the ticks (`axisTicks`, ENC-1254) and emits `<svg><line>/<text>`. This module
 * takes the SAME two inputs and hands them to `@repo/dc-wasm`'s `EngineAxis`,
 * which draws them as `lineAA@1` and `textSDF@1` geometry inside the engine's
 * own canvas. Nothing above this file changes: the domain is still measured,
 * the ticks are still chosen by the time ladder, and the projection is still
 * the view's effective transform.
 *
 * That is the whole point of the acceptance criterion. Because both renderers
 * consume one tick list through one transform, deleting the SVG overlay removes
 * a duplicate, not the axis — and a canvas-only capture (SPEC D10), which
 * structurally cannot contain DOM, now has ticks, gridlines, a spine and labels
 * in it.
 *
 * ── TWO HONEST LIMITS OF THIS WIRING, both pointing at other tickets ────────
 *
 * 1. THE PLOT BOX IS NOT (YET) WHAT THE DATA IS FITTED TO. The furniture is laid
 *    out against `plotBox(canvas)` — the tested frame with
 *    `DEFAULT_PLOT_INSETS` gutters — but every showcase view still bakes a
 *    literal `transform` and a hand-written `setPaneRegion` (`±0.95`), so the
 *    data is not fitted to that box. LIMITATIONS.md **DC-L14**; adoption is
 *    **ENC-1273**. The consequence you can see: data drawn left of the box's
 *    left edge runs under the price labels. The MARKS are unaffected — a tick
 *    is projected through the same transform as the geometry, so it sits at the
 *    right data value either way — it is the FRAMING that is still wrong.
 *
 * 2. AN AXIS IS ONLY DRAWN WHERE ONE IS RESOLVED. A view with no `chrome.axes`,
 *    or a `timestamp` axis with no fitted `TimeBasis`, resolves to nothing and
 *    gets no furniture — the same rule `deriveAxes` applies to the overlay. A
 *    chart that cannot state a domain states no axis; it does not get one
 *    invented for it.
 */

import {
  DEFAULT_PLOT_INSETS,
  darkAxisTheme,
  plotBox,
  type AxisSpec,
  type AxisTextMeasurer,
  type AxisTheme,
  type AxisTick,
  type CanvasSize,
} from '@repo/dc-wasm';
import { axisTicks } from './axisTicks';
import type { ResolvedAxes } from './deriveAxes';
import type { EffectiveTransform } from './mapping';

/**
 * The theme the showcase's engine axis is drawn in.
 *
 * `darkTheme()`, mirrored from `core/src/style/Theme.cpp` — the first non-test
 * consumer the theme layer has ever had (SPEC §1.2 counted zero). It is the
 * preset that clears all of D11's contrast bands against the black the render
 * target clears to; `theme.test.ts` asserts those ratios rather than asserting
 * them in prose. See `defaultAxisTheme`'s comment in `@repo/dc-wasm`.
 */
export const SHOWCASE_AXIS_THEME: AxisTheme = darkAxisTheme;

/** Ticks for one resolved axis, in the shape `EngineAxis` takes. */
function ticksFor(spec: Parameters<typeof axisTicks>[0] | undefined): AxisTick[] {
  if (!spec) return [];
  return axisTicks(spec).map((t) => ({ value: t.value, label: t.label }));
}

/**
 * Build the engine-axis spec for a view, or null when there is nothing to draw.
 *
 * `measurer` may be null while the font is still loading — in that case the
 * geometry (gridlines, ticks, spine) is still planned and the labels are not,
 * because an unmeasured label is a label that might overlap. Returns null only
 * when NEITHER axis resolved.
 */
export function engineAxisSpec(
  axes: ResolvedAxes,
  transform: EffectiveTransform,
  canvas: CanvasSize,
  measurer: AxisTextMeasurer | null,
  theme: AxisTheme = SHOWCASE_AXIS_THEME,
): AxisSpec | null {
  if (!axes.x && !axes.y) return null;
  if (!(canvas.width > 0 && canvas.height > 0)) return null;

  const xTicks = ticksFor(axes.x);
  const yTicks = ticksFor(axes.y);
  if (xTicks.length === 0 && yTicks.length === 0) return null;

  return {
    box: plotBox(canvas, DEFAULT_PLOT_INSETS),
    canvas,
    transform,
    x: axes.x
      ? { ticks: xTicks, grid: axes.x.grid ?? true, title: axes.x.label }
      : undefined,
    y: axes.y
      ? { ticks: yTicks, grid: axes.y.grid ?? true, title: axes.y.label }
      : undefined,
    theme,
    measurer: measurer ?? undefined,
  };
}
