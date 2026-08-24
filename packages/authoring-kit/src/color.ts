/* packages/authoring-kit/src/color.ts — ENC-714
 *
 * Color vocabulary for the authoring kit. The engine wants RGBA as floats in
 * [0,1]; the color vertex formats (rect4_color / point4_color / pos2_color4's
 * packed variants) want unorm8 bytes. These helpers bridge both.
 *
 * Ported from the authoring-corpus runner. Pure & framework-agnostic.
 */

import { clamp } from "./math";

/** RGBA color, components in [0,1]. Alpha defaults to opaque where omitted. */
export type Rgba = [number, number, number, number];
/** RGBA where alpha may be omitted (treated as 1). */
export type RgbaLike = readonly [number, number, number, number?];

/** Build an RGBA tuple (floats in [0,1]); alpha defaults to 1. */
export const rgba = (r: number, g: number, b: number, a = 1): Rgba => [r, g, b, a];

/**
 * HSL → RGBA (floats in [0,1]). `h` is in degrees [0,360); `s`,`l`,`a` in [0,1].
 * Matches the corpus `hsl` exactly. Useful for evenly-spaced categorical
 * palettes (`hsl(i*360/n, 0.6, 0.55)`).
 */
export function hsl(h: number, s: number, l: number, a = 1): Rgba {
  const hn = h / 360;
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  const hue = (t: number): number => {
    t = ((t % 1) + 1) % 1;
    return t < 1 / 6
      ? p + (q - p) * 6 * t
      : t < 1 / 2
        ? q
        : t < 2 / 3
          ? p + (q - p) * (2 / 3 - t) * 6
          : p;
  };
  return [hue(hn + 1 / 3), hue(hn), hue(hn - 1 / 3), a];
}

/**
 * Quantize a single [0,1] float channel to a unorm8 byte [0,255] (round, then
 * clamp). This is the corpus `u8`.
 */
export const toByte = (x: number): number => clamp(Math.round(x * 255), 0, 255);

/**
 * Pack an RGBA color (floats in [0,1]) into four unorm8 bytes `[r,g,b,a]`.
 * Alpha defaults to 1 (255) when omitted. This is the byte layout the engine's
 * `*_color` vertex formats consume.
 */
export function packColorBytes(c: RgbaLike): [number, number, number, number] {
  return [toByte(c[0]), toByte(c[1]), toByte(c[2]), toByte(c[3] ?? 1)];
}

/**
 * Linearly interpolate between two colors per channel at `t` in [0,1]. Alpha is
 * interpolated too (missing alpha treated as 1). This is the per-channel lerp
 * the corpus gradient builders use to shade vertices.
 */
export function mixColor(c0: RgbaLike, c1: RgbaLike, t: number): Rgba {
  const a0 = c0[3] ?? 1;
  const a1 = c1[3] ?? 1;
  return [
    c0[0] + (c1[0] - c0[0]) * t,
    c0[1] + (c1[1] - c0[1]) * t,
    c0[2] + (c1[2] - c0[2]) * t,
    a0 + (a1 - a0) * t,
  ];
}
