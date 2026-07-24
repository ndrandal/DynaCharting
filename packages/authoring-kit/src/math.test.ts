import { describe, it, expect } from "vitest";
import { lerp, clamp, extent, rng } from "./math";

describe("lerp / clamp", () => {
  it("lerp interpolates (and extrapolates unclamped)", () => {
    expect(lerp(0, 10, 0.5)).toBe(5);
    expect(lerp(2, 4, 0)).toBe(2);
    expect(lerp(2, 4, 1)).toBe(4);
    expect(lerp(0, 10, 2)).toBe(20);
  });
  it("clamp bounds into [lo,hi]", () => {
    expect(clamp(5, 0, 10)).toBe(5);
    expect(clamp(-3, 0, 10)).toBe(0);
    expect(clamp(99, 0, 10)).toBe(10);
  });
});

describe("extent", () => {
  it("finds min/max of raw numbers", () => {
    expect(extent([3, 1, 4, 1, 5, 9, 2, 6])).toEqual([1, 9]);
  });
  it("applies an accessor and skips null/NaN", () => {
    const rows = [{ v: 5 }, { v: NaN }, { v: -2 }, { v: 8 }];
    expect(extent(rows, (d) => d.v)).toEqual([-2, 8]);
  });
  it("returns the degenerate sentinel for empty input", () => {
    expect(extent([])).toEqual([Infinity, -Infinity]);
  });
});

describe("rng", () => {
  it("is deterministic for a given seed and stays in [0,1)", () => {
    const a = rng(42);
    const b = rng(42);
    const seqA = [a(), a(), a(), a()];
    const seqB = [b(), b(), b(), b()];
    expect(seqA).toEqual(seqB);
    for (const x of seqA) {
      expect(x).toBeGreaterThanOrEqual(0);
      expect(x).toBeLessThan(1);
    }
  });
  it("differs across seeds", () => {
    expect(rng(1)()).not.toBe(rng(2)());
  });
});
