/* packages/dc-wasm/src/chart/plotbox.test.ts — ENC-1256
 *
 * The plot box, its gutters, the fit, and the tier-2 verdict.
 *
 * The last describe block is the one that matters: it re-runs the framing
 * failure this ticket exists to fix — the live "NEXO candles" chart measured at
 * `specs/2026-09-19-chart-quality-bar/harness/live-nexo-candles-60s.png` — and
 * asserts that the OLD framing fails `checkTier2Framing` and the NEW one passes.
 * A check that has never been seen to fail is not a check (ENC-1249's lesson,
 * applied one tier up).
 */

import { describe, it, expect } from "vitest";
import {
  DEFAULT_PLOT_INSETS,
  FULL_CLIP_REGION,
  PlotBoxError,
  TIER2_FRAMING_BOUNDS,
  checkTier2Framing,
  clipSpanToPxX,
  clipSpanToPxY,
  fitToPlotBox,
  frameSeries,
  framingMetrics,
  gutters,
  paneRegionFor,
  plotBox,
  pxToClipX,
  pxToClipY,
  type PlotInsets,
} from "./plotbox";

const CANVAS = { width: 1280, height: 800 };

describe("pixel ↔ clip conversion", () => {
  it("maps a pixel span to a clip span on each axis", () => {
    // Full width is 2 clip units.
    expect(pxToClipX(CANVAS.width, CANVAS)).toBeCloseTo(2, 12);
    expect(pxToClipY(CANVAS.height, CANVAS)).toBeCloseTo(2, 12);
    expect(pxToClipX(64, CANVAS)).toBeCloseTo(0.1, 12);
    expect(pxToClipY(28, CANVAS)).toBeCloseTo(0.07, 12);
  });

  it("round-trips", () => {
    expect(clipSpanToPxX(pxToClipX(137, CANVAS), CANVAS)).toBeCloseTo(137, 9);
    expect(clipSpanToPxY(pxToClipY(137, CANVAS), CANVAS)).toBeCloseTo(137, 9);
  });
});

describe("plotBox", () => {
  it("insets each side by its gutter, in clip units", () => {
    const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);
    expect(box.x.min).toBeCloseTo(-1 + (2 * 64) / 1280, 12); // -0.9
    expect(box.x.max).toBeCloseTo(1 - (2 * 16) / 1280, 12); // 0.975
    expect(box.y.min).toBeCloseTo(-1 + (2 * 28) / 800, 12); // -0.93
    expect(box.y.max).toBeCloseTo(1 - (2 * 12) / 800, 12); // 0.97
  });

  it("puts screen-top on clip +Y, not clip −Y", () => {
    // The one sign that is easy to get backwards. A big `top` inset must pull
    // y.max DOWN and leave y.min alone.
    const box = plotBox(CANVAS, { top: 200, right: 0, bottom: 0, left: 0 });
    expect(box.y.max).toBeCloseTo(1 - (2 * 200) / 800, 12);
    expect(box.y.min).toBeCloseTo(-1, 12);
  });

  it("is the full clip space with zero insets", () => {
    const box = plotBox(CANVAS, { top: 0, right: 0, bottom: 0, left: 0 });
    expect(box).toEqual({ x: { min: -1, max: 1 }, y: { min: -1, max: 1 } });
  });

  it("refuses insets that leave no box rather than clamping", () => {
    expect(() => plotBox({ width: 100, height: 800 }, { ...DEFAULT_PLOT_INSETS, left: 90, right: 20 })).toThrow(
      PlotBoxError,
    );
    expect(() => plotBox({ width: 1280, height: 30 }, { ...DEFAULT_PLOT_INSETS })).toThrow(PlotBoxError);
  });

  it("refuses a nonsense canvas or inset", () => {
    expect(() => plotBox({ width: 0, height: 800 })).toThrow(PlotBoxError);
    expect(() => plotBox({ width: NaN, height: 800 })).toThrow(PlotBoxError);
    expect(() => plotBox(CANVAS, { ...DEFAULT_PLOT_INSETS, left: -5 })).toThrow(PlotBoxError);
  });
});

describe("gutters", () => {
  const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);
  const g = gutters(box);

  it("bands reach the clip boundary and abut the box", () => {
    expect(g.left.x).toEqual({ min: -1, max: box.x.min });
    expect(g.right.x).toEqual({ min: box.x.max, max: 1 });
    expect(g.bottom.y).toEqual({ min: -1, max: box.y.min });
    expect(g.top.y).toEqual({ min: box.y.max, max: 1 });
  });

  it("the bottom gutter is as many pixels tall as insets.bottom", () => {
    expect(clipSpanToPxY(g.bottom.y.max - g.bottom.y.min, CANVAS)).toBeCloseTo(DEFAULT_PLOT_INSETS.bottom, 9);
    expect(clipSpanToPxX(g.left.x.max - g.left.x.min, CANVAS)).toBeCloseTo(DEFAULT_PLOT_INSETS.left, 9);
  });

  it("no gutter overlaps the box", () => {
    expect(g.left.x.max).toBeLessThanOrEqual(box.x.min);
    expect(g.right.x.min).toBeGreaterThanOrEqual(box.x.max);
    expect(g.bottom.y.max).toBeLessThanOrEqual(box.y.min);
    expect(g.top.y.min).toBeGreaterThanOrEqual(box.y.max);
  });
});

describe("paneRegionFor", () => {
  it("is the box, in the engine's setPaneRegion field names", () => {
    const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);
    expect(paneRegionFor(box)).toEqual({
      clipXMin: box.x.min,
      clipXMax: box.x.max,
      clipYMin: box.y.min,
      clipYMax: box.y.max,
    });
  });

  it("zero insets reproduce the engine's own pane default", () => {
    expect(paneRegionFor(plotBox(CANVAS, { top: 0, right: 0, bottom: 0, left: 0 }))).toEqual(FULL_CLIP_REGION);
  });
});

describe("fitToPlotBox", () => {
  const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);

  it("lands the domain endpoints exactly on the box edges", () => {
    const domain = { x: { min: 100, max: 200 }, y: { min: 408, max: 418 } };
    const t = fitToPlotBox(domain, box);
    expect(domain.x.min * t.sx + t.tx).toBeCloseTo(box.x.min, 10);
    expect(domain.x.max * t.sx + t.tx).toBeCloseTo(box.x.max, 10);
    expect(domain.y.min * t.sy + t.ty).toBeCloseTo(box.y.min, 10);
    expect(domain.y.max * t.sy + t.ty).toBeCloseTo(box.y.max, 10);
  });

  it("keeps the orientation positive — higher data is higher clip", () => {
    // scale.ts's ORIENTATION note: the Y flip lives in the blit (ENC-696 /
    // DC-L05), never here. A negative sy would silently invert every chart.
    const t = fitToPlotBox({ x: { min: 0, max: 10 }, y: { min: 0, max: 10 } }, box);
    expect(t.sx).toBeGreaterThan(0);
    expect(t.sy).toBeGreaterThan(0);
  });

  it("padding shrinks the ink inside the box rather than overflowing it", () => {
    const domain = { x: { min: 0, max: 100 }, y: { min: 0, max: 100 } };
    const t = fitToPlotBox(domain, box, { paddingFracY: 0.1 });
    const lo = domain.y.min * t.sy + t.ty;
    const hi = domain.y.max * t.sy + t.ty;
    expect(lo).toBeGreaterThan(box.y.min);
    expect(hi).toBeLessThan(box.y.max);
  });

  it("leaves an unstated axis at the identity it was given", () => {
    const identity = { sx: 7, tx: 8, sy: 9, ty: 10 };
    const t = fitToPlotBox({ x: { min: 0, max: 1 }, y: null }, box, {}, identity);
    expect(t.sy).toBe(9);
    expect(t.ty).toBe(10);
    expect(t.sx).not.toBe(7);
  });
});

describe("framingMetrics", () => {
  const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);
  const domain = { x: { min: 0, max: 269 }, y: { min: 404.2, max: 419.8 } };

  it("a fitted transform fills its box and clears the edges by the insets", () => {
    const t = fitToPlotBox(domain, box);
    const m = framingMetrics(domain, t, box, CANVAS);
    expect(m.fillRatio).toBeCloseTo(1, 9);
    expect(m.edgeClearancePx.left).toBeCloseTo(DEFAULT_PLOT_INSETS.left, 6);
    expect(m.edgeClearancePx.right).toBeCloseTo(DEFAULT_PLOT_INSETS.right, 6);
    expect(m.edgeClearancePx.top).toBeCloseTo(DEFAULT_PLOT_INSETS.top, 6);
    expect(m.edgeClearancePx.bottom).toBeCloseTo(DEFAULT_PLOT_INSETS.bottom, 6);
    expect(m.inkPx.width).toBeCloseTo(CANVAS.width - 64 - 16, 6);
    expect(m.inkPx.height).toBeCloseTo(CANVAS.height - 12 - 28, 6);
  });

  it("reports NEGATIVE clearance when geometry leaves the frame", () => {
    // The live failure's shape: a transform that projects data past clip ±1.
    const t = { sx: 0.02, tx: -1.5, sy: 1, ty: 0 };
    const m = framingMetrics({ x: { min: 0, max: 200 }, y: { min: -0.5, max: 0.5 } }, t, box, CANVAS);
    expect(m.edgeClearancePx.left).toBeLessThan(0);
    expect(checkTier2Framing(m).pass).toBe(false);
  });

  it("dead margin is measured against the canvas, not the box", () => {
    const t = fitToPlotBox(domain, box);
    const m = framingMetrics(domain, t, box, CANVAS);
    const boxArea = ((CANVAS.width - 80) / CANVAS.width) * ((CANVAS.height - 40) / CANVAS.height);
    expect(m.deadMarginFrac).toBeCloseTo(1 - boxArea, 9);
    // The default insets' cost, stated so a change to them shows up here.
    expect(m.deadMarginFrac).toBeCloseTo(0.109375, 9);
    // A perfect fit spends its whole dead margin on gutters and none on slack.
    expect(m.gutterFrac).toBeCloseTo(m.deadMarginFrac, 9);
  });
});

describe("checkTier2Framing", () => {
  const box = plotBox(CANVAS, DEFAULT_PLOT_INSETS);
  const domain = { x: { min: 0, max: 100 }, y: { min: 0, max: 100 } };

  it("passes a fitted framing", () => {
    const v = checkTier2Framing(framingMetrics(domain, fitToPlotBox(domain, box), box, CANVAS));
    expect(v).toEqual({ pass: true, failures: [] });
  });

  it("fails an underfilled box and says by how much", () => {
    // Half-scale: the data occupies a quarter of the box's area.
    const fitted = fitToPlotBox(domain, box);
    const half = { sx: fitted.sx / 2, tx: fitted.tx / 2, sy: fitted.sy / 2, ty: fitted.ty / 2 };
    const v = checkTier2Framing(framingMetrics(domain, half, box, CANVAS));
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toMatch(/fill ratio/);
  });

  it("fails a framing that overflows the box even though it fills the canvas", () => {
    const full = fitToPlotBox(domain, { x: { min: -1, max: 1 }, y: { min: -1, max: 1 } });
    const v = checkTier2Framing(framingMetrics(domain, full, box, CANVAS));
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toMatch(/edge clearance/);
  });

  it("the bounds are stated, not implied", () => {
    expect(TIER2_FRAMING_BOUNDS.maxDeadMarginFrac).toBe(0.37);
    expect(TIER2_FRAMING_BOUNDS.minFillRatio).toBe(0.9);
    expect(TIER2_FRAMING_BOUNDS.minEdgeClearancePx).toBe(4);
  });
});

describe("frameSeries", () => {
  it("returns a box and a pane region but no transform when nothing has streamed", () => {
    const framed = frameSeries({ x: null, y: null }, CANVAS);
    expect(framed.transform).toBeNull();
    expect(framed.metrics).toBeNull();
    expect(framed.paneRegion).toEqual(paneRegionFor(framed.box));
  });

  it("the pane region and the transform describe the same rectangle", () => {
    const framed = frameSeries({ x: { min: 0, max: 10 }, y: { min: 1, max: 2 } }, CANVAS);
    const t = framed.transform!;
    expect(0 * t.sx + t.tx).toBeCloseTo(framed.paneRegion.clipXMin, 10);
    expect(10 * t.sx + t.tx).toBeCloseTo(framed.paneRegion.clipXMax, 10);
    expect(1 * t.sy + t.ty).toBeCloseTo(framed.paneRegion.clipYMin, 10);
    expect(2 * t.sy + t.ty).toBeCloseTo(framed.paneRegion.clipYMax, 10);
  });

  it("scores tier-2 green for any canvas the default insets fit in", () => {
    for (const canvas of [
      { width: 1280, height: 800 },
      { width: 1337, height: 688 }, // the live product's canvas
      { width: 1920, height: 1080 },
      { width: 640, height: 400 },
      { width: 400, height: 300 }, // small: the fixed gutters cost 30.7% here
    ]) {
      const framed = frameSeries({ x: { min: 0, max: 269 }, y: { min: 404, max: 420 } }, canvas);
      const verdict = checkTier2Framing(framed.metrics!);
      expect(`${canvas.width}x${canvas.height}: ${verdict.failures.join("; ")}`).toBe(
        `${canvas.width}x${canvas.height}: `,
      );
    }
  });
});

/* ── The acceptance: the live chart, before and after ────────────────────────
 *
 * Numbers below are MEASURED from the canvas region of
 * `specs/2026-09-19-chart-quality-bar/harness/live-nexo-candles-60s.png`
 * (1337x688, the region below the app header and left of the rail; adapter
 * nvidia/ampere per SPEC D8), not estimated:
 *
 *   chromatic ink bbox   x 590..1136, y 17..581
 *   dead margin          left 44.1%  right 15.0%  top 2.5%  bottom 15.4%
 *   bbox coverage        33.6% of the canvas  →  dead-margin total 66.4%
 *   seven candles share  canvas row 17 as their top — a flat cut, not seven
 *                        equal highs: the series was clipped, not framed.
 */
describe("the live NEXO chart (SPEC §1.0)", () => {
  const LIVE_CANVAS = { width: 1337, height: 688 };
  // The ink bbox as clip-space extents, i.e. what the OLD framing produced.
  // clipX = 2*px/W − 1 ; clipY = 1 − 2*py/H  (mapping.ts's convention).
  const OLD_INK = {
    x: { min: (2 * 590) / 1337 - 1, max: (2 * 1136) / 1337 - 1 },
    y: { min: 1 - (2 * 581) / 688, max: 1 - (2 * 17) / 688 },
  };
  // A synthetic domain standing in for the ten candles' data extent. Its
  // absolute values are irrelevant — framing is scale-free — so this is an
  // index/price pair shaped like the real one.
  const DOMAIN = { x: { min: 0, max: 9 }, y: { min: 404.2, max: 419.8 } };
  // The transform that maps DOMAIN onto OLD_INK: the old framing, restated.
  const OLD_TRANSFORM = (() => {
    const sx = (OLD_INK.x.max - OLD_INK.x.min) / (DOMAIN.x.max - DOMAIN.x.min);
    const sy = (OLD_INK.y.max - OLD_INK.y.min) / (DOMAIN.y.max - DOMAIN.y.min);
    return { sx, tx: OLD_INK.x.min - sx * DOMAIN.x.min, sy, ty: OLD_INK.y.min - sy * DOMAIN.y.min };
  })();

  const box = plotBox(LIVE_CANVAS, DEFAULT_PLOT_INSETS);

  it("BEFORE: the measured framing fails tier 2 on dead margin and fill", () => {
    const m = framingMetrics(DOMAIN, OLD_TRANSFORM, box, LIVE_CANVAS);
    // Reproduces the raster measurement to within a pixel of rounding.
    expect(m.deadMarginFrac).toBeCloseTo(0.664, 2);
    expect(m.edgeClearancePx.left).toBeCloseTo(590, 0);
    expect(m.edgeClearancePx.top).toBeCloseTo(17, 0);

    const v = checkTier2Framing(m);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" | ")).toMatch(/dead margin 66\.\d%/);
    expect(v.failures.join(" | ")).toMatch(/fill ratio 0\.3\d\d/);
  });

  it("AFTER: fitting the same domain to the plot box passes tier 2", () => {
    const framed = frameSeries(DOMAIN, LIVE_CANVAS);
    const v = checkTier2Framing(framed.metrics!);
    expect(v.failures).toEqual([]);
    expect(v.pass).toBe(true);
  });

  it("AFTER: the numbers move the way the acceptance asks", () => {
    const before = framingMetrics(DOMAIN, OLD_TRANSFORM, box, LIVE_CANVAS);
    const after = frameSeries(DOMAIN, LIVE_CANVAS).metrics!;

    // dead margin: under the stated threshold, and far under the old value.
    expect(after.deadMarginFrac).toBeLessThan(TIER2_FRAMING_BOUNDS.maxDeadMarginFrac);
    expect(after.deadMarginFrac).toBeLessThan(before.deadMarginFrac / 4);

    // ink-to-frame ratio: inside the stated band, up from ~a third.
    expect(before.fillRatio).toBeLessThan(0.5);
    expect(after.fillRatio).toBeGreaterThanOrEqual(TIER2_FRAMING_BOUNDS.minFillRatio);
    expect(after.fillRatio).toBeLessThanOrEqual(TIER2_FRAMING_BOUNDS.maxFillRatio);

    // no geometry intersecting the frame edge: every side clears by its gutter.
    expect(after.minEdgeClearancePx).toBeGreaterThanOrEqual(TIER2_FRAMING_BOUNDS.minEdgeClearancePx);
    expect(after.edgeClearancePx.top).toBeCloseTo(DEFAULT_PLOT_INSETS.top, 6);
  });

  it("AFTER: a domain twice as tall still clears the top edge (no flat cut)", () => {
    // The specific live defect: the series grew past the frame and was cut.
    // With a fit, growth rescales instead of clipping.
    const grown = { x: { min: 0, max: 40 }, y: { min: 380, max: 460 } };
    const framed = frameSeries(grown, LIVE_CANVAS);
    expect(checkTier2Framing(framed.metrics!).pass).toBe(true);
    expect(framed.metrics!.edgeClearancePx.top).toBeCloseTo(DEFAULT_PLOT_INSETS.top, 6);
  });
});

describe("insets are a knob, not a constant", () => {
  it("a caller may halve the defaults and still pass tier 2", () => {
    const half: PlotInsets = { top: 6, right: 8, bottom: 14, left: 32 };
    const framed = frameSeries({ x: { min: 0, max: 100 }, y: { min: 0, max: 1 } }, CANVAS, half);
    expect(checkTier2Framing(framed.metrics!).pass).toBe(true);
  });

  it("but a caller who reserves nothing fails the edge-clearance floor", () => {
    const none: PlotInsets = { top: 0, right: 0, bottom: 0, left: 0 };
    const framed = frameSeries({ x: { min: 0, max: 100 }, y: { min: 0, max: 1 } }, CANVAS, none);
    const v = checkTier2Framing(framed.metrics!);
    expect(v.pass).toBe(false);
    expect(v.failures.join(" ")).toMatch(/edge clearance 0\.0px/);
  });
});
