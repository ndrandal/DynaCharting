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
 * THE TICKS MAP THROUGH THE FITTED TRANSFORM (ENC-1273, SPEC D1 tier 2). The
 * overlay used to REPRODUCE the engine's framing — `effectiveTransform` re-derived
 * the xAnchor's X from the first replayed record and took Y from the view.json
 * literal. `useViewSwitch` now fits the MEASURED domain into the plot box and
 * hands the result down as `framed`; when it is present it is used verbatim,
 * because a reproduction is a second implementation of the framing and the whole
 * failure this fixes is two layers describing two different frames. `framed.box`
 * travels with it, so the furniture is laid out against the very rectangle the
 * data was fitted into rather than against a box recomputed from the canvas.
 * `effectiveTransform` remains the path for the 19 views that declare no
 * `axisDomain`.
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
import type { AxisGridTarget, EngineHost, FramedSeries, ObservedDomain, TimeBasis } from '@repo/dc-wasm';
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
  /**
   * The fitted frame from `useViewSwitch` (ENC-1273) — the plot box the data was
   * fitted into and the transform that did it. When present it REPLACES the
   * reproduced `effectiveTransform`: ticks and geometry then travel through one
   * transform by construction rather than by two derivations agreeing.
   */
  framed?: FramedSeries | null;
  /**
   * Where the GRIDLINES belong (ENC-1316) — the framed view's own pane, and a
   * layer id below every layer that pane holds, so they are drawn behind the
   * data instead of across it. Null for an unframed view.
   */
  gridTarget?: AxisGridTarget | null;
}

/**
 * Query-string switches for the two axis renderers, so the acceptance criterion
 * ("removing the SVG overlay changes nothing about the axis") is one URL rather
 * than a code edit. Both default ON during the transition; `?svgAxis=0` is the
 * configuration a reviewer should look at.
 */
function axisSwitches(): { svg: boolean; engine: boolean } {
  if (typeof window === 'undefined') return { svg: true, engine: true };
  // NOTE THE SPELLING: the switch goes BEFORE the hash —
  // `http://host/?svgAxis=0#/view/candles-aapl`. Routing here is hash-based
  // (`router.ts` splits the hash on '/'), so a query after the '#' would be
  // parsed as part of the view id and land on "View not found".
  const q = new URLSearchParams(window.location.search);
  const off = (k: string) => q.get(k) === '0' || q.get(k) === 'false';
  return { svg: !off('svgAxis'), engine: !off('engineAxis') };
}

/**
 * FAULT INJECTION, and it is here for the same reason `?svgAxis=0` is (ENC-1313).
 *
 * ENC-1313 put two error boundaries around this component. An error boundary
 * that has never been SEEN to catch anything is not a guarantee, it is a
 * hope — the same objection `plotbox.test.ts` raises about a tier-2 check that
 * has never failed. And the failure it exists for is unreproducible on demand:
 * the `PlotBoxError` it was written for is now fixed at source, and the next one
 * has not been written yet.
 *
 * So the boundary is drillable from a URL rather than from a code edit:
 *
 *   ?chromeFault=render   throw during ChromeOverlay's render
 *   ?chromeFault=effect   throw from a passive effect — the EXACT shape of the
 *                         ENC-1313 bug, which is the case that used to unmount
 *                         the whole app
 *
 * Both are inert without the flag, both are named so they cannot be mistaken for
 * a real fault, and both are caught by `ChartChromeBoundary`: the engine canvas,
 * the app bar and the router keep running and a badge says what declined.
 */
function chromeFault(): 'render' | 'effect' | null {
  if (typeof window === 'undefined') return null;
  const v = new URLSearchParams(window.location.search).get('chromeFault');
  if (v === 'render' || v === '1') return 'render';
  if (v === 'effect') return 'effect';
  return null;
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
  framed = null,
  gridTarget = null,
}: ChromeOverlayProps) {
  const switches = useMemo(axisSwitches, []);
  const fault = useMemo(chromeFault, []);

  // The drill for `ChartChromeBoundary` (see `chromeFault`). A passive-effect
  // throw is the shape that used to unmount `<App>`; before ENC-1313 this URL
  // would have emptied `#root`.
  useEffect(() => {
    if (fault === 'effect') {
      throw new Error('chromeFault=effect — deliberate passive-effect throw (ENC-1313 drill)');
    }
  }, [fault]);
  if (fault === 'render') {
    throw new Error('chromeFault=render — deliberate render throw (ENC-1313 drill)');
  }

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

  // The transform the ticks are placed with. THE fitted one when the view is
  // framed (ENC-1273) — not a reproduction of it — falling back to reproducing
  // the engine's runtime framing (the xAnchor X re-derivation from the first
  // replayed record) for a view that is not.
  const fittedTransform = framed?.transform ?? null;
  const transform = useMemo(() => {
    if (fittedTransform) return fittedTransform;
    const fx = view.xAnchor ? firstRecordX(view.records, view.growth) : null;
    return effectiveTransform(view.meta.transform, view.xAnchor, fx);
  }, [view, fittedTransform]);

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
  const engineAxis = useEngineAxis(
    switches.engine ? host : null,
    view.id,
    resolvedAxes,
    transform,
    canvasSize,
    switches.engine,
    sceneEpoch,
    framed?.box ?? null,
    gridTarget,
  );

  // ENC-1313: when the engine axis declines, SAY SO IN THE DOM. `plan: null`
  // used to be indistinguishable from "this view wants no axis", and the one
  // refusal that mattered — a 1px canvas — never got as far as being reported,
  // because the `PlotBoxError` it should have been unmounted the app instead.
  // Absent on a view that drew its axis, so a harness reads presence, not value.
  const axisRefusal = engineAxis.refusal
    ? `${engineAxis.refusal.reason}: ${engineAxis.refusal.detail}`
    : undefined;

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
      data-dc-engine-axis-refusal={axisRefusal}
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
