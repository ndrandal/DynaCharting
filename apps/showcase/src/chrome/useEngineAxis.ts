/* apps/showcase/src/chrome/useEngineAxis.ts — ENC-1253 (chart-quality-bar D7)
 *
 * Drives one `EngineAxis` against the showcase's shared `EngineHost`: loads the
 * font once, re-syncs the furniture whenever the measured domain, the ticks,
 * the view or the canvas size changes, and publishes what it drew so the chart
 * can be ASKED what axis it is rendering — the same discipline ENC-1252 applied
 * to the domain (`window.__dcAxisDomain`).
 *
 * WHY THE AXIS SURVIVES A VIEW SWITCH. `resetScene` (sceneController.ts) deletes
 * exactly the ids the previous MANIFEST created. The axis pane is not one of
 * them, so it is untouched by a view change and is simply re-synced with the new
 * view's ticks. That is deliberate: re-creating the furniture per view would
 * churn ids on a shared allocator for no benefit, and `EngineAxis` is built to
 * be re-synced in place.
 *
 * ID SPACE. The showcase's manifests hand-pick ids in the 10000–10999 band
 * (CONTRACT-buffer-id.md). The axis allocates from 900000 upward so the two
 * cannot collide no matter how many views are added — and so a stray id in a
 * scene dump is immediately attributable.
 */

import { useEffect, useRef, useState } from 'react';
import {
  EngineAxis,
  axisSceneFragment,
  checkTier1Labels,
  createHostMeasurer,
  createIdAllocator,
  type AxisPlan,
  type AxisSceneFragment,
  type AxisTextMeasurer,
  type CanvasSize,
  type EngineHost,
  type Tier1LabelVerdict,
} from '@repo/dc-wasm';
import { engineAxisSpec, SHOWCASE_AXIS_THEME } from './engineAxis';
import type { ResolvedAxes } from './deriveAxes';
import type { EffectiveTransform } from './mapping';

/** Where the axis's ids start. Above every hand-picked manifest id. */
const AXIS_ID_BASE = 900000;

/**
 * How long to wait between attempts to lay the labels out, and how many times.
 *
 * 40ms is a little over two frames at 60Hz — long enough that the engine's own
 * rAF render has finished and the core is idle, short enough that the labels
 * appear within a couple of frames of the marks. 50 attempts is two seconds,
 * after which the honest conclusion is that text is not available on this host
 * and the chart keeps its gridlines, ticks and spine without labels.
 */
const LABEL_RETRY_MS = 40;
const LABEL_RETRY_LIMIT = 50;

/**
 * The font the engine lays the labels out with.
 *
 * `third_party/test_font.ttf` is Fira Sans Regular — the repo's only font, and
 * the one the C++ text tests and the live-viewer already use. It is referenced
 * by URL rather than copied so there is exactly one copy in the repo and no
 * possibility of the browser path and the C++ tests drifting onto different
 * glyph metrics. (Shipping a font properly — a name that is not "test", a
 * licence file, subsetting — is **ENC-1255**.)
 */
const FONT_URL = new URL('../../../../third_party/test_font.ttf', import.meta.url).href;

/**
 * The canvas background the furniture is measured against for the scene dump.
 *
 * `DawnSceneRenderer::render` clears the whole target to opaque black and a pane
 * only paints where it `hasClearColor`, so the gutters — where every tick,
 * spine and label lives — are black. The gridlines cross the view's pane, which
 * is why `axisSceneFragment` takes a background at all: a themed gridline with
 * alpha composites against it.
 */
const GRID_BACKDROP: [number, number, number] = [0.05, 0.05, 0.08];

/** What the engine axis drew, published for observation. */
export interface EngineAxisReport {
  /** The plan the engine was given, or null when no axis was drawn. */
  plan: AxisPlan | null;
  /** D1's tier-1 label row, checked against that plan. */
  tier1: Tier1LabelVerdict | null;
  /** The scene-JSON fragment `harness/score.py` reads. */
  scene: AxisSceneFragment | null;
  /** Whether the font loaded — labels are not drawn without it. */
  fontLoaded: boolean;
  /**
   * How many attempts it took to get a layout out of the core. > 1 means the
   * first tries landed while a render was in flight; a value at the retry limit
   * with `plan.labels` empty means text never became available.
   */
  labelAttempts: number;
}

declare global {
  interface Window {
    /** Per-view report of what the ENGINE drew as an axis (ENC-1253). */
    __dcEngineAxis?: Record<string, EngineAxisReport>;
  }
}

/**
 * Draw `axes` in the engine on `host`, keeping it in sync with the view.
 *
 * @param host      the shared EngineHost (null before it is ready)
 * @param viewId    the active view, so the published report is per view
 * @param axes      the RESOLVED axes (ENC-1252's measured domain)
 * @param transform the view's effective data→clip transform
 * @param canvas    the canvas's backing-store size in device pixels
 * @param enabled   false disables engine drawing entirely (the `?engineAxis=0`
 *                  escape hatch, kept so the two renderers can be compared)
 */
export function useEngineAxis(
  host: EngineHost | null,
  viewId: string | null,
  axes: ResolvedAxes,
  transform: EffectiveTransform,
  canvas: CanvasSize,
  enabled = true,
): EngineAxisReport {
  const axisRef = useRef<EngineAxis | null>(null);
  const measurerRef = useRef<AxisTextMeasurer | null>(null);
  const [fontLoaded, setFontLoaded] = useState(false);
  const [report, setReport] = useState<EngineAxisReport>({
    plan: null,
    tier1: null,
    scene: null,
    fontLoaded: false,
    labelAttempts: 0,
  });

  // Load the font once per host. `loadFont` is buffered until the core is ready,
  // so this is safe to fire as soon as a host exists.
  useEffect(() => {
    if (!host || !enabled) return;
    let cancelled = false;
    void (async () => {
      try {
        const res = await fetch(FONT_URL);
        if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
        const bytes = new Uint8Array(await res.arrayBuffer());
        if (cancelled) return;
        await host.whenReady();
        if (cancelled) return;
        const ok = host.loadFont(bytes);
        setFontLoaded(ok);
        if (!ok) console.warn('[showcase] axis font rejected by the core');
      } catch (e) {
        // No font → geometry only. Said out loud rather than silently label-less:
        // a chart with ticks and no numbers is a specific, diagnosable state.
        console.warn('[showcase] axis font failed to load; drawing marks without labels:', e);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [host, enabled]);

  // Sync the furniture. Runs on every domain/tick/size/view change; `EngineAxis`
  // rewrites its buffers in place rather than allocating, so this is cheap.
  //
  // ── WHY THIS IS NOT JUST `sync(spec)` IN THE EFFECT BODY ──────────────────
  //
  // The core is single-async-op, so `EngineHost` refuses `setTextGeometry` and
  // `measureText` while a render is in flight — they return -1 rather than
  // aborting the WASM runtime. Control commands and data batches are BUFFERED
  // instead and replayed on the next drain, so the gridlines, ticks and spine
  // land whatever the timing. Text does not: it has to return a glyph count
  // synchronously.
  //
  // And the timing is not random. The engine renders from its own rAF loop, and
  // the domain that triggers this effect is published from a rAF callback too
  // (`useViewSwitch.onBatch`), so the effect runs in the same frame batch as a
  // render that has already suspended in ASYNCIFY. Measured on this app: EVERY
  // attempt landed mid-render and the chart drew 8 gridlines and 0 labels, with
  // the font loaded and nothing reporting an error.
  //
  // So the text half is scheduled onto a macrotask and RETRIED against a cheap
  // liveness probe until the core answers. `drawGeometryFirst` is deliberate:
  // the marks appear immediately and the labels follow, rather than the whole
  // axis waiting on the labels.
  useEffect(() => {
    if (!host || !enabled) {
      setReport({ plan: null, tier1: null, scene: null, fontLoaded, labelAttempts: 0 });
      return;
    }
    if (!axisRef.current) {
      axisRef.current = new EngineAxis(host, createIdAllocator(AXIS_ID_BASE));
    }
    const axis = axisRef.current;
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout> | null = null;
    let attempts = 0;

    /**
     * Is the core able to lay text out right now? One glyph through the real
     * `measureText`: it returns -1 when no font is loaded OR when a render is in
     * flight, which are exactly the two conditions that make a label silently
     * not draw. A probe is cheaper than a wrong answer.
     */
    const canMeasure = (): boolean => fontLoaded && host.measureText('0', 0.02, 1).glyphCount > 0;

    const publish = (plan: ReturnType<EngineAxis['sync']>) => {
      const next: EngineAxisReport = {
        plan,
        tier1: checkTier1Labels(plan, canvas, { xIsTime: axes.x?.format === 'timestamp' }),
        scene: axisSceneFragment(plan, SHOWCASE_AXIS_THEME, GRID_BACKDROP),
        fontLoaded,
        labelAttempts: attempts,
      };
      setReport(next);
      if (viewId) {
        window.__dcEngineAxis = { ...(window.__dcEngineAxis ?? {}), [viewId]: next };
      }
    };

    const attempt = (): void => {
      if (cancelled) return;
      attempts++;
      // The measurer converts clip units to pixels using the canvas it was
      // built with, so it is rebuilt whenever this effect re-runs.
      const measurer = canMeasure() ? createHostMeasurer(host, canvas) : null;
      measurerRef.current = measurer;
      const spec = engineAxisSpec(axes, transform, canvas, measurer);
      if (!spec) {
        setReport({ plan: null, tier1: null, scene: null, fontLoaded, labelAttempts: attempts });
        return;
      }
      const plan = axis.sync(spec);
      host.markDirty();
      publish(plan);
      // Retry while labels are OWED, judged from the PLAN rather than from the
      // probe. The probe measures one glyph and the plan measures every label,
      // and the core can go busy in between — so "the measurer was non-null" is
      // not evidence that a single label was measured. What counts is that
      // furniture was drawn and not one label came back, placed or dropped.
      const drewMarks = plan.gridSegments.length > 0 || plan.tickSegments.length > 0;
      const owed =
        fontLoaded && drewMarks && plan.labels.length === 0 && plan.droppedLabels.length === 0;
      if (owed && attempts < LABEL_RETRY_LIMIT) {
        timer = setTimeout(attempt, LABEL_RETRY_MS);
      } else if (owed) {
        console.warn(
          `[showcase] axis labels never laid out after ${attempts} attempts ` +
            '(the core stayed busy, or no font) — marks drawn without labels',
        );
      }
    };

    attempt();
    return () => {
      cancelled = true;
      if (timer) clearTimeout(timer);
    };
  }, [host, enabled, viewId, axes, transform, canvas.width, canvas.height, fontLoaded]);

  return report;
}
