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
 * NOT IN SCOPE (deliberately, so the tickets don't collide): drawing the ticks,
 * gridlines and spine in the engine is ENC-1253; fitting the series to the
 * viewport — i.e. making the baked `transform` a function of this domain — is
 * ENC-1256. This module only produces a number the chart can state.
 */

import type { ObservedDomain } from '@repo/dc-wasm';
import type { AxisSpec } from './types';

/** An axis whose domain is settled: `min`/`max` are present and finite. */
export interface ResolvedAxisSpec extends AxisSpec {
  min: number;
  max: number;
}

/** How one axis got its domain. */
export type DomainSourceKind = 'derived' | 'literal';

/** One axis's resolved domain plus its provenance, for reporting. */
export interface AxisDomainFact {
  min: number;
  max: number;
  source: DomainSourceKind;
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
): { axis: ResolvedAxisSpec; fact: AxisDomainFact } | null {
  if (!spec) return null;
  // A measurement outranks a literal: a view that declares an axisDomain AND
  // leaves a stale min/max behind should show the measurement, not the caption.
  if (measured && Number.isFinite(measured.min) && Number.isFinite(measured.max)) {
    return {
      axis: { ...spec, min: measured.min, max: measured.max },
      fact: { min: measured.min, max: measured.max, source: 'derived' },
    };
  }
  if (isFinitePair(spec)) {
    return {
      axis: { ...spec, min: spec.min, max: spec.max },
      fact: { min: spec.min, max: spec.max, source: 'literal' },
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
 */
export function resolveAxes(
  axes: { x?: AxisSpec; y?: AxisSpec } | undefined,
  observed: ObservedDomain | null,
): { axes: ResolvedAxes; report: AxisDomainReport } {
  const x = resolveAxis(axes?.x, observed?.x);
  const y = resolveAxis(axes?.y, observed?.y);
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
    f === null ? null : { min: +f.min.toFixed(6), max: +f.max.toFixed(6), source: f.source };
  return JSON.stringify({ x: round(report.x), y: round(report.y), records: report.records });
}
