/* apps/showcase/src/chrome/deriveAxes.ts — ENC-1252 (chart-quality-bar SPEC D7)
 *
 * "The axis is a measurement or it is not an axis."
 *
 * Every axis this gallery has ever shipped took its bounds from two numbers in
 * a view.json. That is a caption: if the geometry drifted, the number would not
 * move (SPEC §1.3). This module is the seam where that stops. It resolves each
 * axis's domain in one of three ways, in order:
 *
 *   derived — the view declares an `axisDomain` and its records have streamed;
 *             the bounds come from a `DomainTracker` over the real dataplane
 *             bytes. THIS is the D7 path.
 *   literal — the view.json still carries `min`/`max`. Kept so the fifteen
 *             not-yet-converted views keep rendering; every one of them is a
 *             caption and is listed as such by `axisDomainReport`.
 *   none    — neither. The axis is dropped rather than invented. A chart with
 *             no measurement states no domain.
 *
 * PURE + DOM-FREE on purpose: this is the part with a truth value, so it is the
 * part that gets asserted in tests (`deriveAxes.test.ts`, which folds the real
 * committed `records.json` captures). The React seam is ChromeOverlay.
 *
 * TIME (ENC-1254 / D1 tier 1). The same three-way resolution now covers the
 * *units* an axis is labelled in, not just its bounds. An axis declared
 * `format: 'timestamp'` needs a measured `TimeBasis` to carry its record-index
 * domain into instants; without one it is DROPPED, exactly as a domain with no
 * measurement and no literal is dropped. That is the whole reason a view no
 * longer says `INDEX`: the honest alternatives are a real time axis or no x axis
 * — never a caption naming the wrong quantity.
 *
 * NOT IN SCOPE (deliberately, so the tickets don't collide): drawing the ticks,
 * gridlines and spine in the engine is ENC-1253; fitting the series to the
 * viewport — i.e. making the baked `transform` a function of this domain — is
 * ENC-1256. This module only produces a number the chart can state.
 */

import type { ObservedDomain, TimeBasis, TimeBasisSource } from '@repo/dc-wasm';
import type { AxisSpec } from './types';

/** An axis whose domain is settled: `min`/`max` are present and finite. */
export interface ResolvedAxisSpec extends AxisSpec {
  min: number;
  max: number;
  /**
   * The recordIndex → instant map for a `format: 'timestamp'` axis (ENC-1254).
   * `min`/`max` stay in DATA space (record indices) so the overlay keeps mapping
   * them through the engine's transform unchanged; the basis converts to and
   * from instants only for choosing and labelling ticks. Absent on every other
   * format, and a `timestamp` axis is never resolved without one.
   */
  timeBasis?: TimeBasis;
}

/** How one axis got its domain. */
export type DomainSourceKind = 'derived' | 'literal';

/** One axis's resolved domain plus its provenance, for reporting. */
export interface AxisDomainFact {
  min: number;
  max: number;
  source: DomainSourceKind;
  /**
   * Present only on a `format: 'timestamp'` axis (ENC-1254): the fitted
   * milliseconds-per-record and whether its origin is a real epoch. Published so
   * a reader can tell a live wall clock from a replayed tape's own timeline
   * WITHOUT reading the labels — the same reason `source` is published.
   */
  time?: {
    msPerIndex: number;
    originMs: number;
    epochKnown: boolean;
    samples: number;
    /** How the basis was arrived at — see `TimeBasisSource` (ENC-1282). */
    source: TimeBasisSource;
  };
}

/** What the chart can STATE about its own axes. Published for observation. */
export interface AxisDomainReport {
  x: AxisDomainFact | null;
  y: AxisDomainFact | null;
  /** Records folded by the view's DomainTracker (0 when it declares none). */
  records: number;
}

export interface ResolvedAxes {
  x?: ResolvedAxisSpec;
  y?: ResolvedAxisSpec;
}

function isFinitePair(spec: AxisSpec | undefined): spec is AxisSpec & { min: number; max: number } {
  return (
    !!spec &&
    typeof spec.min === 'number' &&
    typeof spec.max === 'number' &&
    Number.isFinite(spec.min) &&
    Number.isFinite(spec.max)
  );
}

/** Resolve one axis against the measured domain. Returns null when neither exists. */
function resolveAxis(
  spec: AxisSpec | undefined,
  measured: { min: number; max: number } | null | undefined,
  basis: TimeBasis | null | undefined,
): { axis: ResolvedAxisSpec; fact: AxisDomainFact } | null {
  if (!spec) return null;
  // A `timestamp` axis without a fitted basis cannot be labelled in the units it
  // claims, so it is dropped. Falling back to the index labels here would
  // reintroduce `INDEX 4→160` under an axis titled "Time", which is worse than
  // no axis: it is a caption that also names the wrong quantity (SPEC §1.3).
  if (spec.format === 'timestamp' && !basis) return null;
  const time =
    spec.format === 'timestamp' && basis
      ? {
          msPerIndex: basis.msPerIndex,
          originMs: basis.originMs,
          epochKnown: basis.epochKnown,
          samples: basis.samples,
          // ENC-1282: the PROVENANCE of the clock, published beside the numbers
          // for the same reason ENC-1252 publishes `derived|literal` beside the
          // domain. `epochKnown` says whether the origin is a real instant; it
          // does not say who measured it. 'observed' is this client timing its
          // own arrivals, 'transmitted' is the producer's declaration taken
          // where the bar was cut, and a reader quoting a time off this chart
          // needs to be able to tell them apart without inferring it from
          // `samples: 0`.
          source: basis.source,
        }
      : undefined;
  const withBasis = (axis: ResolvedAxisSpec): ResolvedAxisSpec =>
    time && basis ? { ...axis, timeBasis: basis } : axis;

  // A measurement outranks a literal: a view that declares an axisDomain AND
  // leaves a stale min/max behind should show the measurement, not the caption.
  if (measured && Number.isFinite(measured.min) && Number.isFinite(measured.max)) {
    return {
      axis: withBasis({ ...spec, min: measured.min, max: measured.max }),
      fact: { min: measured.min, max: measured.max, source: 'derived', ...(time ? { time } : {}) },
    };
  }
  if (isFinitePair(spec)) {
    return {
      axis: withBasis({ ...spec, min: spec.min, max: spec.max }),
      fact: { min: spec.min, max: spec.max, source: 'literal', ...(time ? { time } : {}) },
    };
  }
  return null;
}

/**
 * Resolve a view's `chrome.axes` against the domain measured from its stream.
 *
 * @param axes     the view's declared axes (labels, formats, tick counts, and
 *                 possibly legacy literal bounds)
 * @param observed the live `DomainTracker` report, or null when the view
 *                 declares no `axisDomain` / nothing has streamed yet
 * @param timeBasis the live `IndexTimeTracker` fit, or null until two records at
 *                 distinct indices have landed. Only the X axis takes one — a
 *                 time-indexed Y axis is not a shape this gallery has.
 */
export function resolveAxes(
  axes: { x?: AxisSpec; y?: AxisSpec } | undefined,
  observed: ObservedDomain | null,
  timeBasis: TimeBasis | null = null,
): { axes: ResolvedAxes; report: AxisDomainReport } {
  const x = resolveAxis(axes?.x, observed?.x, timeBasis);
  const y = resolveAxis(axes?.y, observed?.y, null);
  return {
    axes: {
      ...(x ? { x: x.axis } : {}),
      ...(y ? { y: y.axis } : {}),
    },
    report: {
      x: x ? x.fact : null,
      y: y ? y.fact : null,
      records: observed?.records ?? 0,
    },
  };
}

/** Compact, stable JSON for the `data-dc-axis-domain` attribute / harness read. */
export function axisDomainReportJson(report: AxisDomainReport): string {
  const round = (f: AxisDomainFact | null) =>
    f === null
      ? null
      : {
          min: +f.min.toFixed(6),
          max: +f.max.toFixed(6),
          source: f.source,
          ...(f.time
            ? {
                time: {
                  msPerIndex: +f.time.msPerIndex.toFixed(6),
                  originMs: +f.time.originMs.toFixed(6),
                  epochKnown: f.time.epochKnown,
                  samples: f.time.samples,
                  source: f.time.source,
                },
              }
            : {}),
        };
  return JSON.stringify({ x: round(report.x), y: round(report.y), records: report.records });
}
