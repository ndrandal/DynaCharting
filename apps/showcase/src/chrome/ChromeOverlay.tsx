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
 * THE AXIS IS NOW DRAWN BY THE ENGINE (ENC-1253, SPEC D7 / §1.3). `useEngineAxis`
 * hands the SAME resolved axes and the SAME transform to `EngineAxis`, which
 * emits the gridlines, ticks, spine and labels as `lineAA@1` / `textSDF@1`
 * geometry inside the canvas. The SVG `AxisOverlay` below it is now a DUPLICATE
 * of that, kept on by default only so the two can be compared during the
 * transition — `?svgAxis=0` removes it, and the acceptance criterion for
 * ENC-1253 is that doing so changes nothing about the ticks, gridlines, spine
 * or labels. `?engineAxis=0` turns the engine-drawn one off instead.
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
import type { EngineHost, ObservedDomain, TimeBasis } from '@repo/dc-wasm';
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
import { useEngineAxis } from './useEngineAxis';

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
  /**
   * The live engine host — the axis is drawn INTO it (ENC-1253). Null before
   * the WASM module is ready, in which case no engine furniture is drawn and
   * the SVG overlay is all there is, exactly as before this ticket.
   */
  host?: EngineHost | null;
  /**
   * The canvas's BACKING-STORE size in device pixels. The engine axis is laid
   * out in these units because that is the raster clip space maps onto and the
   * raster `canvas.toDataURL` returns (SPEC D10) — not the CSS box the SVG
   * overlay is sized to.
   */
  canvasSize?: { width: number; height: number };
  /**
   * Bumped every time the view's manifest is re-applied (`useViewSwitch`). The
   * engine axis rebuilds on it so its pane stays LAST in scene order — see the
   * field's own doc comment for why that is load-bearing rather than tidy.
   */
  sceneEpoch?: number;
}

/**
 * Query-string switches for the two axis renderers, so the acceptance criterion
 * ("removing the SVG overlay changes nothing about the axis") is one URL rather
 * than a code edit. Both default ON during the transition; `?svgAxis=0` is the
 * configuration a reviewer should look at.
 */
function axisSwitches(): { svg: boolean; engine: boolean } {
  if (typeof window === 'undefined') return { svg: true, engine: true };
  // Routing here is hash-based (`router.ts`), so `?svgAxis=0` is as likely to
  // arrive AFTER the `#` as before it. Read both rather than silently ignoring
  // the spelling a reader is most likely to type.
  const search = new URLSearchParams(window.location.search);
  const hashQuery = window.location.hash.includes('?')
    ? new URLSearchParams(window.location.hash.slice(window.location.hash.indexOf('?') + 1))
    : new URLSearchParams();
  const off = (k: string) => {
    const v = hashQuery.get(k) ?? search.get(k);
    return v === '0' || v === 'false';
  };
  return { svg: !off('svgAxis'), engine: !off('engineAxis') };
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
  host = null,
  canvasSize = { width: 0, height: 0 },
  sceneEpoch = 0,
}: ChromeOverlayProps) {
  const switches = useMemo(axisSwitches, []);
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

  // ENC-1253: the engine draws the axis. This is the call that makes the
  // canvas-only raster contain gridlines, ticks, a spine and labels.
  useEngineAxis(
    switches.engine ? host : null,
    view.id,
    resolvedAxes,
    transform,
    canvasSize,
    switches.engine,
    sceneEpoch,
  );

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
      {hasAxes && switches.svg && (
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
