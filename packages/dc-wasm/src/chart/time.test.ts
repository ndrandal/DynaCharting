/* packages/dc-wasm/src/chart/time.test.ts — ENC-1254 (chart-quality-bar D1 tier 1)
 *
 * THE EVIDENCE for a time axis, and its NEGATIVE CONTROLS.
 *
 * DC-L01's lesson is that a check nobody has seen fail is not a check. So the
 * tier-1 predicate `parsesAsTimestamp` is asserted in both directions here: it
 * accepts every label the formatter emits, and it REJECTS the two things this
 * repo has actually shipped in a time axis's place — a record index (`INDEX
 * 4→160`) and elapsed m:ss.
 */
import { describe, it, expect } from "vitest";
import {
  TIME_STEPS,
  chooseTimeStep,
  timeTicks,
  timeLabelStyle,
  formatTimeTick,
  parsesAsTimestamp,
  decimalsForStep,
  decimalsForTicks,
  IndexTimeTracker,
  indexToTime,
  timeToIndex,
  timeDomainFor,
  timeBasisFromWire,
  transmittedBasisFromSceneInit,
  isSceneInitFrame,
  type TimeBasis,
} from "./time";

const SEC = 1000;
const MIN = 60 * SEC;
const HOUR = 60 * MIN;
const DAY = 24 * HOUR;

/** Build a dataplane batch: [1B op][4B bufId][4B off][4B len][payload]. */
function batch(bufferId: number, records: number[][], stride: number, offsetBytes = 0): ArrayBuffer {
  const payload = stride * records.length;
  const buf = new ArrayBuffer(13 + payload);
  const dv = new DataView(buf);
  dv.setUint8(0, 1); // append
  dv.setUint32(1, bufferId, true);
  dv.setUint32(5, offsetBytes, true);
  dv.setUint32(9, payload, true);
  records.forEach((rec, r) => {
    rec.forEach((v, f) => dv.setFloat32(13 + r * stride + f * 4, v, true));
  });
  return buf;
}

describe("the step ladder", () => {
  it("every rung divides its own unit's next unit up", () => {
    // This is the property that makes a coarser tick set a SUBSET of a finer
    // one, so a growing axis promotes rungs without re-phasing its ticks.
    const perUnit: Record<string, number> = {
      ms: 1000, // 1000 ms to the second
      second: 60,
      minute: 60,
      hour: 24,
      day: 0, // days per month is not fixed — excluded, see below
      month: 12,
      year: 0, // top of the ladder
    };
    for (const s of TIME_STEPS) {
      const per = perUnit[s.unit];
      if (per === 0) continue;
      expect(
        per % s.count,
        `${s.count}${s.unit} does not divide ${per} ${s.unit} per next unit`,
      ).toBe(0);
    }
  });

  it("is strictly ascending in nominal length", () => {
    for (let i = 1; i < TIME_STEPS.length; i++) {
      expect(TIME_STEPS[i].ms).toBeGreaterThan(TIME_STEPS[i - 1].ms);
    }
  });

  it("picks the SMALLEST rung that fits the target count — never more ticks than asked", () => {
    // 20 s of tape, 6 ticks asked: 5 s gives 4 intervals (fits); 2 s gives 10
    // (does not). The rule is smallest-that-fits, so it lands on 5 s.
    const s = chooseTimeStep(20 * SEC, 6);
    expect(s.unit).toBe("second");
    expect(s.count).toBe(5);
    expect(20 * SEC / s.ms).toBeLessThanOrEqual(6);

    // ...and the rung BELOW it would have overflowed the budget.
    const i = TIME_STEPS.findIndex((r) => r.unit === s.unit && r.count === s.count);
    expect(20 * SEC / TIME_STEPS[i - 1].ms).toBeGreaterThan(6);
  });

  it("walks seconds → minutes → hours → days as the span dictates", () => {
    const at = (span: number) => {
      const s = chooseTimeStep(span, 6);
      return `${s.count}${s.unit}`;
    };
    expect(at(3 * SEC)).toBe("500ms");
    expect(at(30 * SEC)).toBe("5second");
    expect(at(5 * MIN)).toBe("1minute");
    expect(at(90 * MIN)).toBe("15minute");
    expect(at(8 * HOUR)).toBe("2hour");
    expect(at(3 * DAY)).toBe("12hour");
    expect(at(30 * DAY)).toBe("7day");
    expect(at(400 * DAY)).toBe("3month");
    expect(at(20 * 365 * DAY)).toBe("5year");
  });

  it("does not run out at extreme spans", () => {
    // Below the ladder: a sub-millisecond domain gets the finest rung, not NaN.
    expect(chooseTimeStep(0.2, 6).ms).toBe(1);
    expect(chooseTimeStep(0, 6).ms).toBe(1);
    expect(chooseTimeStep(-5, 6).ms).toBe(1);
    expect(chooseTimeStep(Number.NaN, 6).ms).toBe(1);

    // Above it: the year step scales by powers of ten rather than giving up.
    const millennium = chooseTimeStep(5000 * 365 * DAY, 6);
    expect(millennium.unit).toBe("year");
    expect(millennium.count).toBe(1000);

    // And a genuinely absurd span still terminates with a finite step.
    const huge = chooseTimeStep(1e18, 6);
    expect(Number.isFinite(huge.ms)).toBe(true);
    expect(huge.ms).toBeGreaterThan(0);
  });
});

describe("tick placement", () => {
  it("lands on step boundaries, not on the domain edges", () => {
    // 14:32:07 → 14:32:41, 5 s steps. An INDEX axis would put its first tick at
    // :07; a TIME axis puts it at :10.
    const min = Date.UTC(2026, 8, 19, 14, 32, 7, 250);
    const max = Date.UTC(2026, 8, 19, 14, 32, 41, 0);
    const { ticks, step } = timeTicks({ min, max }, 8, { zone: "utc" });
    expect(`${step.count}${step.unit}`).toBe("5second");
    expect(ticks.map((t) => t.label)).toEqual([
      "14:32:10",
      "14:32:15",
      "14:32:20",
      "14:32:25",
      "14:32:30",
      "14:32:35",
      "14:32:40",
    ]);
    // No tick outside the domain.
    for (const t of ticks) {
      expect(t.ms).toBeGreaterThanOrEqual(min);
      expect(t.ms).toBeLessThanOrEqual(max);
    }
  });

  it("a coarser rung's ticks are a SUBSET of a finer rung's", () => {
    const min = Date.UTC(2026, 8, 19, 9, 0, 0);
    const max = Date.UTC(2026, 8, 19, 10, 0, 0);
    const fine = timeTicks({ min, max }, 60, {
      zone: "utc",
      step: TIME_STEPS.find((s) => s.unit === "minute" && s.count === 1)!,
    });
    const coarse = timeTicks({ min, max }, 60, {
      zone: "utc",
      step: TIME_STEPS.find((s) => s.unit === "minute" && s.count === 5)!,
    });
    const fineMs = new Set(fine.ticks.map((t) => t.ms));
    for (const t of coarse.ticks) expect(fineMs.has(t.ms)).toBe(true);
  });

  it("states no ticks for a domain it has no measurement of", () => {
    expect(timeTicks({ min: Number.NaN, max: 1 }, 6).ticks).toEqual([]);
    expect(timeTicks({ min: 10, max: 1 }, 6).ticks).toEqual([]); // inverted
    // A degenerate (single-instant) domain is a one-tick axis at most, not a hang.
    expect(timeTicks({ min: 0, max: 0 }, 6, { zone: "utc" }).ticks.length).toBeLessThanOrEqual(1);
  });

  it("bounds a pathological domain rather than emitting millions of ticks", () => {
    const { ticks } = timeTicks({ min: 0, max: 1e15 }, 1e9, { zone: "utc", maxTicks: 32 });
    expect(ticks.length).toBeLessThanOrEqual(32);
  });

  it("advances DAY steps by the calendar, so a DST day does not shift the clock", () => {
    // Stepping days by +86_400_000 ms across a 23-hour spring-forward day moves
    // every later tick to 23:00 of the previous day. Field arithmetic does not.
    const zone = "local";
    const start = new Date(2026, 2, 6, 0, 0, 0).getTime(); // 6 Mar, local
    const end = new Date(2026, 2, 20, 0, 0, 0).getTime();
    const { ticks, step } = timeTicks({ min: start, max: end }, 6, { zone });
    expect(step.unit).toBe("day");
    for (const t of ticks) {
      const d = new Date(t.ms);
      expect(d.getHours()).toBe(0);
      expect(d.getMinutes()).toBe(0);
    }
  });
});

describe("the label grammar", () => {
  it("spells out only as much calendar as the step and domain require", () => {
    const d = (h: number, m: number, s = 0) => Date.UTC(2026, 8, 19, h, m, s);
    const style = (min: number, max: number, target = 6) =>
      timeLabelStyle({ min, max }, chooseTimeStep(max - min, target), "utc");

    expect(style(d(9, 0), d(9, 0, 3))).toBe("time-ms");
    expect(style(d(9, 0), d(9, 0, 40))).toBe("time-second");
    expect(style(d(9, 0), d(11, 0))).toBe("time-minute");
    // Crossing a calendar day with a sub-day step must qualify the date, or two
    // ticks a day apart both read "09:30".
    expect(style(Date.UTC(2026, 8, 19, 20, 0), Date.UTC(2026, 8, 20, 4, 0))).toBe("date-time");
    expect(style(d(0, 0), d(0, 0) + 30 * DAY)).toBe("date");
    expect(style(d(0, 0), d(0, 0) + 400 * DAY)).toBe("month");
    expect(style(d(0, 0), d(0, 0) + 20 * 365 * DAY)).toBe("year");
  });

  it("is zero-padded, 24-hour and ISO-shaped in every style", () => {
    const t = Date.UTC(2026, 0, 2, 3, 4, 5, 6);
    expect(formatTimeTick(t, "year", "utc")).toBe("2026");
    expect(formatTimeTick(t, "month", "utc")).toBe("2026-01");
    expect(formatTimeTick(t, "date", "utc")).toBe("2026-01-02");
    expect(formatTimeTick(t, "date-time", "utc")).toBe("2026-01-02 03:04");
    expect(formatTimeTick(t, "time-minute", "utc")).toBe("03:04");
    expect(formatTimeTick(t, "time-second", "utc")).toBe("03:04:05");
    expect(formatTimeTick(t, "time-ms", "utc")).toBe("03:04:05.006");
    expect(formatTimeTick(Number.NaN, "time-second", "utc")).toBe("");
  });
});

describe("parsesAsTimestamp — D1's tier-1 predicate", () => {
  it("accepts every label the formatter can emit, across every style and step", () => {
    const styles = [
      "year",
      "month",
      "date",
      "date-time",
      "time-minute",
      "time-second",
      "time-ms",
    ] as const;
    const t = Date.UTC(2026, 8, 19, 14, 32, 5, 250);
    for (const s of styles) {
      expect(parsesAsTimestamp(formatTimeTick(t, s, "utc")), s).toBe(true);
    }
    // And over a real sweep of spans, every generated label parses.
    const spans = [2 * SEC, 45 * SEC, 20 * MIN, 9 * HOUR, 6 * DAY, 90 * DAY, 900 * DAY, 8000 * DAY];
    for (const span of spans) {
      const { ticks } = timeTicks({ min: Date.UTC(2026, 8, 19, 14, 32, 7), max: Date.UTC(2026, 8, 19, 14, 32, 7) + span }, 6, {
        zone: "utc",
      });
      expect(ticks.length, `span ${span} produced no ticks`).toBeGreaterThan(0);
      for (const tick of ticks) {
        expect(parsesAsTimestamp(tick.label), `${span}ms → ${tick.label}`).toBe(true);
      }
    }
  });

  it("REJECTS a record index — the INDEX 4→160 failure it exists to catch", () => {
    // These are the literal labels candles-aapl shipped for six months.
    for (const label of ["4", "30", "56", "82", "108", "134", "160", "162", "270"]) {
      expect(parsesAsTimestamp(label), label).toBe(false);
    }
  });

  it("REJECTS a record index PAST THREE DIGITS — the hole the old control stopped short of", () => {
    // ENC-1390 H1. The control above stops at "270", and `^(\d{4})$` returned
    // true unconditionally, so every 4-digit index was a "year": this predicate
    // PASSED the exact axis it exists to fail as soon as a view reached 1000
    // records. `footprint` / `depth-ladder` / `volume-profile` ship 42720 /
    // 10680 / 6408 of them, and `formatTick(v,'index')` is `String(Math.round(v))`.
    // A control set that stops exactly short of every real failure is the defect,
    // not an oversight — so this one runs to five digits.
    for (const label of ["1000", "1024", "1234", "4096", "6408", "9999", "10680", "42720"]) {
      expect(parsesAsTimestamp(label), label).toBe(false);
    }
  });

  it("REJECTS elapsed m:ss — a duration is not an instant", () => {
    for (const label of ["0:12", "1:05", "12:0", "0:00"]) {
      expect(parsesAsTimestamp(label), label).toBe(false);
    }
  });

  it("REJECTS elapsed m:ss AT AND ABOVE TEN MINUTES, where it puts on a zero pad", () => {
    // ENC-1390 H2. `0:12` is rejected for lacking a zero pad, not for being a
    // duration — so the control above only held below 600 s. `formatTick(v,'time')`
    // emits `10:00` at 600 s and `22:05` at 1325 s, and both are well-formed
    // clock times. The whole window a duration can forge is m = 10..23.
    for (const label of ["10:00", "10:30", "13:20", "16:40", "20:00", "22:05", "23:59"]) {
      expect(parsesAsTimestamp(label), label).toBe(false);
    }
  });

  it("REJECTS prices, percentages, junk and impossible dates", () => {
    for (const label of [
      "$408.00",
      "418",
      "+42%",
      "1.25",
      "",
      "  ",
      "Time",
      "2026-13-01", // month 13
      "2026-02-30", // 30 Feb
      "24:00", // hour 24
      "14:60", // minute 60
      "14:32:60", // second 60
      "2026-09-19T14:32:05.1234", // 4 fractional digits
    ]) {
      expect(parsesAsTimestamp(label), JSON.stringify(label)).toBe(false);
    }
  });

  it("accepts a leap day and rejects the same date in a common year", () => {
    expect(parsesAsTimestamp("2024-02-29")).toBe(true);
    expect(parsesAsTimestamp("2026-02-29")).toBe(false);
    expect(parsesAsTimestamp("2100-02-29")).toBe(false); // centurial non-leap
    expect(parsesAsTimestamp("2000-02-29")).toBe(true); // 400-divisible leap
  });
});

describe("derived decimal places (D12)", () => {
  it("gives a $185 equity and a sub-cent token DIFFERENT precision, from the step alone", () => {
    // The D12 case, in one assertion: no constant in the formatter knows which
    // of these it is looking at.
    expect(decimalsForStep(0.5)).toBe(1); // NEXO-shaped, $0.50 ticks
    expect(decimalsForStep(2)).toBe(0); // AAPL-shaped, $2 ticks
    expect(decimalsForStep(0.00002)).toBe(5); // sub-dollar pair
    expect(decimalsForStep(0.000002)).toBe(6);
  });

  it("survives the float error in log10 of an exact power of ten", () => {
    // Math.log10(0.001) === -2.9999999999999996 — without the epsilon this
    // returns 2 and silently drops a digit.
    expect(decimalsForStep(0.001)).toBe(3);
    expect(decimalsForStep(0.01)).toBe(2);
    expect(decimalsForStep(0.1)).toBe(1);
    expect(decimalsForStep(1)).toBe(0);
    expect(decimalsForStep(1000)).toBe(0);
  });

  it("clamps, and falls back to 2 for a step it cannot use", () => {
    expect(decimalsForStep(0)).toBe(2);
    expect(decimalsForStep(-1)).toBe(2);
    expect(decimalsForStep(Number.NaN)).toBe(2);
    expect(decimalsForStep(1e-30)).toBe(8);
  });

  it("decimalsForTicks finds the precision that keeps a tick set distinct", () => {
    expect(decimalsForTicks([410, 412, 414, 416, 418])).toBe(0);
    expect(decimalsForTicks([185.1, 185.2, 185.3])).toBe(1);
    expect(decimalsForTicks([0.000121, 0.000123, 0.000125])).toBe(6);
    expect(decimalsForTicks([1])).toBe(2); // not enough to tell
  });
});

describe("IndexTimeTracker — the basis is measured, not assumed", () => {
  const CANDLE = 10100;
  const STRIDE = 24;

  it("fits ms-per-record and the origin from the stream", () => {
    const tr = new IndexTimeTracker([{ bufferId: CANDLE, stride: STRIDE }], false);
    for (let i = 0; i < 20; i++) {
      tr.observe(batch(CANDLE, [[i, 1, 2, 3, 4, 0.4]], STRIDE, i * STRIDE), 1000 + i * 75);
    }
    const b = tr.basis()!;
    expect(b).not.toBeNull();
    expect(b.msPerIndex).toBeCloseTo(75, 9);
    expect(b.originMs).toBeCloseTo(1000, 6);
    expect(b.source).toBe("observed");
    expect(b.epochKnown).toBe(false);
    expect(b.samples).toBe(20);
  });

  it("states NO basis until the cadence is a measurement", () => {
    const tr = new IndexTimeTracker([{ bufferId: CANDLE, stride: STRIDE }], true);
    expect(tr.basis()).toBeNull(); // nothing observed
    tr.observe(batch(CANDLE, [[0, 1, 2, 3, 4, 0.4]], STRIDE), 1000);
    expect(tr.basis()).toBeNull(); // one record: cadence unknown
    tr.observe(batch(CANDLE, [[0, 1, 2, 3, 4, 0.4]], STRIDE, STRIDE), 1075);
    expect(tr.basis()).toBeNull(); // two records, SAME index: still unknown
    tr.observe(batch(CANDLE, [[1, 1, 2, 3, 4, 0.4]], STRIDE, 2 * STRIDE), 1150);
    expect(tr.basis()).not.toBeNull();
  });

  it("least squares does not let one late frame tilt the axis", () => {
    const twoPoint = (i0: number, t0: number, i1: number, t1: number) => (t1 - t0) / (i1 - i0);
    const tr = new IndexTimeTracker([{ bufferId: CANDLE, stride: STRIDE }], false);
    for (let i = 0; i < 40; i++) {
      // The LAST frame arrives 400 ms late (a stalled setTimeout / a GC pause).
      const late = i === 39 ? 400 : 0;
      tr.observe(batch(CANDLE, [[i, 1, 2, 3, 4, 0.4]], STRIDE, i * STRIDE), i * 75 + late);
    }
    const b = tr.basis()!;
    // A first/last two-point fit would read 85.3 ms/record — 13% high.
    expect(twoPoint(0, 0, 39, 39 * 75 + 400)).toBeCloseTo(85.26, 1);
    // Least squares over 40 samples absorbs it.
    expect(b.msPerIndex).toBeGreaterThan(75);
    expect(b.msPerIndex).toBeLessThan(77);
  });

  it("folds only its own buffers, and skips a truncated batch", () => {
    const tr = new IndexTimeTracker([{ bufferId: CANDLE, stride: STRIDE }], false);
    expect(tr.observe(batch(9999, [[0, 1, 2, 3, 4, 0.4]], STRIDE), 0)).toBe(0);
    const good = batch(CANDLE, [[0, 1, 2, 3, 4, 0.4]], STRIDE);
    const truncated = good.slice(0, 13 + 10);
    expect(tr.observe(truncated, 0)).toBe(0);
    expect(tr.samples).toBe(0);
  });

  it("reset() forgets the tape, so a replay loop refits rather than averaging two runs", () => {
    const tr = new IndexTimeTracker([{ bufferId: CANDLE, stride: STRIDE }], false);
    for (let i = 0; i < 5; i++) tr.observe(batch(CANDLE, [[i, 0, 0, 0, 0, 0]], STRIDE, i * STRIDE), i * 75);
    tr.reset();
    expect(tr.samples).toBe(0);
    expect(tr.basis()).toBeNull();
  });

  it("maps index ↔ time and carries an index domain into a time domain", () => {
    const b: TimeBasis = {
      originMs: 1000,
      msPerIndex: 75,
      source: "observed",
      epochKnown: false,
      samples: 10,
    };
    expect(indexToTime(b, 0)).toBe(1000);
    expect(indexToTime(b, 4)).toBe(1300);
    expect(timeToIndex(b, 1300)).toBe(4);
    expect(timeDomainFor(b, { min: 3.6, max: 270.4 })).toEqual({ min: 1270, max: 21280 });
    // A zero cadence cannot be inverted, and says so rather than returning 0.
    expect(timeToIndex({ ...b, msPerIndex: 0 }, 1300)).toBeNaN();
  });
});


/* ───────────────────────────────────────────────────────────────────────────
 * ENC-1282 — THE TRANSMITTED BASIS
 *
 * The input change: a fitted estimate becomes a measurement the producer took
 * where the bar was cut. These assert the two things that separate the new
 * input from the old one — that it is EXACT on the producer's grid, and that a
 * GAP in that grid stays a gap instead of shifting later bars earlier.
 * ─────────────────────────────────────────────────────────────────────────── */

/** A realistic wire basis: 1-minute bars from a real epoch. */
const WIRE = { baseMs: 1789862400000, periodMs: 60000, epochKnown: true };

describe("timeBasisFromWire — the producer's clock, not a fit", () => {
  it("maps baseMs/periodMs straight onto originMs/msPerIndex, with no samples", () => {
    const b = timeBasisFromWire(WIRE)!;
    expect(b).toEqual({
      originMs: 1789862400000,
      msPerIndex: 60000,
      source: "transmitted",
      epochKnown: true,
      samples: 0,
    });
  });

  it("is labelled 'transmitted', not 'declared' — a measurement is not a caption", () => {
    expect(timeBasisFromWire(WIRE)!.source).toBe("transmitted");
    expect(timeBasisFromWire(WIRE)!.source).not.toBe("declared");
  });

  it("carries a tape-relative producer statement through unchanged", () => {
    const b = timeBasisFromWire({ ...WIRE, baseMs: 0, epochKnown: false })!;
    expect(b.epochKnown).toBe(false);
    expect(b.originMs).toBe(0);
  });

  it("REFUSES periodMs 0 — a stream with no bar grid declares no basis at all", () => {
    // SPEC D7 corollary. A 0 here is a producer bug, and accepting it would
    // divide the axis by zero rather than drop it.
    expect(timeBasisFromWire({ ...WIRE, periodMs: 0 })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, periodMs: -60000 })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, periodMs: Number.NaN })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, periodMs: Number.POSITIVE_INFINITY })).toBeNull();
  });

  it("reads an ABSENT epochKnown as false, and refuses a present non-boolean", () => {
    // Field for field with customer-layer's `adaptHostTimeBasis` (ENC-1303),
    // the other reader of this same wire field: absent is proto3's `false`, and
    // false is the conservative direction — the client never upgrades a basis
    // to "real wall clock" without being told.
    const b = timeBasisFromWire({ baseMs: 0, periodMs: 60000 })!;
    expect(b.epochKnown).toBe(false);
    expect(b.source).toBe("transmitted");
    expect(timeBasisFromWire({ ...WIRE, epochKnown: "true" })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, epochKnown: 1 })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, epochKnown: null })).toBeNull();
  });

  it("REFUSES the protobuf-JSON int64-as-string form rather than coercing it", () => {
    // The expected drift, and the one `max_bytes`/`byteLength`/`maxBytes`
    // already cost this project once. Rejecting degrades to no basis.
    expect(timeBasisFromWire({ ...WIRE, baseMs: "1789862400000" })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, periodMs: "60000" })).toBeNull();
  });

  it("REFUSES a baseMs or periodMs that is not a safe integer, and every non-object", () => {
    expect(timeBasisFromWire({ ...WIRE, baseMs: 1.5 })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, baseMs: 2 ** 62 })).toBeNull();
    expect(timeBasisFromWire({ ...WIRE, periodMs: 60000.5 })).toBeNull();
    for (const v of [null, undefined, 0, "", [], "not json"]) {
      expect(timeBasisFromWire(v)).toBeNull();
    }
  });

  it("is EXACT where a float32 epoch would be quantised to ~2 minutes", () => {
    // D4's arithmetic: Math.fround of a current ms epoch loses ~131 s. The
    // basis never goes through the float32 lane; only the ordinal does.
    const b = timeBasisFromWire(WIRE)!;
    for (const x of [0, 1, 7, 1440, 525600]) {
      expect(indexToTime(b, x)).toBe(WIRE.baseMs + x * WIRE.periodMs);
    }
    // The float32 grid at a current epoch steps by 131072 ms = 2.18 min, so a
    // per-record float32 epoch could not express a 1-minute bar at all.
    expect(Math.fround(WIRE.baseMs + 16384) - Math.fround(WIRE.baseMs)).toBe(131072);
    expect(Math.fround(WIRE.baseMs)).not.toBe(WIRE.baseMs);
  });
});

describe("a gap in x is a GAP, not a shift (SPEC D7)", () => {
  // Bars 3 and 4 were quiet: TumblingWindow emitted nothing for them
  // (`EmptyBucketDoesNotEmit`). Five records are delivered, carrying ordinals
  // 0,1,2,5,6 — the count of records is 5, the count of bars elapsed is 7.
  const ordinals = [0, 1, 2, 5, 6];

  it("places every delivered bar at the instant the producer named", () => {
    const b = timeBasisFromWire(WIRE)!;
    expect(ordinals.map((x) => indexToTime(b, x))).toEqual([
      1789862400000, 1789862460000, 1789862520000, 1789862700000, 1789862760000,
    ]);
  });

  it("and the delivery-count reading puts the last bar 2 minutes early", () => {
    // The defect D7 removes, stated as a number: reading the x lane as "the
    // i-th record I received" drags every bar after a gap one periodMs earlier
    // per lost bar, permanently and undetectably.
    const b = timeBasisFromWire(WIRE)!;
    const byOrdinal = ordinals.map((x) => indexToTime(b, x));
    const byDelivery = ordinals.map((_, i) => indexToTime(b, i));
    expect(byOrdinal[4] - byDelivery[4]).toBe(2 * WIRE.periodMs);
    // ...and it is invisible at the head of the stream, which is why it stood.
    expect(byOrdinal[0] - byDelivery[0]).toBe(0);
  });

  it("a late joiner's first ordinal is not 0 and still lands correctly", () => {
    const b = timeBasisFromWire(WIRE)!;
    expect(indexToTime(b, 3600)).toBe(WIRE.baseMs + 3600 * WIRE.periodMs);
    expect(timeToIndex(b, WIRE.baseMs + 3600 * WIRE.periodMs)).toBe(3600);
  });
});

describe("transmittedBasisFromSceneInit — lifting the clock out of the envelope", () => {
  const envelope = {
    type: "scene-init",
    commands: [
      { cmd: "setClearColor", rgba: [0, 0, 0, 1] },
      { cmd: "createBuffer", id: 10100, byteLength: 0, timeBasis: WIRE },
      { cmd: "createBuffer", id: 10200, byteLength: 0 },
      { cmd: "createGeometry", id: 10300, vertexBufferId: 10100 },
    ],
  };

  it("reads the basis for the named buffer, from an object or its raw text", () => {
    expect(transmittedBasisFromSceneInit(envelope, 10100)!.msPerIndex).toBe(60000);
    expect(transmittedBasisFromSceneInit(JSON.stringify(envelope), 10100)!.epochKnown).toBe(true);
    expect(transmittedBasisFromSceneInit(envelope.commands, 10100)!.originMs).toBe(WIRE.baseMs);
  });

  it("matches by id, never by position", () => {
    // embassy emits createBuffer commands in sorted-id order, not in the order
    // a manifest declares them, so an index-based read is a coin flip.
    expect(transmittedBasisFromSceneInit(envelope, 10200)).toBeNull();
    expect(transmittedBasisFromSceneInit(envelope, 99999)).toBeNull();
  });

  it("does not borrow another buffer's clock for a buffer that declared none", () => {
    const reordered = {
      type: "scene-init",
      commands: [
        { cmd: "createBuffer", id: 10200, byteLength: 0 },
        { cmd: "createBuffer", id: 10100, byteLength: 0, timeBasis: WIRE },
      ],
    };
    expect(transmittedBasisFromSceneInit(reordered, 10200)).toBeNull();
  });

  it("with no bufferId, takes the first buffer that actually carries one", () => {
    expect(transmittedBasisFromSceneInit(envelope)!.source).toBe("transmitted");
    expect(
      transmittedBasisFromSceneInit({ type: "scene-init", commands: [{ cmd: "createBuffer", id: 1 }] }),
    ).toBeNull();
  });

  it("returns null for every not-an-envelope, including malformed JSON", () => {
    for (const v of ["", "{", "[", null, undefined, 7, { type: "scene-init" }, { commands: 3 }]) {
      expect(transmittedBasisFromSceneInit(v)).toBeNull();
    }
  });

  it("a malformed timeBasis drops the axis rather than captioning it", () => {
    const bad = {
      type: "scene-init",
      commands: [{ cmd: "createBuffer", id: 10100, timeBasis: { baseMs: 0, periodMs: 0, epochKnown: true } }],
    };
    expect(transmittedBasisFromSceneInit(bad, 10100)).toBeNull();
  });
});


describe("isSceneInitFrame — the gate that stops a good basis being retracted", () => {
  // "not a scene-init" and "a scene-init that declares no basis" are different
  // answers, and `transmittedBasisFromSceneInit` returns null for both. embassy
  // sends two other kinds of text frame on the same socket, so a consumer that
  // conflates them drops the axis at connect and then four times a second.
  it("accepts the envelope, as an object or as its raw text", () => {
    expect(isSceneInitFrame({ type: "scene-init", commands: [] })).toBe(true);
    expect(isSceneInitFrame('{"type":"scene-init","commands":[]}')).toBe(true);
  });

  it("rejects the OTHER text frames embassy actually sends on this socket", () => {
    // sticky growth counts, replayed after the envelope on every subscribe
    expect(isSceneInitFrame('{"cmd":"setGeometryVertexCount","id":10200,"vertexCount":211}')).toBe(
      false,
    );
    // the range tracker's live re-frame, ~250 ms cadence
    expect(isSceneInitFrame('{"cmd":"setTransform","id":10050,"sx":0.08,"tx":-1}')).toBe(false);
  });

  it("rejects a bare command array — unwrapping the envelope IS the decision", () => {
    expect(isSceneInitFrame([{ cmd: "createBuffer", id: 10100, timeBasis: WIRE }])).toBe(false);
  });

  it("rejects malformed JSON and every non-envelope", () => {
    for (const v of ["", "{", "not json", null, undefined, 7, [], { type: "other" }, {}]) {
      expect(isSceneInitFrame(v)).toBe(false);
    }
  });
});
