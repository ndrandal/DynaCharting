/* apps/showcase/src/chrome/axisTicks.ts — ENC-1254 (chart-quality-bar D1 tier 1)
 *
 * WHAT AN AXIS SAYS, in data space, with no DOM anywhere near it.
 *
 * `AxisOverlay` used to build its ticks inline: `tickValues(min, max, count)`
 * evenly across the domain, then `formatTick` per value. That is correct for a
 * dimensionless quantity and wrong for the two axes this gallery actually
 * ships:
 *
 *   TIME  — evenly dividing a domain gives ticks at 14:32:07, 14:32:12,
 *           14:32:17. Nobody reads a clock like that. Ticks belong on whole
 *           seconds/minutes/hours, which means the STEP comes from a time ladder
 *           and the positions come from calendar alignment, not from the domain
 *           edges (`timeTicks`, @repo/dc-wasm).
 *   VALUE — evenly dividing 408.056…418.024 puts a gridline at 410.049 and then
 *           labels it. Whatever precision you print, the label and the mark
 *           disagree; print fewer digits and it is a lie, print more and it is
 *           noise. `niceTicks` puts the mark at 410 so the label can be exact.
 *
 * Both cases are decided HERE, in a pure function over the resolved axis spec,
 * so D1's tier-1 check can assert on the labels the chart will draw without
 * rendering anything — and so ENC-1253, which moves the drawing into the engine,
 * inherits the labels rather than reimplementing them.
 */

import { niceTicks, timeTicks, timeToIndex, decimalsForStep, type TimeZoneMode } from '@repo/dc-wasm';
import type { ResolvedAxisSpec } from './deriveAxes';
import { formatTick } from './format';

/** One tick, in DATA space (the units the engine's transform consumes). */
export interface AxisTick {
  /** Tick position in data space — a price, or a record index on a time axis. */
  value: number;
  /** The label to draw. */
  label: string;
  /**
   * The epoch-ms instant a `timestamp` tick was labelled from, before
   * `timeToIndex` carried it back into data space. Absent on every other format.
   *
   * It is kept because the label alone cannot always prove it is one: `1234` is
   * both a year and record 1234, and `10:00` is both a clock time and ten
   * minutes elapsed. D1's tier-1 predicate settles those against this number
   * rather than guessing (ENC-1390, `parsesAsTimestamp`). Nothing draws with it.
   */
  instantMs?: number;
}

/** Default tick intervals when an axis declares none. */
export const DEFAULT_TICK_COUNT = 5;

/**
 * The clock a time axis is aligned and labelled in.
 *
 * A basis with a real epoch (a live socket stamping `Date.now()`) is a wall
 * clock and belongs in the viewer's zone. A basis without one (a replayed
 * capture, whose zero is the start of the tape) must be rendered in UTC: pushing
 * a tape offset through a local zone offset would print `19:00:00` for the first
 * record and claim something the tape never said.
 */
export function zoneFor(epochKnown: boolean): TimeZoneMode {
  return epochKnown ? 'local' : 'utc';
}

/**
 * Build the ticks for one resolved axis, in data space.
 *
 * For a `timestamp` axis the domain is in RECORD-INDEX units: it is carried into
 * instants through the measured basis, ticked on the time ladder, and each tick
 * carried back to an index so the caller's existing data→pixel mapping is
 * unchanged. That round trip is why this returns `value` in data space rather
 * than pixels — the overlay and the engine place marks differently, and neither
 * should have to know about time.
 *
 * Returns [] for an axis with a degenerate domain or (for `timestamp`) a basis
 * with a zero cadence: a chart that cannot say when states no ticks.
 */
export function axisTicks(spec: ResolvedAxisSpec): AxisTick[] {
  const count = Math.max(1, Math.floor(spec.ticks ?? DEFAULT_TICK_COUNT));
  if (!Number.isFinite(spec.min) || !Number.isFinite(spec.max)) return [];

  if (spec.format === 'timestamp') {
    const basis = spec.timeBasis;
    if (!basis || !basis.msPerIndex) return [];
    const zone = zoneFor(basis.epochKnown);
    const a = basis.originMs + spec.min * basis.msPerIndex;
    const b = basis.originMs + spec.max * basis.msPerIndex;
    let domain = a <= b ? { min: a, max: b } : { min: b, max: a };
    if (!basis.epochKnown) {
      // A TAPE HAS NO NEGATIVE TIME. The basis extrapolates `originMs` back to
      // record index 0, and a capture whose first record is index 4 therefore
      // puts index 0 a few hundred ms BEFORE the tape started — and the domain
      // floor is a bar EDGE, half a bar earlier again. Rendered against the
      // epoch-0 reference that a tape-relative basis uses, those milliseconds
      // land on 1969-12-31, which drags the whole axis into `date-time` labels
      // ("1970-01-01 00:00", six times) for 30 ms of extrapolation. Clamping the
      // floor is not a fudge: there is no instant on this tape before its start,
      // so the axis states none. Ticks are unmoved — the first aligned tick is
      // at or after zero either way.
      domain = { min: Math.max(0, domain.min), max: Math.max(0, domain.max) };
    }
    const { ticks } = timeTicks(domain, count, { zone });
    return ticks.map((t) => ({ value: timeToIndex(basis, t.ms), label: t.label, instantMs: t.ms }));
  }

  // Everything else: round marks, and a precision derived from the step between
  // them (D12 — never an equity-shaped constant).
  const values = niceTicks(spec.min, spec.max, count);
  if (values.length === 0) return [];
  const step = values.length > 1 ? Math.abs(values[1] - values[0]) : Math.abs(spec.max - spec.min);
  const decimals = decimalsForStep(step);
  return values.map((v) => ({ value: v, label: formatTick(v, spec.format, decimals) }));
}
