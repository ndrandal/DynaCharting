/* apps/showcase/src/chrome/ChromeOverlay.tsx
 *
 * Composites a view's 'logical chart' chrome — axes (gridlines + tick labels),
 * legend, colorbar — plus the FPS HUD, as an absolutely-positioned layer that
 * exactly overlaps the engine canvas box. It measures its own box (which is
 * sized to fill the canvas region) with a ResizeObserver so ticks/gridlines
 * re-flow on resize. It is pointer-events:none so it never intercepts canvas
 * interaction. Driven entirely by the active view's `chrome` metadata + baked
 * transform — adding chrome to a view is data-only (no edit here).
 *
 * TIME (ENC-1254, SPEC D1 tier 1). The x axis of a market view is labelled from
 * the `TimeBasis` measured off the same stream, and the basis travels into the
 * published report so a reader can tell a live wall clock from a replayed tape's
 * own timeline without squinting at the labels.
 *
 * AXIS DOMAIN (ENC-1252, SPEC D7). The axes' bounds are resolved here from the
 * LIVE domain measured off the view's own streamed records (`observedDomain`),
 * falling back to a legacy view.json literal only for views that have not yet
 * declared an `axisDomain`. The resolved domain and its provenance are published
 * on the overlay root as `data-dc-axis-domain` and on `window.__dcAxisDomain`,
 * so the chart can be asked what domain it is stating — by a reader, a test, or
 * the CDP render driver (ENC-1248) — instead of being taken on trust.
 */

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ObservedDomain, TimeBasis } from '@repo/dc-wasm';
import type { ShowcaseView } from '../views/registry';
import { AxisOverlay } from './AxisOverlay';
import { resolveAxes, axisDomainReportJson, type AxisDomainReport } from './deriveAxes';
import { Legend } from './Legend';
import { Colorbar } from './Colorbar';
import { FpsHud } from './FpsHud';
import { ColorbarAxisLabels } from './ColorbarAxisLabels';
import { effectiveTransform } from './mapping';
import { firstRecordX } from './firstRecordX';
import type { FrameStatsHub } from './frameStats';

interface ChromeOverlayProps {
  view: ShowcaseView;
  /** Frame-stats hub for the FPS HUD (null when no engine). */
  statsHub: FrameStatsHub | null;
  /** Whether the FPS HUD is shown (toggled by 'F'). */
  fpsVisible: boolean;
  /**
   * The domain measured from this view's streamed records (ENC-1252). Null when
   * the view declares no `axisDomain`, or before its first record lands — in
   * which case the axes fall back to whatever literal the view.json still
   * carries, and state so in the report.
   */
  observedDomain?: ObservedDomain | null;
  /**
   * The recordIndex → instant map fitted from this view's stream (ENC-1254).
   * Null until two records at distinct indices have landed — and a
   * `format: 'timestamp'` axis is DROPPED while it is null, rather than falling
   * back to the record-index labels it replaced.
   */
  timeBasis?: TimeBasis | null;
}

/** Window surface the domain report is published on (see the module header). */
declare global {
  interface Window {
    __dcAxisDomain?: Record<string, AxisDomainReport>;
  }
}

export function ChromeOverlay({
  view,
  statsHub,
  fpsVisible,
  observedDomain = null,
  timeBasis = null,
}: ChromeOverlayProps) {
  const boxRef = useRef<HTMLDivElement | null>(null);
  const [size, setSize] = useState({ w: 0, h: 0 });

  // Track the overlay box size (== canvas CSS box) for the data→pixel mapping.
  useEffect(() => {
    const el = boxRef.current;
    if (!el) return;
    const measure = () => setSize({ w: el.clientWidth, h: el.clientHeight });
    measure();
    const obs = new ResizeObserver(measure);
    obs.observe(el);
    return () => obs.disconnect();
  }, []);

  const chrome = view.chrome;

  // The effective transform reproduces the engine's runtime framing (including
  // the xAnchor X re-derivation from the first replayed record).
  const transform = useMemo(() => {
    const fx = view.xAnchor ? firstRecordX(view.records, view.growth) : null;
    return effectiveTransform(view.meta.transform, view.xAnchor, fx);
  }, [view]);

  // The axis domain: measured where the view declares an axisDomain, literal
  // otherwise, absent when neither — never invented.
  const { axes: resolvedAxes, report } = useMemo(
    () => resolveAxes(chrome?.axes, observedDomain, timeBasis),
    [chrome?.axes, observedDomain, timeBasis],
  );
  const reportJson = useMemo(() => axisDomainReportJson(report), [report]);

  // Publish it so the chart can be ASKED what domain it is stating.
  useEffect(() => {
    window.__dcAxisDomain = { ...(window.__dcAxisDomain ?? {}), [view.id]: report };
  }, [view.id, report]);

  const hasAxes = !!resolvedAxes.x || !!resolvedAxes.y;
  const hasLegend = !!chrome?.legend?.length;
  const hasColorbar = !!chrome?.colorbar;
  const catX = chrome?.colorbar?.categories?.x;
  const catY = chrome?.colorbar?.categories?.y;

  return (
    <div
      className="chrome-overlay"
      ref={boxRef}
      aria-hidden={false}
      data-dc-view={view.id}
      data-dc-axis-domain={reportJson}
    >
      {hasAxes && (
        <AxisOverlay axes={resolvedAxes} transform={transform} width={size.w} height={size.h} />
      )}
      {/* Categorical symbol labels for heatmaps (correlation matrix etc.). */}
      {hasColorbar && (catX || catY) && (
        <ColorbarAxisLabels x={catX} y={catY} width={size.w} height={size.h} />
      )}
      {hasLegend && <Legend items={chrome!.legend!} />}
      {hasColorbar && <Colorbar spec={chrome!.colorbar!} />}
      <FpsHud hub={statsHub} visible={fpsVisible} />
    </div>
  );
}
