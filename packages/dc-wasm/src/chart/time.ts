/* packages/dc-wasm/src/chart/time.ts — ENC-1254 (chart-quality-bar SPEC D1 tier 1)
 *
 * TIME RENDERS AS TIME.
 *
 * Tier 1 ("Legible") requires that a reader can take a value off the chart and
 * that "time is rendered as time". The showcase's x axis was labelled `INDEX,
 * 4 → 160` on a market time series for six months and nobody flinched, because
 * with a hand-typed axis there is no ground truth in the frame to contradict it
 * (SPEC §1.3). ENC-1252 made the *domain* a measurement; this module makes the
 * *labels* one.
 *
 * Three things live here, and they are separate on purpose:
 *
 *   1. THE STEP LADDER + TICK GENERATOR  (`chooseTimeStep`, `timeTicks`)
 *   2. THE LABEL GRAMMAR                 (`formatTimeTick`, `parsesAsTimestamp`)
 *   3. THE INDEX → TIME BASIS            (`IndexTimeTracker`, `indexToTime`)
 *
 * (1) and (2) are pure functions over epoch milliseconds. They do not know
 * whether an SVG overlay, a WebGPU text pipeline or a PNG exporter will draw
 * the result — which matters, because ENC-1253 moves the drawing into the
 * engine and must not have to reimplement the formatting.
 *
 * ── WHY A LADDER AND NOT `niceTicks` ─────────────────────────────────────────
 * `niceTicks` snaps a step to 1/2/5 × 10ⁿ, which is correct for a decimal
 * quantity and wrong for time. Time is not decimal: 1000 ms to the second, 60 s
 * to the minute, 60 min to the hour, 24 h to the day. A decimal ladder produces
 * 20 s and 200 s steps, whose ticks land at :20, :40, 3:20, 6:40 — off every
 * boundary a human reads a clock by. Every rung of TIME_STEPS below divides its
 * own next unit, so an aligned tick always lands on a whole second / minute /
 * hour / day, and promoting to the next rung as the span grows yields a SUBSET
 * of the previous tick set rather than re-phasing it. That is the property that
 * makes a live, growing axis stop jittering.
 *
 * ── WHY THE PRECISION IS DERIVED, NOT CONSTANT (D12) ─────────────────────────
 * ENC-1260's D12 is binding here: the decimal places and the time step come
 * from the OBSERVED domain, never from an equity-shaped constant.
 * `feed-simulator`'s universe is 30 fictional equity tickers and will never get
 * crypto pairs (SPEC §2 D12 / §5 Q5), so this code is written and tuned against
 * a ~$185 fictional equity — and crypto, which is what the platform sells, is
 * shaped differently: 24/7 with no session gap, and pairs spanning five orders
 * of magnitude. A formatter that hardcodes two decimals or assumes a session
 * boundary under-specifies silently the moment the symbol changes, and there is
 * no axis ground truth in the frame to catch it. So: no session hours anywhere
 * in this file, and `decimalsForStep` is a function of the tick step.
 *
 * ── WHAT "A TIMESTAMP" MEANS HERE ────────────────────────────────────────────
 * D1's tier-1 check is "assert axis tick text parses as a timestamp".
 * `parsesAsTimestamp` is that predicate, exported so the scorer (ENC-1261) and
 * the engine-side axis (ENC-1253) apply the SAME one. It deliberately REJECTS
 * the two things this repo has shipped in a time axis's place:
 *
 *   '162'    — a record index (the INDEX 4→160 failure)
 *   '0:12'   — elapsed m:ss (a DURATION, not an instant; the old `time` format)
 *
 * A check that cannot fail is not a check, so both are pinned as negative
 * controls in time.test.ts.
 */

import type { Range } from "./scale";

// ─────────────────────────────────────────────────────────────────────────────
// 1. The step ladder
// ─────────────────────────────────────────────────────────────────────────────

/** The calendar unit a step is counted in. */
export type TimeUnit = "ms" | "second" | "minute" | "hour" | "day" | "month" | "year";

/** One rung of the ladder: `count` × `unit`, with its nominal length in ms. */
export interface TimeStep {
  unit: TimeUnit;
  count: number;
  /**
   * Nominal length in ms. EXACT for ms/second/minute/hour; NOMINAL for
   * day/month/year, which are calendar quantities (a DST day is 23 or 25 h, a
   * month is 28–31 days). `timeTicks` advances those by calendar arithmetic,
   * not by adding this number — it is used only to CHOOSE a rung.
   */
  ms: number;
}

const MS = 1;
const SEC = 1000;
const MIN = 60 * SEC;
const HOUR = 60 * MIN;
const DAY = 24 * HOUR;
const MONTH = 30.436875 * DAY; // mean Gregorian month — for rung selection only
const YEAR = 365.2425 * DAY; // mean Gregorian year — for rung selection only

function rung(unit: TimeUnit, count: number, unitMs: number): TimeStep {
  return { unit, count, ms: count * unitMs };
}

/**
 * The ladder, ascending. Every rung divides its own unit's next unit up
 * (1000 ms / 60 s / 60 min / 24 h / 12 months), so aligned ticks land on whole
 * boundaries and a coarser rung's ticks are a subset of a finer one's.
 *
 * Spans wider than the last rung are handled by scaling the year step by powers
 * of ten (see `chooseTimeStep`) — the ladder does not need a rung per geological
 * era to stay correct.
 */
export const TIME_STEPS: readonly TimeStep[] = [
  rung("ms", 1, MS),
  rung("ms", 2, MS),
  rung("ms", 5, MS),
  rung("ms", 10, MS),
  rung("ms", 20, MS),
  rung("ms", 25, MS),
  rung("ms", 50, MS),
  rung("ms", 100, MS),
  rung("ms", 200, MS),
  rung("ms", 250, MS),
  rung("ms", 500, MS),
  rung("second", 1, SEC),
  rung("second", 2, SEC),
  rung("second", 5, SEC),
  rung("second", 10, SEC),
  rung("second", 15, SEC),
  rung("second", 30, SEC),
  rung("minute", 1, MIN),
  rung("minute", 2, MIN),
  rung("minute", 5, MIN),
  rung("minute", 10, MIN),
  rung("minute", 15, MIN),
  rung("minute", 30, MIN),
  rung("hour", 1, HOUR),
  rung("hour", 2, HOUR),
  rung("hour", 3, HOUR),
  rung("hour", 4, HOUR),
  rung("hour", 6, HOUR),
  rung("hour", 12, HOUR),
  rung("day", 1, DAY),
  rung("day", 2, DAY),
  rung("day", 7, DAY),
  rung("day", 14, DAY),
  rung("month", 1, MONTH),
  rung("month", 3, MONTH),
  rung("month", 6, MONTH),
  rung("year", 1, YEAR),
  rung("year", 2, YEAR),
  rung("year", 5, YEAR),
  rung("year", 10, YEAR),
];

/**
 * Pick the step for a span.
 *
 * THE RULE: the SMALLEST rung whose step divides the span into at most
 * `targetTicks` intervals. Smallest-that-fits (rather than nearest) is the
 * deliberate choice — it guarantees the axis never shows MORE than the ticks it
 * was asked for, which is the failure that produces collided labels (tier 1's
 * other clause), while showing fewer is merely sparse.
 *
 * Degenerate spans (0, negative, non-finite) return the finest rung: an axis
 * over a single instant is a one-tick axis, not an error.
 *
 * Spans beyond the last rung scale the 10-year step by powers of ten, so a
 * century-wide or millennium-wide domain still gets round years.
 */
export function chooseTimeStep(spanMs: number, targetTicks: number): TimeStep {
  const target = Number.isFinite(targetTicks) && targetTicks >= 1 ? Math.floor(targetTicks) : 1;
  if (!Number.isFinite(spanMs) || spanMs <= 0) return TIME_STEPS[0];
  for (const s of TIME_STEPS) {
    if (spanMs / s.ms <= target) return s;
  }
  // Past the ladder: keep multiplying the year count by 10 until it fits.
  let years = 10;
  while (spanMs / (years * YEAR) > target && years < 1e9) years *= 10;
  return rung("year", years, YEAR);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. Calendar arithmetic (zone-aware, DST-correct)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Which clock the labels and the tick alignment are expressed in.
 *
 * 'local' — the viewer's zone. Correct for a live chart: a trader reads their
 *           own wall clock.
 * 'utc'   — correct when the time basis has no real epoch (a replayed capture,
 *           whose origin is the tape's own zero — see `TimeBasis.epochKnown`),
 *           because rendering a tape offset through a local zone offset would
 *           shift 0 to 19:00 and invent a claim the tape does not make.
 */
export type TimeZoneMode = "local" | "utc";

/** The calendar fields of an instant, in the requested zone. */
interface Parts {
  year: number;
  month: number; // 1-12
  day: number; // 1-31
  hour: number;
  minute: number;
  second: number;
  ms: number;
}

function partsOf(ms: number, zone: TimeZoneMode): Parts {
  const d = new Date(ms);
  return zone === "utc"
    ? {
        year: d.getUTCFullYear(),
        month: d.getUTCMonth() + 1,
        day: d.getUTCDate(),
        hour: d.getUTCHours(),
        minute: d.getUTCMinutes(),
        second: d.getUTCSeconds(),
        ms: d.getUTCMilliseconds(),
      }
    : {
        year: d.getFullYear(),
        month: d.getMonth() + 1,
        day: d.getDate(),
        hour: d.getHours(),
        minute: d.getMinutes(),
        second: d.getSeconds(),
        ms: d.getMilliseconds(),
      };
}

function fromParts(p: Parts, zone: TimeZoneMode): number {
  if (zone === "utc") {
    return Date.UTC(p.year, p.month - 1, p.day, p.hour, p.minute, p.second, p.ms);
  }
  return new Date(p.year, p.month - 1, p.day, p.hour, p.minute, p.second, p.ms).getTime();
}

/** Midnight of the calendar day containing `ms`, in the requested zone. */
function startOfDay(ms: number, zone: TimeZoneMode): number {
  const p = partsOf(ms, zone);
  return fromParts({ ...p, hour: 0, minute: 0, second: 0, ms: 0 }, zone);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. Tick generation
// ─────────────────────────────────────────────────────────────────────────────

/** One tick: the instant, and the label for it. */
export interface TimeTick {
  /** Epoch ms of the tick. */
  ms: number;
  /** The formatted label. */
  label: string;
}

/** How much of the calendar a label must spell out to be unambiguous. */
export type TimeLabelStyle =
  | "time-ms" /** 14:32:05.250 */
  | "time-second" /** 14:32:05 */
  | "time-minute" /** 14:32 */
  | "date-time" /** 2026-09-19 14:32 — the domain crosses a day boundary */
  | "date" /** 2026-09-19 */
  | "month" /** 2026-09 */
  | "year" /** 2026 */;

/**
 * Every style, so a caller can ask "is this label ANY rendering of this
 * instant?" without hand-listing them — which is how the fixed list in
 * `time.test.ts` used to drift behind the union.
 */
export const TIME_LABEL_STYLES: readonly TimeLabelStyle[] = [
  "time-ms",
  "time-second",
  "time-minute",
  "date-time",
  "date",
  "month",
  "year",
];

/**
 * The label style for a (domain, step) pair.
 *
 * The step sets the RESOLUTION (you do not print seconds on an hourly axis),
 * and the domain sets the SCOPE: a sub-day step over a domain that crosses a
 * calendar day must spell the date out, or two ticks a day apart both read
 * `09:30` and the axis is ambiguous — which is the same class of defect as
 * labelling an index `INDEX`, just less obvious.
 */
export function timeLabelStyle(domain: Range, step: TimeStep, zone: TimeZoneMode = "local"): TimeLabelStyle {
  switch (step.unit) {
    case "year":
      return "year";
    case "month":
      return "month";
    case "day":
      return "date";
    default:
      break;
  }
  const crossesDay =
    Number.isFinite(domain.min) &&
    Number.isFinite(domain.max) &&
    startOfDay(domain.min, zone) !== startOfDay(domain.max, zone);
  if (crossesDay) return "date-time";
  if (step.unit === "ms") return "time-ms";
  if (step.unit === "second") return "time-second";
  return "time-minute";
}

const p2 = (n: number) => String(Math.trunc(n)).padStart(2, "0");
const p3 = (n: number) => String(Math.trunc(n)).padStart(3, "0");

/**
 * The YEAR field.
 *
 * 0000-9999 is the ordinary 4-digit form. Outside it — which a degenerate
 * `IndexTimeTracker` basis can reach — this emits the ECMAScript / ISO-8601
 * EXPANDED form: a sign and six digits, `+010000` / `-000500`, the same shape
 * `Date.prototype.toISOString` uses.
 *
 * This used to be `padStart(4, "0")` while the header claimed years were clamped
 * "by padding/truncation". Padding a 5-digit year does nothing, so the formatter
 * emitted `10000-01-01` and `-500-01-01` — and `parsesAsTimestamp`, its own
 * predicate, REJECTED both (ENC-1390). The two have to be closed over each
 * other: a producer whose output its own checker fails is a checker that has
 * never been run on the producer. Truncating instead would have been worse than
 * the bug — year 10000 printed as `0000` is a wrong answer, not a clamped one.
 */
const yearField = (y: number): string => {
  const t = Math.trunc(y);
  if (t >= 0 && t <= 9999) return String(t).padStart(4, "0");
  return (t < 0 ? "-" : "+") + String(Math.abs(t)).padStart(6, "0");
};

/**
 * Format one instant in a given style.
 *
 * The grammar is ISO-8601-shaped (`YYYY-MM-DD`, `HH:MM:SS.mmm`, zero-padded,
 * 24-hour) rather than locale-formatted, for three reasons: it is unambiguous
 * in every locale, it sorts lexicographically, and it is machine-checkable —
 * `parsesAsTimestamp` below is D1's tier-1 assertion and it needs a grammar,
 * not a best-effort `Date.parse`.
 *
 * Years outside 0000-9999 are emitted in the ECMAScript / ISO-8601 EXPANDED
 * form (`+010000`, `-000500`) rather than clamped — `yearField` above, and the
 * grammar accepts exactly what it emits (ENC-1390).
 */
export function formatTimeTick(ms: number, style: TimeLabelStyle, zone: TimeZoneMode = "local"): string {
  if (!Number.isFinite(ms)) return "";
  const p = partsOf(ms, zone);
  const date = `${yearField(p.year)}-${p2(p.month)}-${p2(p.day)}`;
  switch (style) {
    case "year":
      return yearField(p.year);
    case "month":
      return `${yearField(p.year)}-${p2(p.month)}`;
    case "date":
      return date;
    case "date-time":
      return `${date} ${p2(p.hour)}:${p2(p.minute)}`;
    case "time-ms":
      return `${p2(p.hour)}:${p2(p.minute)}:${p2(p.second)}.${p3(p.ms)}`;
    case "time-second":
      return `${p2(p.hour)}:${p2(p.minute)}:${p2(p.second)}`;
    case "time-minute":
    default:
      return `${p2(p.hour)}:${p2(p.minute)}`;
  }
}

/** Advance one aligned instant to the next one, `step` later, in `zone`. */
function advance(ms: number, step: TimeStep, zone: TimeZoneMode): number {
  if (step.unit === "year" || step.unit === "month" || step.unit === "day") {
    // Calendar units are advanced by FIELD, not by adding milliseconds: a DST
    // day is 23 or 25 hours long and a month is 28-31 days, so `+ step.ms`
    // would drift the wall-clock time of every subsequent tick.
    const p = partsOf(ms, zone);
    if (step.unit === "year") return fromParts({ ...p, year: p.year + step.count }, zone);
    if (step.unit === "month") {
      const m0 = p.year * 12 + (p.month - 1) + step.count;
      return fromParts({ ...p, year: Math.floor(m0 / 12), month: (m0 % 12) + 1 }, zone);
    }
    return fromParts({ ...p, day: p.day + step.count }, zone);
  }
  // Sub-day steps: re-align against the containing day each time, so a DST
  // transition produces ONE irregular interval (which is what the wall clock
  // actually did) instead of permanently phase-shifting every later tick.
  const next = alignDown(ms + step.ms, step, zone);
  return next > ms ? next : ms + step.ms;
}

/** The largest aligned instant ≤ `ms` for `step`, in `zone`. */
function alignDown(ms: number, step: TimeStep, zone: TimeZoneMode): number {
  const p = partsOf(ms, zone);
  switch (step.unit) {
    case "year": {
      const y = Math.floor(p.year / step.count) * step.count;
      return fromParts({ year: y, month: 1, day: 1, hour: 0, minute: 0, second: 0, ms: 0 }, zone);
    }
    case "month": {
      const m = Math.floor((p.month - 1) / step.count) * step.count;
      return fromParts({ year: p.year, month: m + 1, day: 1, hour: 0, minute: 0, second: 0, ms: 0 }, zone);
    }
    case "day": {
      // Day steps anchor on the 1st of the month, so a 7-day axis reads
      // 01, 08, 15, 22 rather than drifting with the epoch.
      const d = Math.floor((p.day - 1) / step.count) * step.count;
      return fromParts({ ...p, day: d + 1, hour: 0, minute: 0, second: 0, ms: 0 }, zone);
    }
    default: {
      // Sub-day: anchor on the containing local/UTC midnight. Every sub-day rung
      // divides 24 h, so this lands on whole seconds/minutes/hours of the clock
      // even in a zone whose UTC offset is :30 or :45.
      const day = startOfDay(ms, zone);
      const into = ms - day;
      return day + Math.floor(into / step.ms) * step.ms;
    }
  }
}

/** Options for `timeTicks`. */
export interface TimeTicksOptions {
  /** Which clock to align and label in. Default 'local'. */
  zone?: TimeZoneMode;
  /** Override the step (otherwise chosen from the span + target count). */
  step?: TimeStep;
  /** Hard cap on emitted ticks, to bound a pathological domain. Default 512. */
  maxTicks?: number;
}

/** A tick set plus the step and label style it was built with. */
export interface TimeTickSet {
  ticks: TimeTick[];
  step: TimeStep;
  style: TimeLabelStyle;
  zone: TimeZoneMode;
}

/**
 * Ticks across a TIME domain (epoch ms), at aligned instants.
 *
 * Ticks are placed on step boundaries, NOT evenly across [min,max] — which is
 * the whole difference between a time axis and an index axis rendered with a
 * clock font. `min` and `max` themselves are never emitted unless they happen to
 * be aligned; a chart whose first tick sits a few seconds inside the frame is
 * correct, and one whose labels read 14:32:07, 14:32:12, 14:32:17 is not.
 *
 * Returns an empty tick list (with the chosen step) for a non-finite or
 * inverted domain, rather than throwing — a chart with no domain states no
 * ticks, exactly as `DomainTracker` states no domain.
 */
export function timeTicks(domain: Range, targetTicks: number, opts: TimeTicksOptions = {}): TimeTickSet {
  const zone = opts.zone ?? "local";
  const maxTicks = opts.maxTicks ?? 512;
  const { min, max } = domain;
  const span = max - min;
  const step = opts.step ?? chooseTimeStep(span, targetTicks);
  const style = timeLabelStyle(domain, step, zone);
  if (!Number.isFinite(min) || !Number.isFinite(max) || max < min) {
    return { ticks: [], step, style, zone };
  }
  const ticks: TimeTick[] = [];
  let t = alignDown(min, step, zone);
  if (t < min) t = advance(t, step, zone);
  let guard = 0;
  while (t <= max && guard++ < maxTicks) {
    ticks.push({ ms: t, label: formatTimeTick(t, style, zone) });
    const next = advance(t, step, zone);
    if (!(next > t)) break; // never loop on a degenerate step
    t = next;
  }
  return { ticks, step, style, zone };
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. The tier-1 predicate
// ─────────────────────────────────────────────────────────────────────────────

const TIMESTAMP_GRAMMARS: RegExp[] = [
  /^(\d{4})$/, // year
  /^(\d{4})-(\d{2})$/, // year-month
  /^(\d{4})-(\d{2})-(\d{2})$/, // date
  /^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{3}))?)?$/, // date-time
  /^(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{3}))?)?$/, // time of day
];

/**
 * D1's tier-1 predicate: does this axis label denote an INSTANT?
 *
 * Strict by design. It accepts only the zero-padded, 24-hour, ISO-shaped
 * grammars `formatTimeTick` emits, and validates the field ranges, so it cannot
 * be satisfied by a number that happens to look date-ish. In particular it
 * REJECTS:
 *
 *   '162', '4'      a record index — the `INDEX 4→160` failure this exists to catch
 *   '0:12'          elapsed m:ss — a DURATION, and not zero-padded
 *   '$408.00'       a price
 *   '2026-13-01'    a well-shaped impossible date
 *
 * It intentionally does NOT use `Date.parse`: `Date.parse` is implementation
 * defined for non-ISO input and accepts things like '162' in some engines, which
 * would make the tier-1 check pass on the exact axis it was written to fail.
 */
export function parsesAsTimestamp(label: string): boolean {
  if (typeof label !== "string") return false;
  const s = label.trim();
  if (s.length === 0) return false;
  for (const re of TIMESTAMP_GRAMMARS) {
    const m = re.exec(s);
    if (!m) continue;
    const n = (i: number) => (m[i] === undefined ? undefined : Number(m[i]));
    if (re === TIMESTAMP_GRAMMARS[0]) return true; // 4-digit year
    if (re === TIMESTAMP_GRAMMARS[1]) return inRange(n(2)!, 1, 12);
    if (re === TIMESTAMP_GRAMMARS[2]) return validDate(n(1)!, n(2)!, n(3)!);
    if (re === TIMESTAMP_GRAMMARS[3]) {
      return (
        validDate(n(1)!, n(2)!, n(3)!) &&
        validClock(n(4)!, n(5)!, n(6), n(7))
      );
    }
    return validClock(n(1)!, n(2)!, n(3), n(4));
  }
  return false;
}

function inRange(v: number, lo: number, hi: number): boolean {
  return Number.isInteger(v) && v >= lo && v <= hi;
}

function validDate(y: number, mo: number, d: number): boolean {
  if (!inRange(mo, 1, 12) || !inRange(d, 1, 31)) return false;
  const dim = [31, (y % 4 === 0 && y % 100 !== 0) || y % 400 === 0 ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  return d <= dim[mo - 1];
}

function validClock(h: number, mi: number, s?: number, msec?: number): boolean {
  if (!inRange(h, 0, 23) || !inRange(mi, 0, 59)) return false;
  if (s !== undefined && !inRange(s, 0, 59)) return false;
  if (msec !== undefined && !inRange(msec, 0, 999)) return false;
  return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. Numeric precision, derived (D12)
// ─────────────────────────────────────────────────────────────────────────────

/** Hard cap on derived decimals: past this, a chart wants a different unit. */
export const MAX_DERIVED_DECIMALS = 8;

/**
 * Decimal places for a tick STEP — the D12 rule.
 *
 * `step` is the spacing between adjacent ticks, so this is the precision at
 * which adjacent tick labels stop being the same string. It is a function of
 * the OBSERVED domain and nothing else: a $185 equity with a $0.50 step gets 1
 * decimal, the same code on a $0.000123 token with a $0.000002 step gets 6, and
 * neither number appears anywhere in this file.
 *
 * `toFixed` is clamped to [0, MAX_DERIVED_DECIMALS].
 */
export function decimalsForStep(step: number): number {
  if (!Number.isFinite(step) || step <= 0) return 2;
  // 1e-9 absorbs the float error in log10 of an exact power of ten
  // (log10(0.001) === -2.9999999999999996 in IEEE754).
  const d = -Math.floor(Math.log10(step) + 1e-9);
  return Math.min(MAX_DERIVED_DECIMALS, Math.max(0, d));
}

/**
 * Decimal places that keep a tick SET distinct — the fallback for tick values
 * that were not produced by a nice step (evenly-divided domains, categorical
 * placements). Returns the smallest d ≤ MAX_DERIVED_DECIMALS for which no two
 * adjacent labels collide, and MAX_DERIVED_DECIMALS if none does.
 */
export function decimalsForTicks(values: readonly number[]): number {
  const finite = values.filter((v) => Number.isFinite(v));
  if (finite.length < 2) return 2;
  for (let d = 0; d <= MAX_DERIVED_DECIMALS; d++) {
    let ok = true;
    for (let i = 1; i < finite.length; i++) {
      if (finite[i].toFixed(d) === finite[i - 1].toFixed(d)) {
        ok = false;
        break;
      }
    }
    if (ok) return d;
  }
  return MAX_DERIVED_DECIMALS;
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. The index → time basis
// ─────────────────────────────────────────────────────────────────────────────

/**
 * How a `TimeBasis` was arrived at \u2014 three epistemic classes, deliberately
 * not two (ENC-1282; timestamps-on-the-wire SPEC D6).
 *
 * 'observed'    \u2014 fitted by least squares from (x, observation-time) pairs
 *                 taken off the real record stream. A measurement OF THIS
 *                 CLIENT: it measures when the records arrived here, not when
 *                 the bars were cut upstream. ENC-1254's path, and the one
 *                 LIMITATIONS.md DC-L16 was written to name.
 * 'transmitted' \u2014 read off the wire, from `createBuffer.timeBasis` (treaty
 *                 `dataplane.v1.DcTimeBasis`). A measurement OF THE PRODUCER:
 *                 `baseMs` is the aligned bucket boundary GMA_V3 stamped where
 *                 the bar was cut and `periodMs` is the bar width forum
 *                 authored, so `t = baseMs + x\u00b7periodMs` is exact rather
 *                 than fitted, and `epochKnown` is the producer's statement
 *                 rather than the client's guess.
 * 'declared'    \u2014 asserted by a caller that claims to know the cadence out
 *                 of band. NOT the wire path, and not a measurement: nothing
 *                 verifies it, which makes it a caption in the D7 sense and it
 *                 is labelled as one. It has no producer anywhere in this
 *                 workspace and must not acquire one \u2014 a basis that came
 *                 off the wire is 'transmitted'.
 *
 * WHY THREE AND NOT TWO. SPEC D6 left one seam here ('declared') and called it
 * a caption. A transmitted basis is the opposite of a caption: it is the only
 * basis on this path that was measured where the bar was defined. Shipping it
 * under the 'declared' label would re-create precisely the confusion DC-L16
 * exists to name \u2014 an axis whose stated provenance reads as an
 * unverifiable assertion when it is in fact the producer's own clock \u2014 and
 * would leave a reader unable to tell the two apart, since one label would then
 * cover both. Rewriting 'declared''s comment instead would have had the same
 * effect for the opposite reason: it would have deleted the name for an
 * unverifiable assertion while a consumer can still hand one in.
 */
export type TimeBasisSource = "observed" | "transmitted" | "declared";

/**
 * A linear map from a record's x lane to an instant: `t = originMs + x·msPerIndex`.
 *
 * ── READ THIS BEFORE TRUSTING A LABEL ─────────────────────
 * There are two ways to arrive at this map and they are not the same evidence.
 *
 *  - A basis the PRODUCER transmitted (`source: 'transmitted'`). Since ENC-1281
 *    the dataplane declares `timeBasis {baseMs, periodMs, epochKnown}` once per
 *    buffer on `createBuffer`, and since ENC-1302 the record's x lane is a BAR
 *    ORDINAL on the producer's grid, not a count of records delivered
 *    (`embassy/internal/pipeline/compound_route.go`; SPEC D7). So
 *    `t = baseMs + x·periodMs` is exact. Use `timeBasisFromWire`.
 *  - A basis this CLIENT fitted (`source: 'observed'`, `IndexTimeTracker`). The
 *    only time it can see is WHEN IT OBSERVED THE RECORD — not when the bar
 *    closed upstream. Those coincide on a live feed and do not on a replayed
 *    capture, which is emitted at the capture's own cadence.
 *
 * The x lane is NOT the buffer index and must not be treated as one. Gaps in it
 * are real — a quiet bar that emitted nothing, a coalesced frame, a silent
 * mid-pipeline drop — and rendering them as gaps is the point (SPEC D7): the
 * alternative, counting deliveries, shifts every later bar one period early,
 * permanently and undetectably. A late joiner's first x is not 0 either.
 *
 * `epochKnown` is how the real/relative distinction survives into the chart:
 *
 *  - true  — `originMs` is a real epoch: the aligned bucket boundary the
 *            producer stamped, or (for a fitted basis) a live client stamping
 *            `Date.now()` on arrival. Labels are wall-clock instants; format in
 *            the local zone.
 *  - false — `originMs` is relative to some tape's zero (a replay stamping the
 *            capture's own `t`, or a producer with no wall-clock bar grid).
 *            Labels are offsets along that tape rendered in clock form; format
 *            in UTC, or a local zone offset will shift 0 to 19:00 and invent a
 *            claim the tape never made.
 *
 * A stream with no uniform bar period declares NO BASIS AT ALL — never
 * `periodMs: 0` (SPEC D7 corollary). A consumer with no basis DROPS the axis; it
 * does not fall back to index labels under a heading that says "Time".
 *
 * Publish `source` and `epochKnown` next to the domain, the way ENC-1252
 * publishes `source: derived|literal`. An axis whose provenance is not stated is
 * a caption again.
 */
export interface TimeBasis {
  /** Epoch ms at x = 0 (extrapolated; may precede the first record). */
  originMs: number;
  /** Milliseconds per unit of x — the bar width, for a transmitted basis. */
  msPerIndex: number;
  source: TimeBasisSource;
  /** Whether `originMs` is a real epoch — see the interface docs. */
  epochKnown: boolean;
  /**
   * Samples the fit was taken over. **0 for a basis that was not fitted**
   * ('transmitted' and 'declared'): a transmitted basis is two exact scalars,
   * not a regression, so reporting a sample count for it would imply a
   * confidence it neither has nor needs.
   */
  samples: number;
}

/** Map a record index to an instant. */
export function indexToTime(basis: TimeBasis, index: number): number {
  return basis.originMs + index * basis.msPerIndex;
}

/** Map an instant back to a record index. Returns NaN for a zero cadence. */
export function timeToIndex(basis: TimeBasis, ms: number): number {
  if (basis.msPerIndex === 0) return NaN;
  return (ms - basis.originMs) / basis.msPerIndex;
}

/** Map an index-space domain into a time-space domain through a basis. */
export function timeDomainFor(basis: TimeBasis, indexDomain: Range): Range {
  const a = indexToTime(basis, indexDomain.min);
  const b = indexToTime(basis, indexDomain.max);
  return a <= b ? { min: a, max: b } : { min: b, max: a };
}

// ─────────────────────────────────────────────────────────────────────────────
// 6a. The TRANSMITTED basis — read off the wire, not fitted (ENC-1282, SPEC D6)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * The wire form of a transmitted basis: treaty `dataplane.v1.DcTimeBasis`, as it
 * rides `DcCreateBufferCmd.timeBasis` on the dataplane's scene-init frame.
 *
 * `int64` on the wire; the adapter that produces this object has already
 * narrowed both fields to `number` and refuses anything outside the safe-integer
 * range, so this type is `number` rather than `bigint` on purpose.
 */
export interface WireTimeBasis {
  /** Epoch ms (UTC) of bar ordinal 0. */
  baseMs: number;
  /** Bar width in ms. Always > 0 when the basis is present. */
  periodMs: number;
  /** True when `baseMs` is a real wall clock taken where the bar was cut. */
  epochKnown: boolean;
}

/**
 * Build a `TimeBasis` from a transmitted `DcTimeBasis`, or `null` if the value
 * is not one.
 *
 * This is the whole of ENC-1282's input change: the map is `t = baseMs +
 * x·periodMs` where `x` is the record's bar ordinal, so `originMs = baseMs` and
 * `msPerIndex = periodMs` exactly — no fit, no samples, no drift.
 *
 * IT REFUSES RATHER THAN REPAIRS, and that is deliberate. Every rejection below
 * returns `null`, and a caller with no basis DROPS the time axis (the ENC-1254
 * behaviour this keeps). The alternative — coercing a malformed declaration into
 * some nearby basis — would put a plausible clock on the axis with nothing
 * behind it, which is the caption failure DC-L16 names, now with the producer's
 * authority borrowed to sell it.
 *
 *  - `periodMs` must be a safe integer and **strictly > 0**. A stream with no uniform
 *    bar period declares no basis at all and never declares `periodMs: 0`
 *    (SPEC D7 corollary), so a 0 here is a producer bug, not "no cadence", and
 *    silently accepting it would divide the axis by zero.
 *  - `baseMs` and `periodMs` must be JSON **numbers** and safe integers. The
 *    canonical protobuf-JSON int64-as-string form `{"baseMs":"1789862400000"}`
 *    is rejected, not coerced: coercing would let the emitter drift to a second
 *    representation and keep working, which is the `max_bytes`/`byteLength`/
 *    `maxBytes` divergence this field is trying not to repeat.
 *  - `epochKnown: false` is allowed and means the grid is tape-relative — a
 *    legitimate producer statement, not an error. An ABSENT `epochKnown` reads
 *    as `false`; a present non-boolean is rejected.
 *
 * THE RULES ABOVE ARE NOT INVENTED HERE. They mirror, field for field,
 * `customer-layer/apps/web/src/wire/dataplane.ts` `adaptHostTimeBasis`
 * (ENC-1303), the other consumer of this exact wire field. Two readers of one
 * field that disagree about what is valid is a divergence waiting to be
 * discovered by a chart that renders in one app and drops its axis in the
 * other, so the agreement is deliberate and worth preserving on both sides.
 * That is also why an absent `epochKnown` defaults to `false` rather than being
 * refused: it is proto3's default for a `bool`, and it is the conservative
 * direction — the client never UPGRADES a basis to "real wall clock" without
 * being told, it only ever declines to.
 */
export function timeBasisFromWire(value: unknown): TimeBasis | null {
  if (typeof value !== "object" || value === null) return null;
  const v = value as Record<string, unknown>;
  const baseMs = v.baseMs;
  const periodMs = v.periodMs;
  const epochKnown = v.epochKnown;
  if (typeof baseMs !== "number" || !Number.isSafeInteger(baseMs)) return null;
  if (typeof periodMs !== "number" || !Number.isSafeInteger(periodMs) || periodMs <= 0) {
    return null;
  }
  if (epochKnown !== undefined && typeof epochKnown !== "boolean") return null;
  return {
    originMs: baseMs,
    msPerIndex: periodMs,
    source: "transmitted",
    epochKnown: epochKnown === true,
    samples: 0,
  };
}

/**
 * True when `frame` is a dataplane **scene-init** envelope.
 *
 * Exists because "this is not a scene-init" and "this is a scene-init that
 * declares no basis" are different answers, and `transmittedBasisFromSceneInit`
 * returns `null` for both. A consumer that conflates them retracts a good basis
 * every time any other text frame arrives — and embassy sends two other kinds on
 * the same socket: sticky `setGeometryVertexCount` frames, replayed after the
 * envelope on every subscribe, and `setTransform` frames from the range tracker
 * at roughly 250 ms. The first makes the axis drop deterministically at connect;
 * the second makes it flicker four times a second.
 *
 * Accepts the parsed object or the raw text. A bare command array is NOT a
 * scene-init: it carries no `type`, so a caller that has already unwrapped the
 * envelope has also already made this decision.
 */
export function isSceneInitFrame(frame: unknown): boolean {
  let value: unknown = frame;
  if (typeof value === "string") {
    try {
      value = JSON.parse(value);
    } catch {
      return false;
    }
  }
  if (typeof value !== "object" || value === null || Array.isArray(value)) return false;
  return (value as Record<string, unknown>).type === "scene-init";
}

/**
 * Pull the transmitted basis out of a dataplane scene-init frame.
 *
 * Accepts either the parsed envelope (`{type: 'scene-init', commands: [...]}`),
 * a bare command array, or the raw text frame, and returns the basis declared on
 * the `createBuffer` for `bufferId` — or, when `bufferId` is omitted, the first
 * `createBuffer` that carries one.
 *
 * Returns `null` for every "there is no basis here" case, which includes the
 * ordinary one: a buffer whose stream has no uniform bar period carries no
 * `timeBasis` key at all, and its axis is dropped.
 *
 * Buffers are matched by id and never by position: the envelope's `createBuffer`
 * commands are emitted in sorted-id order, not in the order a view's manifest
 * declares them.
 */
export function transmittedBasisFromSceneInit(
  frame: unknown,
  bufferId?: number,
): TimeBasis | null {
  let value: unknown = frame;
  if (typeof value === "string") {
    try {
      value = JSON.parse(value);
    } catch {
      return null;
    }
  }
  const commands = Array.isArray(value)
    ? value
    : typeof value === "object" && value !== null
      ? (value as Record<string, unknown>).commands
      : undefined;
  if (!Array.isArray(commands)) return null;
  for (const raw of commands) {
    if (typeof raw !== "object" || raw === null) continue;
    const cmd = raw as Record<string, unknown>;
    if (cmd.cmd !== "createBuffer") continue;
    if (bufferId !== undefined && cmd.id !== bufferId) continue;
    const basis = timeBasisFromWire(cmd.timeBasis);
    if (basis) return basis;
    // A matched buffer with no (or a malformed) basis is an answer, not a
    // reason to keep looking at other buffers' clocks.
    if (bufferId !== undefined) return null;
  }
  return null;
}

const OP_APPEND = 1;
const OP_UPDATE_RANGE = 2;
const RECORD_HEADER_SIZE = 13; // [1B op][4B bufferId][4B offset][4B payloadBytes]

/** One buffer whose records carry the index this tracker fits against. */
export interface IndexTimeSource {
  /** Dataplane buffer id. */
  bufferId: number;
  /** Bytes per record. */
  stride: number;
  /** Byte offset of the index field within a record (candle6 / rect4: 0). */
  indexOffset?: number;
}

/**
 * Fits a `TimeBasis` from the real record stream.
 *
 * Feed it every dataplane batch together with the instant at which that batch
 * was observed — `Date.now()` on a live socket, the capture's frame `t` on a
 * replay — and it fits `t = origin + index·msPerIndex` by ordinary least
 * squares, O(1) per record. Least squares rather than a first/last two-point
 * fit because batch delivery jitters: a single late `setTimeout` would tilt a
 * two-point line through the whole axis.
 *
 * It reports `null` until it has two records at DISTINCT indices — with one
 * sample the cadence is unknown, and an axis that invents one is the caption
 * D7 deletes. A caller with no basis must drop the axis, not fall back.
 */
export class IndexTimeTracker {
  private readonly sources = new Map<number, Required<IndexTimeSource>>();
  private n = 0;
  private sx = 0;
  private sy = 0;
  private sxx = 0;
  private sxy = 0;
  private minIdx = Number.POSITIVE_INFINITY;
  private maxIdx = Number.NEGATIVE_INFINITY;

  constructor(
    sources: IndexTimeSource[],
    private readonly epochKnown: boolean,
  ) {
    for (const s of sources) {
      this.sources.set(s.bufferId, {
        bufferId: s.bufferId,
        stride: s.stride,
        indexOffset: s.indexOffset ?? 0,
      });
    }
  }

  /** Records folded so far. */
  get samples(): number {
    return this.n;
  }

  /**
   * Fold one batch observed at `observedAtMs`. Records on unregistered buffers
   * are skipped, as are truncated ones. Returns the count folded.
   */
  observe(batch: ArrayBuffer | ArrayBufferView, observedAtMs: number): number {
    if (!Number.isFinite(observedAtMs)) return 0;
    const view =
      batch instanceof ArrayBuffer
        ? new DataView(batch)
        : new DataView(batch.buffer, batch.byteOffset, batch.byteLength);
    let o = 0;
    let folded = 0;
    while (o + RECORD_HEADER_SIZE <= view.byteLength) {
      const op = view.getUint8(o);
      const bufferId = view.getUint32(o + 1, true);
      const offsetBytes = view.getUint32(o + 5, true);
      const payloadBytes = view.getUint32(o + 9, true);
      o += RECORD_HEADER_SIZE;
      if (o + payloadBytes > view.byteLength) break;
      const src =
        op === OP_APPEND || op === OP_UPDATE_RANGE ? this.sources.get(bufferId) : undefined;
      if (src) {
        const { stride, indexOffset } = src;
        // Same record-phase rule as DomainTracker: a partial leading record in
        // an unaligned updateRange is skipped, never decoded at the wrong offset.
        const phase = (stride - (offsetBytes % stride)) % stride;
        for (let r = o + phase; r + stride <= o + payloadBytes; r += stride) {
          const idx = view.getFloat32(r + indexOffset, true);
          if (!Number.isFinite(idx)) continue;
          this.n++;
          this.sx += idx;
          this.sy += observedAtMs;
          this.sxx += idx * idx;
          this.sxy += idx * observedAtMs;
          if (idx < this.minIdx) this.minIdx = idx;
          if (idx > this.maxIdx) this.maxIdx = idx;
          folded++;
        }
      }
      o += payloadBytes;
    }
    return folded;
  }

  /**
   * The fitted basis, or null when fewer than two distinct indices have been
   * observed (the cadence is not yet a measurement).
   */
  basis(): TimeBasis | null {
    if (this.n < 2 || !(this.maxIdx > this.minIdx)) return null;
    const denom = this.n * this.sxx - this.sx * this.sx;
    if (!Number.isFinite(denom) || denom === 0) return null;
    const msPerIndex = (this.n * this.sxy - this.sx * this.sy) / denom;
    const originMs = (this.sy - msPerIndex * this.sx) / this.n;
    if (!Number.isFinite(msPerIndex) || !Number.isFinite(originMs)) return null;
    return {
      originMs,
      msPerIndex,
      source: "observed",
      epochKnown: this.epochKnown,
      samples: this.n,
    };
  }

  /** Forget everything observed (a replay loop restarting). */
  reset(): void {
    this.n = 0;
    this.sx = 0;
    this.sy = 0;
    this.sxx = 0;
    this.sxy = 0;
    this.minIdx = Number.POSITIVE_INFINITY;
    this.maxIdx = Number.NEGATIVE_INFINITY;
  }
}
