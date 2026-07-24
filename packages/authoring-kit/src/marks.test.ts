import { describe, it, expect } from "vitest";
import {
  lineMark,
  candlesMark,
  areaMark,
  rectsMark,
  heatMark,
  gridMark,
  wedgeMark,
  gradientRectMark,
  scatterMark,
} from "./marks";

describe("lineMark", () => {
  it("produces a lineAA@1 / rect4 descriptor with N-1 segments", () => {
    const m = lineMark([
      { x: 0, y: 0 },
      { x: 1, y: 1 },
      { x: 2, y: 0 },
    ]);
    expect(m.pipeline).toBe("lineAA@1");
    expect(m.format).toBe("rect4");
    expect(m.count).toBe(2);
    expect(m.floats).toEqual([0, 0, 1, 1, 1, 1, 2, 0]);
    expect(m.style).toMatchObject({ r: 0.4, g: 0.7, b: 1, a: 1, lineWidth: 2 });
    expect(m.space).toBe("data");
  });
  it("threads color, width and dash", () => {
    const m = lineMark([{ x: 0, y: 0 }, { x: 1, y: 1 }], {
      color: [1, 0, 0, 0.5],
      width: 3,
      dash: [4, 2],
    });
    expect(m.style).toMatchObject({ r: 1, g: 0, b: 0, a: 0.5, lineWidth: 3, dashLength: 4, gapLength: 2 });
  });
});

describe("candlesMark", () => {
  it("packs candle6 rows with default halfWidth and up/down colors", () => {
    const m = candlesMark([{ x: 0, open: 1, high: 2, low: 0, close: 1.5 }]);
    expect(m.pipeline).toBe("instancedCandle@1");
    expect(m.format).toBe("candle6");
    expect(m.count).toBe(1);
    expect(m.floats).toEqual([0, 1, 2, 0, 1.5, 0.32]);
    expect(m.style).toMatchObject({
      colorUp: [0.16, 0.78, 0.45, 1],
      colorDown: [0.92, 0.3, 0.33, 1],
    });
  });
});

describe("areaMark / rectsMark", () => {
  it("areaMark emits (N-1)*6 pos2 vertices under triSolid@1", () => {
    const m = areaMark([{ x: 0, y: 2 }, { x: 1, y: 3 }], 0);
    expect(m.pipeline).toBe("triSolid@1");
    expect(m.count).toBe(6);
    expect(m.floats.length).toBe(12);
  });
  it("rectsMark emits one instance per rect with optional cornerRadius", () => {
    const m = rectsMark([{ x0: 0, y0: 0, x1: 1, y1: 1 }], { cornerRadius: 2 });
    expect(m.pipeline).toBe("instancedRect@1");
    expect(m.count).toBe(1);
    expect(m.floats).toEqual([0, 0, 1, 1]);
    expect(m.style).toMatchObject({ cornerRadius: 2 });
  });
});

describe("heatMark (per-cell color via triGradient fallback)", () => {
  it("routes colored rects through triGradient@1 / pos2_color4", () => {
    const m = heatMark([{ x0: 0, y0: 0, x1: 1, y1: 1, rgba: [0.5, 0, 0, 1] }]);
    expect(m.pipeline).toBe("triGradient@1");
    expect(m.format).toBe("pos2_color4");
    expect(m.count).toBe(6); // 2 tris * 3 verts
  });
});

describe("wedgeMark (pie)", () => {
  it("emits segs*3 pos2 vertices", () => {
    const m = wedgeMark(0, 0, 1, 0, Math.PI / 2, { segs: 8 });
    expect(m.pipeline).toBe("triSolid@1");
    expect(m.count).toBe(24);
  });
});

describe("gradientRectMark", () => {
  it("produces 6 pos2_color4 verts", () => {
    const m = gradientRectMark(
      { x0: 0, y0: 0, x1: 1, y1: 1 },
      { angle: 0, c0: [0, 0, 0, 1], c1: [1, 1, 1, 1] },
    );
    expect(m.pipeline).toBe("triGradient@1");
    expect(m.count).toBe(6);
    expect(m.floats.length).toBe(36); // 6 verts * 6 floats
  });
});

describe("scatterMark", () => {
  it("authors in clip space (no transform attached)", () => {
    const m = scatterMark(
      [{ x: 1, y: 2, size: 6, rgba: [1, 0, 0, 1] }],
      { sx: 1, tx: 0, sy: 1, ty: 0 },
      { segs: 12, W: 100, H: 100 },
    );
    expect(m.pipeline).toBe("triGradient@1");
    expect(m.space).toBe("clip");
    expect(m.count).toBe(12 * 3); // segs triangles
  });
});

describe("gridMark", () => {
  it("builds horizontal + vertical rect4 segments across data ranges", () => {
    const m = gridMark({
      ys: [10, 20],
      xs: [1],
      xRange: { min: 0, max: 5 },
      yRange: { min: 0, max: 30 },
    });
    expect(m.pipeline).toBe("lineAA@1");
    expect(m.count).toBe(3); // 2 horizontal + 1 vertical
    expect(m.floats).toEqual([0, 10, 5, 10, 0, 20, 5, 20, 1, 0, 1, 30]);
  });
});
