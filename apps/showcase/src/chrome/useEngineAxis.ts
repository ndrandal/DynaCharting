/* apps/showcase/src/chrome/useEngineAxis.ts — ENC-1253 (chart-quality-bar D7)
 *
 * Drives one `EngineAxis` against the showcase's shared `EngineHost`: loads the
 * font once, re-syncs the furniture whenever the measured domain, the ticks,
 * the view or the canvas size changes, and publishes what it drew so the chart
 * can be ASKED what axis it is rendering — the same discipline ENC-1252 applied
 * to the domain (`window.__dcAxisDomain`).
 *
 * WHY THE AXIS IS REBUILT ON EVERY SCENE RE-APPLY — and this one cost a render
 * to find. `resetScene` (sceneController.ts) deletes exactly the ids the
 * previous MANIFEST created, so the axis pane survives a view change untouched.
 * That looked like a feature. It is a bug, because **panes render in SCENE
 * ORDER and a pane paints its clear colour across its whole region**
 * (`DawnSceneRenderer::render` → `clearPane`, ENC-511). Re-applying the
 * manifest makes the view's pane NEWER than the axis pane, and the view's clear
 * quad — `±0.95` on every showcase view — then covers every gridline, tick,
 * spine and label.
 *
 * Measured: the first capture of this work issued all 8 gridlines, 8 tick marks,
 * 2 spines and 10 labels, reported `tier1.pass` from the plan, had ZERO rejected
 * commands, and the canvas contained nothing but candles. Nothing in the error
 * list, the console or the plan said otherwise — the same shape of silence as
 * the scissor trap `plotbox.ts` note (7) warns about, one layer up.
 *
 * So `sceneEpoch` (from `useViewSwitch`, bumped on view change, restart AND
 * every replay loop) rebuilds the furniture: dispose, reset the allocator to the
 * same id base, re-sync. The ids are reused rather than advanced, so a session
 * that loops for an hour does not walk the id space.
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
  type IdAllocator,
  type PlotBox,
  type Tier1LabelVerdict,
} from '@repo/dc-wasm';
import { engineAxisSpec, SHOWCASE_AXIS_THEME, type EngineAxisRefusal } from './engineAxis';
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
  /**
   * Why no axis was drawn, when `plan` is null (ENC-1313). Null while an axis
   * IS drawn.
   *
   * This exists so "declined" and "never asked" are different observable
   * states. Before it, `engineAxisSpec` returned a bare null down four
   * different paths — one of which was a `PlotBoxError` it did not survive to
   * report, because the throw unmounted the app first. A published refusal is
   * the difference between a chart that can be ASKED what it is doing and one
   * whose silence has to be guessed at (the same discipline as
   * `window.__dcAxisDomain`, ENC-1252).
   */
  refusal: EngineAxisRefusal | null;
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
 * @param sceneEpoch bumped whenever the view's manifest is re-applied; forces a
 *                  rebuild so the furniture pane is last in scene order again
 * @param box       the plot box the DATA was fitted into (ENC-1273), so the
 *                  furniture and the geometry share one rectangle; null for a
 *                  view that is not framed, in which case the default box for
 *                  this canvas is used
 */
export function useEngineAxis(
  host: EngineHost | null,
  viewId: string | null,
  axes: ResolvedAxes,
  transform: EffectiveTransform,
  canvas: CanvasSize,
  enabled = true,
  sceneEpoch = 0,
  box: PlotBox | null = null,
): EngineAxisReport {
  const axisRef = useRef<EngineAxis | null>(null);
  const idsRef = useRef<IdAllocator | null>(null);
  const epochRef = useRef<number>(-1);
  const measurerRef = useRef<AxisTextMeasurer | null>(null);
  const [fontLoaded, setFontLoaded] = useState(false);
  /**
   * The last refusal that was warned about, so a retry loop that re-runs this
   * effect many times a second does not print the same line many times a
   * second. Declining is said ONCE per distinct reason, not once per attempt
   * (and always published, whether or not it is printed).
   */
  const warnedRef = useRef<string | null>(null);
  const [report, setReport] = useState<EngineAxisReport>({
    plan: null,
    tier1: null,
    scene: null,
    refusal: null,
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
      setReport({
        plan: null,
        tier1: null,
        scene: null,
        refusal: { reason: 'no-axes-resolved', detail: 'the engine axis is disabled on this host' },
        fontLoaded,
        labelAttempts: 0,
      });
      return;
    }
    if (!idsRef.current) idsRef.current = createIdAllocator(AXIS_ID_BASE);
    // Rebuild ONLY when the scene was re-applied, so the furniture pane is
    // created after the view's and therefore renders after its clear quad.
    // `reset()` hands back the same ids, which is legal because `dispose()`
    // released them. Rebuilding on every domain change instead would re-issue
    // the whole scaffold many times a second for no reason — the ordering is
    // what is stale after a re-apply, not the geometry.
    if (axisRef.current && epochRef.current !== sceneEpoch) {
      axisRef.current.dispose();
      idsRef.current.reset();
      axisRef.current = null;
    }
    epochRef.current = sceneEpoch;
    if (!axisRef.current) axisRef.current = new EngineAxis(host, idsRef.current);
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

    const publish = (next: EngineAxisReport) => {
      setReport(next);
      if (viewId) {
        window.__dcEngineAxis = { ...(window.__dcEngineAxis ?? {}), [viewId]: next };
      }
    };

    const publishPlan = (plan: ReturnType<EngineAxis['sync']>) =>
      publish({
        plan,
        tier1: checkTier1Labels(plan, canvas, { xIsTime: axes.x?.format === 'timestamp' }),
        scene: axisSceneFragment(plan, SHOWCASE_AXIS_THEME, GRID_BACKDROP),
        refusal: null,
        fontLoaded,
        labelAttempts: attempts,
      });

    /**
     * DECLINE VISIBLY (ENC-1313). The refusal is always published — so
     * `window.__dcEngineAxis[viewId].refusal` and the overlay's
     * `data-dc-engine-axis-refusal` can be read off a live page — and
     * `plot-box-refused` is additionally SAID, because that one means "the frame
     * you asked for does not fit the canvas you have", which a person should
     * see. The others are ordinary states: eight views legitimately state no
     * domain, and every view is unsized for one commit.
     */
    const publishRefusal = (refusal: EngineAxisRefusal) => {
      publish({
        plan: null,
        tier1: null,
        scene: null,
        refusal,
        fontLoaded,
        labelAttempts: attempts,
      });
      const key = `${viewId ?? '-'}:${refusal.reason}:${refusal.detail}`;
      if (refusal.reason === 'plot-box-refused' && warnedRef.current !== key) {
        warnedRef.current = key;
        console.warn(
          `[showcase] engine axis declined to draw on ${viewId ?? 'this view'}: ${refusal.detail}. ` +
            'No axis furniture this commit; it is retried when the canvas is laid out.',
        );
      }
    };

    const attempt = (): void => {
      if (cancelled) return;
      attempts++;
      // WAIT for the core rather than syncing half the axis. The marks and the
      // labels have to describe ONE domain: syncing the geometry now and the
      // labels two frames later leaves a stale number beside a fresh gridline,
      // which is exactly the disagreement §1.3 is about. So when the font is
      // loaded but the core is busy, nothing is written at all and the whole
      // sync retries. (A chart with NO font still draws its marks — there are
      // no labels to fall out of step with.)
      if (fontLoaded && !canMeasure()) {
        if (attempts < LABEL_RETRY_LIMIT) {
          timer = setTimeout(attempt, LABEL_RETRY_MS);
        } else {
          console.warn(
            `[showcase] the core stayed busy for ${attempts} attempts; axis not synced`,
          );
        }
        return;
      }
      // The measurer converts clip units to pixels using the canvas it was
      // built with, so it is rebuilt whenever this effect re-runs.
      const measurer = fontLoaded ? createHostMeasurer(host, canvas) : null;
      measurerRef.current = measurer;
      // `engineAxisSpec` NEVER THROWS (ENC-1313). It used to call `plotBox()`
      // unguarded, and on the 1x1 canvas this effect sees for one commit at
      // mount that threw out of the passive effect and unmounted the whole app
      // on 11 of the 22 views. It now declines by name and we publish that.
      const { spec, refusal } = engineAxisSpec(
        axes,
        transform,
        canvas,
        measurer,
        SHOWCASE_AXIS_THEME,
        box,
      );
      if (!spec) {
        publishRefusal(refusal);
        return;
      }
      warnedRef.current = null;
      const plan = axis.sync(spec);
      host.markDirty();
      publishPlan(plan);
      // The probe measures ONE glyph and the sync measures and lays out every
      // label, so the core can go busy in between: `EngineAxis.sync` reports
      // any refusal by pruning the plan, and that shortfall is what is retried.
      const owed =
        fontLoaded &&
        plan.droppedLabels.some((d) => d.reason.includes("refused"));
      if (owed && attempts < LABEL_RETRY_LIMIT) {
        timer = setTimeout(attempt, LABEL_RETRY_MS);
      } else if (owed) {
        console.warn(
          `[showcase] axis labels still refused after ${attempts} attempts — ` +
            `${plan.labels.length} of ${plan.labels.length + plan.droppedLabels.length} drawn`,
        );
      }
    };

    attempt();
    return () => {
      cancelled = true;
      if (timer) clearTimeout(timer);
    };
  }, [host, enabled, viewId, sceneEpoch, axes, transform, canvas.width, canvas.height, fontLoaded, box]);

  return report;
}
