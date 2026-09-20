/* apps/showcase/src/chrome/engineAxis.test.ts — ENC-1313
 *
 * THE SEAM THAT USED TO TAKE THE APP DOWN.
 *
 * `engineAxisSpec` is called from a React passive effect (`useEngineAxis`) with
 * whatever canvas size the DOM currently reports. At `1e125e3` it called
 * `plotBox(canvas, DEFAULT_PLOT_INSETS)` on that size unguarded, and
 * `ShowcaseEngine.sizeCanvas` publishes `Math.max(1, round(clientWidth * dpr))`
 * — i.e. **1x1** — for one commit before layout runs. `plotBox()` throws on 1px,
 * the throw escaped the effect, nothing above `<ChromeOverlay>` was an error
 * boundary, and React unmounted `<App>`: `#root` empty, no canvas, white page.
 * ENC-1262 measured it at **11 of the 22 showcase views, 3/3 cold loads each**
 * (`specs/2026-09-19-chart-quality-bar/harness/deeplink-crash.mjs`).
 *
 * The eleven were exactly the views whose axes resolve on the FIRST commit — a
 * literal `min`/`max`, with no `TimeBasis` to fit first. The reference chart
 * (`candles-aapl`) is a `timestamp` view and was structurally in the surviving
 * half, so verifying ENC-1253 and ENC-1273 on it could not have found this. The
 * cases below are therefore written as the 1px canvas, not as a view id: what
 * has to hold is a property of the SEAM, and pinning it to any particular view
 * would reproduce the sampling mistake that hid it.
 *
 * THE ASSERTION THAT MATTERS is the second describe block's: a 1px canvas is
 * HANDLED — named, reported, declined — rather than avoided by arriving late.
 */

import { describe, it, expect } from 'vitest';
import { DEFAULT_PLOT_INSETS, plotBox } from '@repo/dc-wasm';
import { engineAxisSpec } from './engineAxis';
import type { ResolvedAxes } from './deriveAxes';
import type { EffectiveTransform } from './mapping';

const TRANSFORM: EffectiveTransform = { sx: 1, tx: 0, sy: 1, ty: 0 };

/**
 * A view that states its domain synchronously — the shape of all eleven: a
 * literal `min`/`max` with no `TimeBasis` to fit first, so `deriveAxes` resolves
 * it on the FIRST commit, while the canvas is still 1x1.
 */
const LITERAL_AXES: ResolvedAxes = {
  x: { label: 'Index', min: 0, max: 100, format: 'index', ticks: 6 },
  y: { label: 'Price', min: 406, max: 418, format: 'price', ticks: 6 },
};

describe('engineAxisSpec — a canvas that can carry a plot box', () => {
  it('returns a spec, and no refusal, for a laid-out canvas', () => {
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, { width: 1280, height: 800 }, null);
    expect(r.refusal).toBeNull();
    expect(r.spec).not.toBeNull();
    // The default box for this canvas, since no fitted box was passed.
    expect(r.spec!.box).toEqual(plotBox({ width: 1280, height: 800 }, DEFAULT_PLOT_INSETS));
    expect(r.spec!.x!.ticks.length).toBeGreaterThan(0);
    expect(r.spec!.y!.ticks.length).toBeGreaterThan(0);
  });

  it('uses the FITTED box when one is given, rather than re-deriving one', () => {
    const fitted = plotBox({ width: 1280, height: 800 }, { top: 40, right: 40, bottom: 40, left: 40 });
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, { width: 1280, height: 800 }, null, undefined, fitted);
    expect(r.spec!.box).toBe(fitted);
  });
});

describe('engineAxisSpec — the 1px bootstrap canvas (ENC-1313)', () => {
  // The exact size `ShowcaseEngine.sizeCanvas` publishes before layout.
  const BOOTSTRAP = { width: 1, height: 1 };

  it('DOES NOT THROW on the 1px canvas that unmounted the app', () => {
    // The whole regression in one line. Before ENC-1313 this threw
    // `PlotBoxError: plotBox: horizontal insets (64+16) leave no plot box in 1px`
    // out of a passive effect, which React turns into an unmounted tree.
    expect(() => engineAxisSpec(LITERAL_AXES, TRANSFORM, BOOTSTRAP, null)).not.toThrow();
  });

  it('declines VISIBLY: no spec, and a named reason carrying the measurement', () => {
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, BOOTSTRAP, null);
    expect(r.spec).toBeNull();
    // Not a bare null. `useEngineAxis` publishes this on
    // `window.__dcEngineAxis[viewId].refusal` and `ChromeOverlay` puts it in the
    // DOM, so "no axis" is a state the page can be ASKED about rather than one
    // that looks identical to "no axis was wanted".
    expect(r.refusal).not.toBeNull();
    expect(r.refusal!.reason).toBe('plot-box-refused');
    expect(r.refusal!.detail).toContain('leave no plot box in 1px');
  });

  it('declines for a too-SHORT canvas as well, naming the vertical side', () => {
    // 1px is only the instance this shipped. A wide, 20px-tall canvas is the
    // other half of the same condition and must not reach a throw either.
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, { width: 1280, height: 20 }, null);
    expect(r.spec).toBeNull();
    expect(r.refusal!.reason).toBe('plot-box-refused');
    expect(r.refusal!.detail).toContain('vertical insets');
  });

  it('DRAWS on the narrowest canvas the default gutters do fit, so the guard is a bound and not a blanket', () => {
    // 64 + 16 = 80: 81px wide (and 41px tall, past 12 + 28) is the first canvas
    // that fits. A guard that refused this too would be indistinguishable from
    // one that refused everything.
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, { width: 81, height: 41 }, null);
    expect(r.refusal).toBeNull();
    expect(r.spec).not.toBeNull();
  });

  it('passes the refusal through even when a fitted box WOULD have worked', () => {
    // `box` short-circuits the `plotBox()` call, so this case checks the canvas
    // guard is not skipped by a caller that happens to supply a box: a 1px
    // canvas cannot carry axis furniture whatever rectangle you hand it, and
    // `planAxis` measures its labels in that canvas's pixels.
    const fitted = plotBox({ width: 1280, height: 800 }, DEFAULT_PLOT_INSETS);
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, BOOTSTRAP, null, undefined, fitted);
    expect(r.spec).toBeNull();
    expect(r.refusal!.reason).toBe('canvas-not-sized');
  });
});

describe('engineAxisSpec — the other ways there is nothing to draw', () => {
  it('names the unsized canvas rather than returning a bare null', () => {
    const r = engineAxisSpec(LITERAL_AXES, TRANSFORM, { width: 0, height: 0 }, null);
    expect(r.spec).toBeNull();
    expect(r.refusal!.reason).toBe('canvas-not-sized');
  });

  it('names the eight views that state no domain at all', () => {
    // `deriveAxes`: "a chart that cannot state a domain states no axis". That is
    // the NORMAL state for contour, treemap, sankey &c — a refusal, not a fault.
    const r = engineAxisSpec({}, TRANSFORM, { width: 1280, height: 800 }, null);
    expect(r.spec).toBeNull();
    expect(r.refusal!.reason).toBe('no-axes-resolved');
  });
});
