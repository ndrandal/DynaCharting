/* packages/dc-wasm/src/chart/domain.test.ts — ENC-1252 (chart-quality-bar D7)
 *
 * The property under test is the one the acceptance criterion names: the domain
 * TRACKS THE DATA. Every assertion here is of the form "fold these bytes, get
 * this number back", and several are explicitly "fold DIFFERENT bytes, get a
 * DIFFERENT number back" — which is the thing a hand-typed literal can never do.
 */
import { describe, it, expect } from "vitest";
import {
  DomainTracker,
  RECORD_LAYOUTS,
  applyDomainPolicy,
  type DomainSource,
} from "./domain";

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;
const HEADER = 13;

/** Build one dataplane batch containing a single record block. */
function batch(op: number, bufferId: number, offsetBytes: number, floats: number[]): ArrayBuffer {
  const payload = new Float32Array(floats);
  const buf = new ArrayBuffer(HEADER + payload.byteLength);
  const dv = new DataView(buf);
  dv.setUint8(0, op);
  dv.setUint32(1, bufferId, true);
  dv.setUint32(5, offsetBytes, true);
  dv.setUint32(9, payload.byteLength, true);
  new Uint8Array(buf, HEADER).set(new Uint8Array(payload.buffer));
  return buf;
}

/** candle6 record: [x, open, high, low, close, halfWidth]. */
function candle(x: number, o: number, h: number, l: number, c: number, hw = 0.4): number[] {
  return [x, o, h, l, c, hw];
}

const CANDLES: DomainSource[] = [{ bufferId: 10100, format: "candle6" }];

describe("RECORD_LAYOUTS", () => {
  it("matches the engine's strideOf(VertexFormat)", () => {
    // core/include/dc/scene/Geometry.hpp
    expect(RECORD_LAYOUTS.candle6.stride).toBe(24);
    expect(RECORD_LAYOUTS.rect4.stride).toBe(16);
    expect(RECORD_LAYOUTS.rect4_color.stride).toBe(24);
    expect(RECORD_LAYOUTS.pos2_clip.stride).toBe(8);
    expect(RECORD_LAYOUTS.pos2_alpha.stride).toBe(12);
    expect(RECORD_LAYOUTS.pos2_color4.stride).toBe(24);
    expect(RECORD_LAYOUTS.pos2_uv4.stride).toBe(16);
    expect(RECORD_LAYOUTS.point4_color.stride).toBe(16);
    expect(RECORD_LAYOUTS.glyph8.stride).toBe(32);
  });

  it("keeps every declared field offset inside its own record", () => {
    for (const [name, l] of Object.entries(RECORD_LAYOUTS)) {
      for (const off of [...l.x, ...l.y, ...(l.xHalfWidth !== undefined ? [l.xHalfWidth] : [])]) {
        expect(off + 4, `${name} field @${off}`).toBeLessThanOrEqual(l.stride);
      }
    }
  });
});

describe("DomainTracker — states nothing until it has measured something", () => {
  it("reports null domains before any record", () => {
    const t = new DomainTracker(CANDLES);
    const d = t.domain();
    expect(d.x).toBeNull();
    expect(d.y).toBeNull();
    expect(d.records).toBe(0);
  });

  it("ignores buffers outside its axis group (embassy Observe semantics)", () => {
    const t = new DomainTracker(CANDLES);
    // A volume sub-pane streaming on 10130 must not widen the price axis.
    expect(t.observe(batch(OP_APPEND, 10130, 0, [0, 0, 160000, 1]))).toBe(0);
    expect(t.domain().y).toBeNull();
  });
});

describe("DomainTracker — the domain is the data", () => {
  it("takes y from the candle's OHLC and x from centre ± halfWidth", () => {
    const t = new DomainTracker(CANDLES);
    t.observe(
      batch(OP_APPEND, 10100, 0, [
        ...candle(10, 100, 105, 99, 103, 0.4),
        ...candle(11, 103, 108, 102, 107, 0.4),
      ]),
    );
    const d = t.raw();
    expect(d.records).toBe(2);
    expect(d.y).toEqual({ min: 99, max: 108 });
    // The domain of a bar is the bar, not its centre line.
    expect(d.x!.min).toBeCloseTo(9.6, 5);
    expect(d.x!.max).toBeCloseTo(11.4, 5);
  });

  it("grows the domain as data streams in — the whole point of D7", () => {
    const t = new DomainTracker(CANDLES);
    t.observe(batch(OP_APPEND, 10100, 0, candle(0, 100, 101, 99, 100)));
    const first = t.raw();
    expect(first.y).toEqual({ min: 99, max: 101 });

    t.observe(batch(OP_APPEND, 10100, 24, candle(1, 100, 140, 60, 70)));
    const second = t.raw();
    expect(second.y).toEqual({ min: 60, max: 140 });
    expect(second.y).not.toEqual(first.y);
    expect(second.x!.max).toBeGreaterThan(first.x!.max);
  });

  it("changing the DATA changes the DOMAIN (a literal could not)", () => {
    const mk = (base: number) => {
      const t = new DomainTracker(CANDLES);
      for (let i = 0; i < 8; i++) {
        t.observe(
          batch(OP_APPEND, 10100, i * 24, candle(i, base + i, base + i + 2, base + i - 2, base + i)),
        );
      }
      return t.raw();
    };
    const a = mk(400);
    const b = mk(31500); // same shape, different instrument
    expect(a.y).not.toEqual(b.y);
    expect(b.y!.min).toBeCloseTo(a.y!.min + 31100, 3);
    expect(b.y!.max).toBeCloseTo(a.y!.max + 31100, 3);
    // …and the x domain is identical, because the x data is identical.
    expect(b.x).toEqual(a.x);
  });

  it("folds only the new bytes (O(Δ)) — 100 batches, 100 records", () => {
    const t = new DomainTracker(CANDLES);
    for (let i = 0; i < 100; i++) {
      expect(t.observe(batch(OP_APPEND, 10100, i * 24, candle(i, 10, 12, 8, 11)))).toBe(1);
    }
    expect(t.records).toBe(100);
    expect(t.raw().y).toEqual({ min: 8, max: 12 });
  });

  it("reset() forgets everything (a replay loop restarting on a fresh buffer)", () => {
    const t = new DomainTracker(CANDLES);
    t.observe(batch(OP_APPEND, 10100, 0, candle(0, 100, 200, 50, 150)));
    t.reset();
    expect(t.domain().y).toBeNull();
    expect(t.records).toBe(0);
    t.observe(batch(OP_APPEND, 10100, 0, candle(0, 1, 2, 0.5, 1.5)));
    expect(t.raw().y).toEqual({ min: 0.5, max: 2 });
  });
});

describe("DomainTracker — wire-format handling", () => {
  it("reads multiple record blocks and multiple buffers in one batch", () => {
    // Two blocks concatenated: candles on 10100, an SMA rect4 on 10120.
    const a = new Uint8Array(batch(OP_APPEND, 10100, 0, candle(5, 100, 110, 90, 105)));
    const b = new Uint8Array(batch(OP_APPEND, 10120, 0, [5, 101, 6, 112]));
    const merged = new Uint8Array(a.byteLength + b.byteLength);
    merged.set(a, 0);
    merged.set(b, a.byteLength);

    const t = new DomainTracker([
      { bufferId: 10100, format: "candle6" },
      { bufferId: 10120, format: "rect4" },
    ]);
    expect(t.observe(merged.buffer)).toBe(2);
    // rect4 pushes y to 112 and x to 6; candle6 pushes y down to 90.
    expect(t.raw().y).toEqual({ min: 90, max: 112 });
    expect(t.raw().x!.max).toBe(6);
  });

  it("honours updateRange's offset phase instead of misreading the record grid", () => {
    const t = new DomainTracker(CANDLES);
    // A 4-byte-misaligned updateRange: the leading fragment is NOT a record
    // start. Skipping it is the only way to avoid reading `open` as `x`.
    const floats = [999, ...candle(3, 100, 110, 90, 105)];
    t.observe(batch(OP_UPDATE_RANGE, 10100, 20, floats));
    const d = t.raw();
    expect(d.records).toBe(1);
    expect(d.y).toEqual({ min: 90, max: 110 });
    expect(d.x!.min).toBeCloseTo(2.6, 5);
  });

  it("drops a truncated trailing record rather than reading past it", () => {
    const full = new Uint8Array(batch(OP_APPEND, 10100, 0, candle(1, 10, 12, 8, 11)));
    const cut = full.slice(0, full.byteLength - 4); // payloadBytes now lies
    const t = new DomainTracker(CANDLES);
    expect(t.observe(cut.buffer)).toBe(0);
    expect(t.domain().y).toBeNull();
  });

  it("never lets a NaN or Infinity widen the domain", () => {
    const t = new DomainTracker(CANDLES);
    t.observe(batch(OP_APPEND, 10100, 0, candle(1, 10, 12, 8, 11)));
    t.observe(batch(OP_APPEND, 10100, 24, candle(2, NaN, Infinity, -Infinity, NaN)));
    expect(t.raw().y).toEqual({ min: 8, max: 12 });
  });

  it("accepts an ArrayBufferView over a larger buffer", () => {
    const one = new Uint8Array(batch(OP_APPEND, 10100, 0, candle(1, 10, 12, 8, 11)));
    const padded = new Uint8Array(8 + one.byteLength);
    padded.set(one, 8);
    const t = new DomainTracker(CANDLES);
    expect(t.observe(padded.subarray(8))).toBe(1);
    expect(t.raw().y).toEqual({ min: 8, max: 12 });
  });

  it("refuses to guess an unknown record layout", () => {
    expect(() => new DomainTracker([{ bufferId: 1, format: "nope" }])).toThrow(/not a known record layout/);
  });

  it("respects an axes:'x' source — a volume pane shares X, never Y", () => {
    const t = new DomainTracker([
      { bufferId: 10100, format: "candle6" },
      { bufferId: 10130, format: "rect4", axes: "x" },
    ]);
    t.observe(batch(OP_APPEND, 10100, 0, candle(5, 100, 110, 90, 105)));
    t.observe(batch(OP_APPEND, 10130, 0, [20, 0, 21, 160000]));
    expect(t.raw().y).toEqual({ min: 90, max: 110 }); // volume did NOT widen price
    expect(t.raw().x!.max).toBe(21);
  });
});

describe("applyDomainPolicy", () => {
  it("is the identity on a healthy range", () => {
    expect(applyDomainPolicy({ min: 408, max: 418 })).toEqual({ min: 408, max: 418 });
  });

  it("widens a collapsed range so a downstream fit can never divide by zero", () => {
    const d = applyDomainPolicy({ min: 100, max: 100 });
    expect(d.max).toBeGreaterThan(d.min);
    // floor = max(|100| * 0.005, 0.01) = 0.5 per side
    expect(d.min).toBeCloseTo(99.5, 9);
    expect(d.max).toBeCloseTo(100.5, 9);
  });

  it("uses the absolute floor near zero", () => {
    const d = applyDomainPolicy({ min: 0, max: 0 });
    expect(d).toEqual({ min: -0.01, max: 0.01 });
  });

  it("pads only when asked (a reported domain is unpadded)", () => {
    expect(applyDomainPolicy({ min: 0, max: 10 })).toEqual({ min: 0, max: 10 });
    expect(applyDomainPolicy({ min: 0, max: 10 }, { paddingFrac: 0.1 })).toEqual({ min: -1, max: 11 });
  });

  it("orders an inverted range", () => {
    expect(applyDomainPolicy({ min: 20, max: 5 })).toEqual({ min: 5, max: 20 });
  });

  it("passes non-finite input through untouched rather than laundering it", () => {
    const d = applyDomainPolicy({ min: NaN, max: 4 });
    expect(Number.isNaN(d.min)).toBe(true);
  });
});
