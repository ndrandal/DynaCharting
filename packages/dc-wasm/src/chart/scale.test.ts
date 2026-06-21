/* packages/dc-wasm/src/chart/scale.test.ts — ENC-699 (G4) */
import { describe, it, expect } from "vitest";
import {
  scale,
  niceTicks,
  indexTicks,
  fitAxis,
  fitTransform,
  horizontalGridSegments,
  verticalGridSegments,
  gridSegments,
  padRange,
  CLIP_RANGE,
} from "./scale";

describe("scale", () => {
  it("maps data range linearly onto output range", () => {
    const s = scale({ min: 0, max: 10 }, { min: -1, max: 1 });
    expect(s.map(0)).toBe(-1);
    expect(s.map(10)).toBe(1);
    expect(s.map(5)).toBeCloseTo(0, 12);
  });

  it("invert is the exact inverse of map", () => {
    const s = scale({ min: 100, max: 250 }, { min: 0, max: 800 });
    for (const v of [100, 137.5, 200, 250]) {
      expect(s.invert(s.map(v))).toBeCloseTo(v, 9);
    }
  });

  it("exposes slope and intercept matching out = s*v + t", () => {
    const s = scale({ min: 2, max: 6 }, { min: 10, max: 30 });
    expect(s.s).toBeCloseTo((30 - 10) / (6 - 2), 12);
    expect(s.t).toBeCloseTo(10 - 2 * s.s, 12);
    expect(s.map(4)).toBeCloseTo(s.s * 4 + s.t, 12);
  });

  it("handles a degenerate data range without NaN (midpoint fallback)", () => {
    const s = scale({ min: 5, max: 5 }, { min: -1, max: 1 });
    expect(s.s).toBe(0);
    expect(s.map(5)).toBe(0); // midpoint of [-1,1]
    expect(s.map(999)).toBe(0);
    expect(Number.isNaN(s.invert(0))).toBe(false);
    expect(s.invert(0)).toBe(5);
  });

  it("supports inverted output ranges (e.g. pixel-space top-down)", () => {
    const s = scale({ min: 0, max: 100 }, { min: 480, max: 0 });
    expect(s.map(0)).toBe(480);
    expect(s.map(100)).toBe(0);
    expect(s.invert(240)).toBeCloseTo(50, 9);
  });
});

describe("niceTicks", () => {
  it("produces round 1/2/5-stepped ticks within range", () => {
    const ticks = niceTicks(0, 100, 5);
    expect(ticks).toEqual([0, 20, 40, 60, 80, 100]);
  });

  it("keeps all ticks inside [min, max]", () => {
    const min = 103.7;
    const max = 187.2;
    const ticks = niceTicks(min, max, 6);
    expect(ticks.length).toBeGreaterThan(0);
    for (const t of ticks) {
      expect(t).toBeGreaterThanOrEqual(min - 1e-6);
      expect(t).toBeLessThanOrEqual(max + 1e-6);
    }
  });

  it("returns ascending, evenly-spaced ticks", () => {
    const ticks = niceTicks(0, 1, 5);
    for (let i = 1; i < ticks.length; i++) {
      expect(ticks[i]).toBeGreaterThan(ticks[i - 1]);
    }
    const step = ticks[1] - ticks[0];
    for (let i = 1; i < ticks.length; i++) {
      expect(ticks[i] - ticks[i - 1]).toBeCloseTo(step, 9);
    }
  });

  it("chooses a step near the requested density", () => {
    // ~6 ticks over a 60-wide span -> step 10.
    const ticks = niceTicks(0, 60, 6);
    expect(ticks[1] - ticks[0]).toBe(10);
  });

  it("works on small fractional ranges (no float garbage)", () => {
    const ticks = niceTicks(1.001, 1.009, 4);
    for (const t of ticks) {
      // round-to-6 keeps values clean
      expect(t).toBe(+t.toFixed(6));
    }
  });

  it("treats count<=0 as 1", () => {
    expect(() => niceTicks(0, 10, 0)).not.toThrow();
    expect(niceTicks(0, 10, 0).length).toBeGreaterThan(0);
  });

  it("returns [min] for degenerate or inverted ranges", () => {
    expect(niceTicks(5, 5, 6)).toEqual([5]);
    expect(niceTicks(10, 2, 6)).toEqual([10]);
  });

  it("returns [] for non-finite inputs", () => {
    expect(niceTicks(NaN, 10, 6)).toEqual([]);
    expect(niceTicks(0, Infinity, 6)).toEqual([]);
  });
});

describe("indexTicks", () => {
  it("spaces integer indices across [0, count-1]", () => {
    expect(indexTicks(64, 8)).toEqual([0, 8, 16, 24, 32, 40, 48, 56]);
  });

  it("always starts at 0 and stays within bounds", () => {
    const t = indexTicks(50, 8);
    expect(t[0]).toBe(0);
    for (const i of t) expect(i).toBeLessThan(50);
  });

  it("never uses a step below 1", () => {
    expect(indexTicks(3, 100)).toEqual([0, 1, 2]);
  });

  it("returns [] for empty data", () => {
    expect(indexTicks(0, 8)).toEqual([]);
  });
});

describe("fitAxis", () => {
  it("maps full data range onto NDC [-1,1] with positive slope", () => {
    const f = fitAxis({ min: 100, max: 200 });
    expect(f.s).toBeGreaterThan(0);
    expect(f.s).toBeCloseTo(2 / 100, 12); // 2/range
    // ORIENTATION: higher data -> higher clip
    expect(f.s * 100 + f.t).toBeCloseTo(-1, 12);
    expect(f.s * 200 + f.t).toBeCloseTo(1, 12);
  });

  it("respects a custom (pane) clip band", () => {
    const f = fitAxis({ min: 0, max: 10 }, { min: 0.08, max: 0.9 });
    expect(f.s * 0 + f.t).toBeCloseTo(0.08, 12);
    expect(f.s * 10 + f.t).toBeCloseTo(0.9, 12);
  });
});

describe("fitTransform", () => {
  it("returns engine setTransform field shape {sx,tx,sy,ty}", () => {
    const xf = fitTransform({ x: { min: 0, max: 9 }, y: { min: 100, max: 200 } });
    expect(Object.keys(xf).sort()).toEqual(["sx", "sy", "tx", "ty"]);
  });

  it("uses the mathematically correct orientation: positive sy = 2/range", () => {
    // Matches embassy RangeTracker (sy = 2.0/rng > 0). The Y-flip bug is fixed
    // centrally in ENC-696, NOT here.
    const xf = fitTransform({ x: { min: 0, max: 1 }, y: { min: 0, max: 50 } });
    expect(xf.sy).toBeGreaterThan(0);
    expect(xf.sy).toBeCloseTo(2 / 50, 12);
    // y=min -> clip -1 (bottom), y=max -> clip +1 (top)
    expect(xf.sy * 0 + xf.ty).toBeCloseTo(-1, 12);
    expect(xf.sy * 50 + xf.ty).toBeCloseTo(1, 12);
  });

  it("supports stacked panes sharing X with distinct Y clip bands", () => {
    const dataX = { min: 0, max: 63 };
    const price = fitTransform(
      { x: dataX, y: { min: 100, max: 200 } },
      { x: CLIP_RANGE, y: { min: 0.08, max: 0.9 } },
    );
    const volume = fitTransform(
      { x: dataX, y: { min: 0, max: 1000 } },
      { x: CLIP_RANGE, y: { min: -0.9, max: -0.14 } },
    );
    // shared X
    expect(volume.sx).toBeCloseTo(price.sx, 12);
    expect(volume.tx).toBeCloseTo(price.tx, 12);
    // independent Y bands
    expect(price.sy * 200 + price.ty).toBeCloseTo(0.9, 12);
    expect(volume.sy * 0 + volume.ty).toBeCloseTo(-0.9, 12);
    expect(volume.sy * 1000 + volume.ty).toBeCloseTo(-0.14, 12);
  });

  it("matches the harness price-transform algebra", () => {
    // harness: PY0=0.08, PY1=0.90; syP=(PY1-PY0)/(pMax-pMin); tyP=PY0 - pMin*syP
    const pMin = 100;
    const pMax = 200;
    const xf = fitTransform(
      { x: { min: 0, max: 9 }, y: { min: pMin, max: pMax } },
      { x: CLIP_RANGE, y: { min: 0.08, max: 0.9 } },
    );
    const syP = (0.9 - 0.08) / (pMax - pMin);
    const tyP = 0.08 - pMin * syP;
    expect(xf.sy).toBeCloseTo(syP, 12);
    expect(xf.ty).toBeCloseTo(tyP, 12);
  });
});

describe("grid segments", () => {
  it("horizontalGridSegments emits rect4 lines spanning x at each y tick", () => {
    const seg = horizontalGridSegments([10, 20], { min: 0, max: 5 });
    // x0,y0,x1,y1 per tick
    expect(seg).toEqual([0, 10, 5, 10, 0, 20, 5, 20]);
  });

  it("verticalGridSegments emits rect4 lines spanning y at each x tick", () => {
    const seg = verticalGridSegments([1, 4], { min: 100, max: 200 });
    expect(seg).toEqual([1, 100, 1, 200, 4, 100, 4, 200]);
  });

  it("gridSegments builds both arrays with correct rect4 counts", () => {
    const g = gridSegments({
      xTicks: [0, 8, 16],
      yTicks: [100, 150, 200],
      dataRange: { x: { min: 0, max: 16 }, y: { min: 100, max: 200 } },
    });
    expect(g.horizontalCount).toBe(3);
    expect(g.verticalCount).toBe(3);
    expect(g.horizontal.length).toBe(g.horizontalCount * 4);
    expect(g.vertical.length).toBe(g.verticalCount * 4);
    // first horizontal line: full x span at first y tick
    expect(g.horizontal.slice(0, 4)).toEqual([0, 100, 16, 100]);
    // first vertical line: full y span at first x tick
    expect(g.vertical.slice(0, 4)).toEqual([0, 100, 0, 200]);
  });

  it("produces empty arrays for empty tick lists", () => {
    const g = gridSegments({
      xTicks: [],
      yTicks: [],
      dataRange: { x: { min: 0, max: 1 }, y: { min: 0, max: 1 } },
    });
    expect(g.horizontal).toEqual([]);
    expect(g.vertical).toEqual([]);
    expect(g.horizontalCount).toBe(0);
    expect(g.verticalCount).toBe(0);
  });
});

describe("padRange", () => {
  it("expands a range outward by a fraction of its span", () => {
    const r = padRange({ min: 100, max: 200 }, 0.06);
    expect(r.min).toBeCloseTo(94, 9);
    expect(r.max).toBeCloseTo(206, 9);
  });

  it("does not mutate the input", () => {
    const input = { min: 0, max: 10 };
    padRange(input, 0.1);
    expect(input).toEqual({ min: 0, max: 10 });
  });
});

describe("integration: full chart fit round-trip", () => {
  it("scale.invert recovers data from a fitTransform clip coordinate", () => {
    const dataY = { min: 100, max: 200 };
    const clipY = { min: -1, max: 1 };
    const xf = fitTransform({ x: { min: 0, max: 9 }, y: dataY }, { x: CLIP_RANGE, y: clipY });
    const sY = scale(dataY, clipY);
    // the per-axis scale must agree with the 2D transform's y component
    expect(sY.s).toBeCloseTo(xf.sy, 12);
    expect(sY.t).toBeCloseTo(xf.ty, 12);
    const clip = xf.sy * 137.5 + xf.ty;
    expect(sY.invert(clip)).toBeCloseTo(137.5, 9);
  });
});
