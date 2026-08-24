import { describe, it, expect } from "vitest";
import { f32, packMixed } from "./packing";

describe("f32", () => {
  it("builds a tightly packed Float32Array", () => {
    const b = f32([1, 2, 3]);
    expect(b).toBeInstanceOf(Float32Array);
    expect(Array.from(b)).toEqual([1, 2, 3]);
  });
});

describe("packMixed", () => {
  it("interleaves f32 lanes with a unorm8x4 color lane (rect4_color)", () => {
    const buf = packMixed([{ floats: [1, 2, 3, 4], rgba: [1, 0, 0, 1] }], 4, 0);
    // stride = (4 + 1 + 0) * 4 = 20 bytes
    expect(buf.byteLength).toBe(20);
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    expect(dv.getFloat32(0, true)).toBe(1);
    expect(dv.getFloat32(4, true)).toBe(2);
    expect(dv.getFloat32(8, true)).toBe(3);
    expect(dv.getFloat32(12, true)).toBe(4);
    expect([buf[16], buf[17], buf[18], buf[19]]).toEqual([255, 0, 0, 255]);
  });

  it("supports trailing float lanes and default alpha", () => {
    const buf = packMixed([{ floats: [1, 2], rgba: [0, 1, 0], floatsAfter: [9] }], 2, 1);
    // stride = (2 + 1 + 1) * 4 = 16
    expect(buf.byteLength).toBe(16);
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    expect(dv.getFloat32(0, true)).toBe(1);
    expect(dv.getFloat32(4, true)).toBe(2);
    expect([buf[8], buf[9], buf[10], buf[11]]).toEqual([0, 255, 0, 255]); // alpha default -> 255
    expect(dv.getFloat32(12, true)).toBe(9);
  });

  it("packs multiple records back to back", () => {
    const buf = packMixed(
      [
        { floats: [0, 0, 1, 1], rgba: [1, 1, 1, 1] },
        { floats: [2, 2, 3, 3], rgba: [0, 0, 0, 1] },
      ],
      4,
      0,
    );
    expect(buf.byteLength).toBe(40);
    const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
    expect(dv.getFloat32(20, true)).toBe(2); // second record's first float
    expect([buf[36], buf[37], buf[38], buf[39]]).toEqual([0, 0, 0, 255]);
  });
});
