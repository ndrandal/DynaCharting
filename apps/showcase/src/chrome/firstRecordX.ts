/* apps/showcase/src/chrome/firstRecordX.ts
 *
 * Decode the first replayed record's X value (record-index) for an xAnchor view,
 * so the overlay can reproduce the engine's runtime X re-anchor (mapping.ts). It
 * scans the view's records.json frames for the first APPEND record targeting the
 * growth buffer and reads the float at the x field offset — the same value
 * useReplay anchors on. Returns null when the view isn't xAnchored.
 */

import type { Records, GrowthSync } from '../engine/useReplay';

const OP_APPEND = 1;
const RECORD_HEADER_SIZE = 13;

function b64ToBytes(b64: string): Uint8Array {
  const bin = atob(b64);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return bytes;
}

/** First APPEND record's x for `growth.bufferId`, scanning frames in order. */
export function firstRecordX(records: Records | undefined, growth: GrowthSync | undefined): number | null {
  if (!records || !growth) return null;
  for (const frame of records.frames) {
    const bytes = b64ToBytes(frame.b64);
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    let o = 0;
    while (o + RECORD_HEADER_SIZE <= dv.byteLength) {
      const op = dv.getUint8(o);
      const bufId = dv.getUint32(o + 1, true);
      const payloadBytes = dv.getUint32(o + 9, true);
      o += RECORD_HEADER_SIZE;
      if (o + payloadBytes > dv.byteLength) break;
      if (op === OP_APPEND && bufId === growth.bufferId && payloadBytes >= growth.xField + 4) {
        return dv.getFloat32(o + growth.xField, true);
      }
      o += payloadBytes;
    }
  }
  return null;
}
