import { describe, it, expect } from "vitest";
import {
  polylineSegments,
  areaTriangles,
  triangulateFan,
  circleRing,
  wedgeTriangles,
  triGradientFloats,
  gradientRectVerts,
  rectsColoredVerts,
} from "./geometry";

describe("polylineSegments", () => {
  it("emits N-1 rect4 segments", () => {
    const segs = polylineSegments([
      { x: 0, y: 0 },
      { x: 1, y: 1 },
      { x: 2, y: 0 },
    ]);
    expect(segs).toEqual([0, 0, 1, 1, 1, 1, 2, 0]);
  });
});

describe("areaTriangles", () => {
  it("emits 2 triangles (12 floats) per gap to baseline", () => {
    const tris = areaTriangles([{ x: 0, y: 2 }, { x: 1, y: 3 }], 0);
    expect(tris).toEqual([0, 2, 0, 0, 1, 3, 1, 3, 0, 0, 1, 0]);
  });
});

describe("triangulateFan", () => {
  it("fans a closed square from center into 4 triangles", () => {
    const ring = [
      { x: 1, y: 0 },
      { x: 0, y: 1 },
      { x: -1, y: 0 },
      { x: 0, y: -1 },
    ];
    const tris = triangulateFan({ x: 0, y: 0 }, ring);
    expect(tris.length).toBe(24); // 4 tris * 3 verts * 2 coords
    // first triangle: center, ring[0], ring[1]
    expect(tris.slice(0, 6)).toEqual([0, 0, 1, 0, 0, 1]);
    // last triangle closes back to ring[0]
    expect(tris.slice(18, 24)).toEqual([0, 0, 0, -1, 1, 0]);
  });
});

describe("circleRing", () => {
  it("samples `segs` points at radius r (aspect scales x)", () => {
    const ring = circleRing(0, 0, 2, 4);
    expect(ring.length).toBe(4);
    // first point at angle 0 -> (r, 0)
    expect(ring[0].x).toBeCloseTo(2, 6);
    expect(ring[0].y).toBeCloseTo(0, 6);
    // quarter turn -> (0, r)
    expect(ring[1].x).toBeCloseTo(0, 6);
    expect(ring[1].y).toBeCloseTo(2, 6);
  });
  it("aspect scales only the x radius", () => {
    const ring = circleRing(0, 0, 2, 4, 0.5);
    expect(ring[0].x).toBeCloseTo(1, 6); // 2 * 0.5
    expect(ring[1].y).toBeCloseTo(2, 6); // y unscaled
  });
});

describe("wedgeTriangles", () => {
  it("emits segs triangles (segs*6 floats)", () => {
    const tris = wedgeTriangles(0, 0, 1, 0, Math.PI / 2, 8);
    expect(tris.length).toBe(8 * 6);
    // every triangle starts at the center (cx,cy)
    for (let i = 0; i < tris.length; i += 6) {
      expect(tris[i]).toBe(0);
      expect(tris[i + 1]).toBe(0);
    }
  });
});

describe("gradient vertex builders", () => {
  it("triGradientFloats flattens to pos2_color4 with default alpha", () => {
    const flat = triGradientFloats([
      { x: 1, y: 2, rgba: [0.1, 0.2, 0.3] },
      { x: 3, y: 4, rgba: [0.4, 0.5, 0.6, 0.7] },
    ]);
    expect(flat).toEqual([1, 2, 0.1, 0.2, 0.3, 1, 3, 4, 0.4, 0.5, 0.6, 0.7]);
  });
  it("gradientRectVerts emits 6 colored verts spanning c0..c1", () => {
    const v = gradientRectVerts(
      { x0: 0, y0: 0, x1: 1, y1: 1 },
      { angle: 0, c0: [0, 0, 0, 1], c1: [1, 1, 1, 1] },
    );
    expect(v.length).toBe(6);
    // angle 0 -> gradient along +x: left edge is c0 (black), right edge is c1 (white)
    expect(v[0].rgba).toEqual([0, 0, 0, 1]); // x0,y0 corner
    expect(v[1].rgba).toEqual([1, 1, 1, 1]); // x1,y0 corner
  });
  it("rectsColoredVerts emits 6 verts per rect carrying its color", () => {
    const v = rectsColoredVerts([{ x0: 0, y0: 0, x1: 1, y1: 1, rgba: [0.5, 0, 0, 1] }]);
    expect(v.length).toBe(6);
    v.forEach((vert) => expect(vert.rgba).toEqual([0.5, 0, 0, 1]));
  });
});
