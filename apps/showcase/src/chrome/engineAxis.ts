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
 * 1. THE PLOT BOX IS NOW WHAT THE DATA IS FITTED TO — for a view `framing.ts`
 *    accepts (ENC-1273). `useViewSwitch` fits the MEASURED domain into the box
 *    and passes the very `PlotBox` it used down as `box`, so the furniture and
 *    the data are laid out against one rectangle by construction rather than by
 *    two `plotBox(canvas)` calls happening to agree. `box` is null for a view
 *    that is not framed (no `axisDomain`, or a stacked multi-pane layout —
 *    LIMITATIONS.md **DC-L-1273**), and the default below is then the frame the
 *    furniture is laid out in while the data stays on its baked literal, which
 *    is the state ENC-1253 shipped and DC-L14 described: data drawn left of the
 *    box's left edge runs under the price labels. The MARKS were never wrong —
 *    a tick is projected through the same transform as the geometry — it was
 *    the FRAMING.
 *
 * 2. AN AXIS IS ONLY DRAWN WHERE ONE IS RESOLVED. A view with no `chrome.axes`,
 *    or a `timestamp` axis with no fitted `TimeBasis`, resolves to nothing and
 *    gets no furniture — the same rule `deriveAxes` applies to the overlay. A
 *    chart that cannot state a domain states no axis; it does not get one
 *    invented for it.
 *
 * ── WHY THIS FUNCTION RETURNS A REFUSAL AND NOT A NULL (ENC-1313) ───────────
 *
 * This is called from a React PASSIVE EFFECT with whatever canvas size the DOM
 * currently reports, and during mount that is **1x1** —
 * `ShowcaseEngine.sizeCanvas` bootstraps the backing store at
 * `Math.max(1, round(clientWidth * dpr))` and publishes it for one commit before
 * layout runs. The `canvas.width > 0` guard below passes on 1, and the
 * unconditional `plotBox(canvas, DEFAULT_PLOT_INSETS)` that used to follow then
 * threw `PlotBoxError: … leave no plot box in 1px`. A throw out of a passive
 * effect unmounts everything up to the nearest error boundary, and there was no
 * error boundary, so it emptied `#root`: **a deep link to 11 of the 22 showcase
 * views took the whole app down**, 3/3 cold loads each at `1e125e3` (ENC-1262
 * found it; `harness/deeplink-crash.mjs` is the re-check).
 *
 * The eleven were exactly the views whose axes resolve on the FIRST commit — a
 * literal `min`/`max`, no `TimeBasis` to fit first. `candles-aapl`, the
 * reference chart, is a `timestamp` view, so it resolves after layout and was
 * structurally in the surviving half. **ENC-1253 and ENC-1273 were both
 * verified on it, and neither could have caught this.** That is the negative
 * transfer SPEC §5 Q4 exists to surface.
 *
 * So the four "nothing to draw" paths are now NAMED rather than nulled.
 * `useEngineAxis` publishes the refusal on `window.__dcEngineAxis[viewId]` and
 * `ChromeOverlay` puts it in the DOM, because a silent null is how this
 * project's other defects started: "declined to draw an axis" and "was never
 * asked for an axis" have to be different observable states, or §1.3's captions
 * happen again.
 */

import {
  DEFAULT_PLOT_INSETS,
  darkAxisTheme,
  tryPlotBox,
  type AxisGridTarget,
  type AxisSpec,
  type AxisTextMeasurer,
  type AxisTheme,
  type AxisTick,
  type CanvasSize,
  type PlotBox,
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
 * Why no axis spec was built. Every path that declines names itself (ENC-1313).
 *
 * Only `plot-box-refused` and `canvas-not-sized` are transients of mounting;
 * `no-axes-resolved` is the steady state of the eight views that declare no
 * `chrome.axes`, and `no-ticks` of a domain too degenerate to tick. None of them
 * is an error — but all of them have to be *sayable*, because "drew no axis" and
 * "was asked for no axis" looked identical before this and one of them was a
 * crash.
 */
export type EngineAxisRefusalReason =
  /** Neither axis resolved — `deriveAxes` stated no domain (the normal case). */
  | 'no-axes-resolved'
  /** The canvas is not laid out yet, or is too small to carry the gutters. */
  | 'canvas-not-sized'
  /** Axes resolved but produced no ticks (a degenerate domain, no time basis). */
  | 'no-ticks'
  /** `tryPlotBox` refused this canvas: the gutters do not fit in it. */
  | 'plot-box-refused';

/** A named decline, with the measurement that produced it. */
export interface EngineAxisRefusal {
  reason: EngineAxisRefusalReason;
  /** Human-readable, and carries the numbers — e.g. `…no plot box in 1px`. */
  detail: string;
}

/** `engineAxisSpec`'s answer: the spec, or the named reason there is none. */
export type EngineAxisResolution =
  | { spec: AxisSpec; refusal: null }
  | { spec: null; refusal: EngineAxisRefusal };

/**
 * Build the engine-axis spec for a view, or say why there is nothing to draw.
 *
 * NEVER THROWS. See the module header: this runs inside a React passive effect
 * against a canvas that is 1x1 for one commit at mount, and a throw from here
 * unmounts the application.
 *
 * `measurer` may be null while the font is still loading — in that case the
 * geometry (gridlines, ticks, spine) is still planned and the labels are not,
 * because an unmeasured label is a label that might overlap.
 */
export function engineAxisSpec(
  axes: ResolvedAxes,
  transform: EffectiveTransform,
  canvas: CanvasSize,
  measurer: AxisTextMeasurer | null,
  theme: AxisTheme = SHOWCASE_AXIS_THEME,
  box: PlotBox | null = null,
  gridTarget: AxisGridTarget | null = null, // eslint-disable-line
): EngineAxisResolution {
  const decline = (reason: EngineAxisRefusalReason, detail: string): EngineAxisResolution => ({
    spec: null,
    refusal: { reason, detail },
  });

  if (!axes.x && !axes.y) {
    return decline(
      'no-axes-resolved',
      'neither axis resolved a domain — a chart that cannot state a domain states no axis',
    );
  }
  if (!(canvas.width > 0 && canvas.height > 0)) {
    return decline(
      'canvas-not-sized',
      `the canvas is ${canvas.width}x${canvas.height}: not laid out yet`,
    );
  }

  // THE GUARD. `tryPlotBox` is asked about the CANVAS even when a fitted `box`
  // was supplied, because the furniture is measured in this canvas's pixels
  // (`planAxis` sizes every label off `canvas`), so a canvas that cannot carry
  // the gutters cannot carry the labels either — whatever rectangle it is
  // handed. Checking only when `box` is absent would leave the framed views
  // (ENC-1273) on the old path for exactly one commit, which is the window the
  // whole bug lived in.
  const resolved = tryPlotBox(canvas, DEFAULT_PLOT_INSETS);
  if (!resolved.fits) {
    return decline(
      resolved.reason === 'canvas-not-positive' ? 'canvas-not-sized' : 'plot-box-refused',
      resolved.detail,
    );
  }

  const xTicks = ticksFor(axes.x);
  const yTicks = ticksFor(axes.y);
  if (xTicks.length === 0 && yTicks.length === 0) {
    return decline('no-ticks', 'both axes resolved but neither produced a tick');
  }

  return {
    spec: {
      // The box the DATA was fitted into when there is one (ENC-1273);
      // otherwise the default frame for this canvas — the same call, one layer
      // later, which `tryPlotBox` above has already established fits.
      box: box ?? resolved.box,
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
      // THE GRIDLINES GO BEHIND THE DATA when the caller has framed it
      // (ENC-1316). Only a framed view supplies one: its pane region IS the
      // box, so a line spanning the box is not touched by the scissor, and the
      // pane's clear quad is painted before its layers. An unframed view keeps
      // its grid in the furniture pane, on top, where it was.
      gridTarget: (void gridTarget, undefined),
    },
    refusal: null,
  };
}
