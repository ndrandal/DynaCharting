/* packages/authoring-kit/src/packing.ts — ENC-714
 *
 * Vertex-buffer packing helpers. `f32` builds a tightly-packed Float32 buffer
 * for the all-float formats (pos2, rect4, candle6, pos2_color4). `packMixed`
 * builds the interleaved layout for formats that mix f32 lanes with ONE
 * unorm8x4 color lane (rect4_color, point4_color) — where the color occupies a
 * single 4-byte slot, not four floats.
 *
 * Ported from the authoring-corpus runner (`f32`, `packMixed`). Pure &
 * framework-agnostic; little-endian (WebGPU/engine convention).
 */

import { toByte } from "./color";
import type { RgbaLike } from "./color";

/** Build a tightly-packed Float32Array from a flat number list. */
export const f32 = (arr: ArrayLike<number>): Float32Array => new Float32Array(arr);

/**
 * One record for {@link packMixed}: `beforeN` float lanes, then one unorm8x4
 * color, then `afterN` float lanes.
 */
export interface MixedRecord {
  /** float lanes emitted BEFORE the color (length must be >= beforeN). */
  floats: ArrayLike<number>;
  /** the color lane, packed to unorm8x4 (alpha defaults to 1). */
  rgba: RgbaLike;
  /** float lanes emitted AFTER the color (length must be >= afterN). */
  floatsAfter?: ArrayLike<number>;
}

/**
 * Pack records for a mixed format: `beforeN` little-endian f32 lanes, one
 * unorm8x4 color (4 bytes), then `afterN` little-endian f32 lanes, per record.
 * Stride is `(beforeN + 1 + afterN) * 4` bytes. Returns a `Uint8Array`.
 *
 * Example — rect4_color (x0,y0,x1,y1 + color): `packMixed(recs, 4, 0)`.
 */
export function packMixed(
  records: readonly MixedRecord[],
  beforeN: number,
  afterN: number,
): Uint8Array {
  const stride = (beforeN + 1 + afterN) * 4; // bytes
  const buf = new Uint8Array(records.length * stride);
  const dv = new DataView(buf.buffer);
  let o = 0;
  for (const rec of records) {
    for (let i = 0; i < beforeN; i++) {
      dv.setFloat32(o, rec.floats[i], true);
      o += 4;
    }
    dv.setUint8(o, toByte(rec.rgba[0]));
    dv.setUint8(o + 1, toByte(rec.rgba[1]));
    dv.setUint8(o + 2, toByte(rec.rgba[2]));
    dv.setUint8(o + 3, toByte(rec.rgba[3] ?? 1));
    o += 4;
    const after = rec.floatsAfter;
    for (let i = 0; i < afterN; i++) {
      dv.setFloat32(o, after ? after[i] : 0, true);
      o += 4;
    }
  }
  return buf;
}
