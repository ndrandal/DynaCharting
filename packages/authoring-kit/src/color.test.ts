import { describe, it, expect } from "vitest";
import { rgba, hsl, toByte, packColorBytes, mixColor } from "./color";

describe("rgba", () => {
  it("builds a tuple with default opaque alpha", () => {
    expect(rgba(0.1, 0.2, 0.3)).toEqual([0.1, 0.2, 0.3, 1]);
    expect(rgba(0.1, 0.2, 0.3, 0.4)).toEqual([0.1, 0.2, 0.3, 0.4]);
  });
});

describe("hsl", () => {
  const near = (got: number[], want: number[]) => {
    got.forEach((g, i) => expect(g).toBeCloseTo(want[i], 6));
  };
  it("maps primary hues to RGB", () => {
    near(hsl(0, 1, 0.5), [1, 0, 0, 1]); // red
    near(hsl(120, 1, 0.5), [0, 1, 0, 1]); // green
    near(hsl(240, 1, 0.5), [0, 0, 1, 1]); // blue
  });
  it("desaturates to gray", () => {
    near(hsl(0, 0, 0.5), [0.5, 0.5, 0.5, 1]);
  });
  it("carries alpha", () => {
    expect(hsl(0, 1, 0.5, 0.25)[3]).toBe(0.25);
  });
});

describe("toByte / packColorBytes", () => {
  it("quantizes [0,1] to unorm8 with rounding and clamping", () => {
    expect(toByte(0)).toBe(0);
    expect(toByte(1)).toBe(255);
    expect(toByte(0.5)).toBe(128);
    expect(toByte(2)).toBe(255); // clamp high
    expect(toByte(-1)).toBe(0); // clamp low
  });
  it("packs RGBA to four bytes, alpha defaulting to 255", () => {
    expect(packColorBytes([1, 0, 0, 1])).toEqual([255, 0, 0, 255]);
    expect(packColorBytes([0.5, 0.5, 0.5])).toEqual([128, 128, 128, 255]);
  });
});

describe("mixColor", () => {
  it("interpolates per channel including alpha", () => {
    expect(mixColor([0, 0, 0, 1], [1, 1, 1, 1], 0.5)).toEqual([0.5, 0.5, 0.5, 1]);
    expect(mixColor([0, 0, 0, 0], [1, 1, 1, 1], 0.25)).toEqual([0.25, 0.25, 0.25, 0.25]);
  });
});
