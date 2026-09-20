/* apps/showcase/src/views/useViewSwitch.ts
 *
 * View-switching + replay controller (CONTRACT-view-catalog.md "Switching").
 * Given an EngineHost and the selected ShowcaseView, it:
 *
 *   1. on view change (or first ready): resetScene(host, prevManifest) →
 *      applyManifest(host, view.manifest) → bake view.json.transform into the
 *      view's transform id (since the showcase-explicit path has no
 *      RangeTracker — framing is baked per CONTRACT-view-catalog.md),
 *   2. drives useReplay(host, view.records) to stream the captured frames,
 *   3. loops: on replay-complete, resetScene → applyManifest → restart replay,
 *      so each loop replays the SAME data onto a fresh buffer (no re-append
 *      growth) — ambient motion without drift.
 *
 * One engine instance, applyManifest/resetScene between views (DESIGN-showcase-ui
 * §6 "one engine"). This replaces the slice's hardcoded single-manifest path and
 * is the data source for the T5.6 replay controls.
 *
 * TIME BASIS (ENC-1254, SPEC D1 tier 1). Alongside the domain tracker it owns an
 * `IndexTimeTracker` over the view's PRIMARY growth buffer, fitting
 * `t = origin + recordIndex·msPerIndex` from the capture's own frame timestamps.
 * That fit is what turns a record-index domain into a time domain the axis can
 * label — and it is a measurement, not a constant: the record stream carries no
 * timestamp at all (see the `TimeBasis` docs in @repo/dc-wasm). The basis is
 * built from `view.growth`, which every candle/OHLC view already declares, so no
 * view file gains a new literal. It reports `epochKnown: false` because the
 * showcase replays a captured TAPE: its zero is the start of the capture, not a
 * wall clock.
 *
 * AXIS DOMAIN (ENC-1252, SPEC D7). When the selected view exports an
 * `axisDomain`, this controller also owns a `DomainTracker` over that axis
 * group and folds every replayed batch through it, so the chrome axes state a
 * domain MEASURED from the records the engine just drew. The tracker is rebuilt
 * on view change and deliberately NOT reset on a replay loop: a loop replays the
 * same data onto a fresh buffer, so the measured domain is unchanged, and
 * clearing it would only make the axis flicker once a second.
 *
 * FRAMING (ENC-1273, SPEC D1 tier 2 / §1.0) — THE LITERAL IS GONE. That measured
 * domain is now what the data is FITTED to. For a view `framing.ts` accepts,
 * `frameSeries(domain, canvas)` returns the plot box, the pane region and the
 * data→clip transform, and this controller applies BOTH halves — `setPaneRegion`
 * AND `setTransform` — so the scissor and the projection describe one rectangle
 * (plotbox.ts contract note 7). `bakeTransform`'s `view.meta.transform` literal
 * survives only as the seed for the frames before the first record lands, and
 * `useReplay`'s `xAnchor` (which wrote its own `setTransform` once per replay
 * pass, from a hand-typed 150-index window) is NOT armed for a framed view: it
 * would overwrite the fit every loop. That closes LIMITATIONS.md DC-L14 on this
 * render path — the furniture and the data are now laid out against ONE box, so
 * the leftmost bars no longer run under the price labels.
 */

import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  DEFAULT_PLOT_INSETS,
  DomainTracker,
  IndexTimeTracker,
  composeTransform,
  fitRegionToBox,
  frameSeries,
  paneRegionFor,
  tryPlotBox,
  type AxisGridTarget,
  type CanvasSize,
  type EngineHost,
  type FramedSeries,
  type ObservedDomain,
  type TimeBasis,
  type Transform2D,
} from '@repo/dc-wasm';
import { applyManifest, resetScene } from '../scene/sceneController';
import type { SceneManifest } from '../scene/commands';
import { useReplay } from '../engine/useReplay';
import { effectiveTransform } from '../chrome/mapping';
import { framingFor, type FramingResolution } from './framing';
import type { ShowcaseView } from './registry';

/**
 * The layer the axis GRIDLINES are drawn on, inside the framed view's own pane
 * (ENC-1316, `AxisSpec.gridTarget`).
 *
 * Panes AND layers render in id order (`Scene::paneIds()`/`layerIds()` sort
 * ascending), so this number is the whole mechanism: it has to be below every
 * layer any view's manifest creates, and the lowest one in the catalog is 101.
 * `framing.test.ts` asserts that across every committed manifest rather than
 * leaving it to a comment — a view added with a layer id of 5 would put its
 * marks UNDER the grid, and would look exactly like a theme problem.
 */
export const SHOWCASE_GRID_LAYER_ID = 9;

/**
 * The transform attached to draw items that have none of their own, so the
 * clip→clip re-frame has somewhere to live (ENC-1316).
 *
 * Above every hand-picked manifest id (the 10000-10999 band) and below the
 * engine axis's own allocator base (900000), so a stray id in a scene dump is
 * attributable at a glance. It is deleted and re-created on every apply because
 * `resetScene` only knows about ids the MANIFEST created, so this one outlives
 * the scene it belongs to.
 */
export const SHOWCASE_FIT_TRANSFORM_ID = 880000;

/** True when two domain reports state the same thing (avoids pointless renders). */
function sameDomain(a: ObservedDomain | null, b: ObservedDomain | null): boolean {
  if (a === b) return true;
  if (!a || !b) return false;
  const sameAxis = (p: { min: number; max: number } | null, q: { min: number; max: number } | null) =>
    p === q || (!!p && !!q && p.min === q.min && p.max === q.max);
  return sameAxis(a.x, b.x) && sameAxis(a.y, b.y);
}

/**
 * True when two fitted bases state the same map (avoids pointless renders).
 *
 * The fit MOVES as records land — every new sample refines the least-squares
 * slope — so this compares to a tolerance rather than for equality. 1e-6 ms per
 * record over a 300-record tape is 0.3 µs of drift across the whole axis, i.e.
 * below any label's resolution; anything coarser would re-render on noise.
 */
function sameBasis(a: TimeBasis | null, b: TimeBasis | null): boolean {
  if (a === b) return true;
  if (!a || !b) return false;
  return (
    Math.abs(a.msPerIndex - b.msPerIndex) < 1e-6 &&
    Math.abs(a.originMs - b.originMs) < 1e-6 &&
    a.epochKnown === b.epochKnown
  );
}

/** Bake the view's transform onto its transform-creating manifest command. */
function bakeTransform(host: EngineHost, view: ShowcaseView): void {
  const t = view.meta.transform;
  const transformId = view.growth?.transformId;
  if (!t || transformId === undefined) return;
  host.applyControl({ cmd: 'setTransform', id: transformId, sx: t.sx, sy: t.sy, tx: t.tx, ty: t.ty });
  host.markDirty();
}

/**
 * Write a fitted frame into the engine: the pane's scissor rectangle AND the
 * transform that projects into it, from the SAME `PlotBox` (plotbox.ts note 7).
 *
 * Both, always, in that order. Issuing only the transform leaves the pane on
 * the manifest's hand-written `±0.95` and the two disagree by a few percent of
 * the canvas; issuing only the region leaves the data on whatever was there and
 * the scissor silently crops it. `paneRegionFor` is what makes them one
 * rectangle, and this is the only place either is written for a framed view.
 */
function applyFraming(host: EngineHost, framing: ViewFraming, framed: FramedSeries): void {
  host.applyControl({ cmd: 'setPaneRegion', id: framing.paneId, ...framed.paneRegion });
  if (framed.transform) {
    host.applyControl({ cmd: 'setTransform', id: framing.transformId, ...framed.transform });
  }
  host.markDirty();
}

/**
 * Apply a view's scene (reset prior → apply manifest → frame it).
 *
 * The manifest re-issues its own `setPaneRegion` and re-creates its transform,
 * so a framed view has to be re-framed SYNCHRONOUSLY here rather than in a
 * follow-up effect — otherwise every replay loop renders one frame on the
 * manifest's `±0.95` region and the baked literal before snapping back.
 * `framed` is null until the first record lands, and only then is the view.json
 * literal used, as a seed for a chart that has not measured anything yet.
 */
function applyView(
  host: EngineHost,
  view: ShowcaseView,
  prev: SceneManifest | null,
  framing: ViewFraming | null,
  framed: FramedSeries | null,
): SceneManifest {
  resetScene(host, prev);
  const applied = applyManifest(host, view.manifest);
  if (framing && framed) applyFraming(host, framing, framed);
  else bakeTransform(host, view);
  return applied;
}

export interface UseViewSwitch {
  /**
   * The x/y domain measured from the current view's streamed records (ENC-1252),
   * or null when the view declares no `axisDomain` / nothing has streamed yet.
   */
  axisDomain: ObservedDomain | null;
  /**
   * The recordIndex → instant map fitted from the current view's stream
   * (ENC-1254), or null until two records at distinct indices have landed. A
   * `timestamp` axis with no basis is dropped rather than captioned.
   */
  timeBasis: TimeBasis | null;
  /** [0..1] replay progress of the current view (for the transport scrubber). */
  progress: number;
  /** Whether replay is playing. */
  playing: boolean;
  setPlaying: (p: boolean) => void;
  /** Restart the current view's replay from the first frame. */
  restart: () => void;
  /** Whether the replay loops on completion (ambient motion). */
  loop: boolean;
  setLoop: (l: boolean) => void;
  /**
   * Increments every time the scene is torn down and the view's manifest is
   * re-applied — on a view change, a restart, and EVERY replay loop (ENC-1253).
   *
   * It matters to anything that draws into the same scene alongside the
   * manifest, because panes render in SCENE ORDER and a pane paints its clear
   * colour across its whole region. The axis furniture is created once; after a
   * re-apply the view's pane is newer than it, and the view's clear quad covers
   * it completely — gridlines, ticks, spine and labels all issued, all
   * invisible, with nothing rejected. `useEngineAxis` watches this and rebuilds
   * so the furniture is last again.
   */
  sceneEpoch: number;
  /**
   * The fitted frame this view's data is drawn in (ENC-1273), or null when the
   * view is not framed by the plot box or nothing has streamed yet.
   *
   * The chrome overlay MUST map its ticks through `framed.transform` rather
   * than re-deriving one, or the furniture and the geometry state different
   * frames — which is the disagreement DC-L14 describes, one layer up.
   */
  framed: FramedSeries | null;
  /**
   * Why this view is (or is not) framed by the plot box. Published so the
   * refusal is observable rather than a silent no-op — see `framing.ts`.
   */
  framingResolution: FramingResolution | null;
}

/**
 * Switch to `view` on `host`, apply its scene, and loop-replay its records.
 * `loop` (default true) drives ambient motion. Returns replay transport state.
 */
export function useViewSwitch(
  host: EngineHost | null,
  view: ShowcaseView | null,
  canvas: CanvasSize = { width: 0, height: 0 },
  initialLoop = true,
): UseViewSwitch {
  const appliedRef = useRef<SceneManifest | null>(null);
  const [progress, setProgress] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [loop, setLoop] = useState(initialLoop);
  // Bumping this key re-arms useReplay (a fresh timeline pass) after we reset
  // the scene — used for both the loop and explicit restart.
  const [epoch, setEpoch] = useState(0);

  // --- ENC-1252: the measured axis domain --------------------------------
  const trackerRef = useRef<DomainTracker | null>(null);
  const [axisDomain, setAxisDomain] = useState<ObservedDomain | null>(null);
  const axisDomainRef = useRef<ObservedDomain | null>(null);
  const flushPendingRef = useRef(false);

  // --- ENC-1254: the measured time basis -----------------------------------
  const timeRef = useRef<IndexTimeTracker | null>(null);
  const [timeBasis, setTimeBasis] = useState<TimeBasis | null>(null);
  const timeBasisRef = useRef<TimeBasis | null>(null);

  // A fresh tracker per view: an axis group belongs to one view's buffers.
  useEffect(() => {
    const spec = view?.axisDomain;
    trackerRef.current = spec ? new DomainTracker(spec.sources, spec.policy) : null;
    axisDomainRef.current = null;
    setAxisDomain(null);

    // The time basis is fitted over the view's PRIMARY growth buffer — the one
    // whose records the x axis is indexed by. `epochKnown: false`: a replayed
    // capture's timeline starts at the tape's zero, not at an epoch.
    const g = view?.growth;
    timeRef.current = g
      ? new IndexTimeTracker([{ bufferId: g.bufferId, stride: g.stride, indexOffset: g.xField }], false)
      : null;
    timeBasisRef.current = null;
    setTimeBasis(null);
  }, [view]);

  // Fold every replayed batch. Publishing is coalesced to one animation frame
  // so a burst of frames costs one render, and only when the domain moved.
  const onBatch = useCallback((batch: ArrayBuffer, observedAtMs: number) => {
    const tracker = trackerRef.current;
    const timeTracker = timeRef.current;
    // The SAME bytes, at the SAME instant, folded by both: the domain the chart
    // states and the clock it states it on come from one observation.
    const foldedTime = timeTracker ? timeTracker.observe(batch, observedAtMs) : 0;
    const foldedDomain = tracker ? tracker.observe(batch) : 0;
    if (foldedDomain === 0 && foldedTime === 0) return;
    if (flushPendingRef.current) return;
    flushPendingRef.current = true;
    requestAnimationFrame(() => {
      flushPendingRef.current = false;
      const tt = timeRef.current;
      if (tt) {
        const nextBasis = tt.basis();
        if (!sameBasis(nextBasis, timeBasisRef.current)) {
          timeBasisRef.current = nextBasis;
          setTimeBasis(nextBasis);
        }
      }
      const t = trackerRef.current;
      if (!t) return;
      const next = t.domain();
      if (sameDomain(next, axisDomainRef.current)) return;
      axisDomainRef.current = next;
      setAxisDomain(next);
    });
  }, []);

  // --- ENC-1273: the fitted frame ------------------------------------------
  // Which pane/transform a fitted frame belongs to, or the stated reason there
  // is none. Derived from the view's own manifest — see `framing.ts`.
  const framingResolution = useMemo(() => framingFor(view), [view]);
  const framing = framingResolution?.framed ? framingResolution.framing : null;

  /**
   * The frame itself: the MEASURED domain (ENC-1252) fitted into the plot box
   * for the canvas the engine is actually rendering into.
   *
   * `canvas` is the canvas's BACKING-STORE size in device pixels — the same
   * size `ChromeOverlay` hands the engine axis (ENC-1253), which is what makes
   * `plotBox()` here and `plotBox()` there return the identical rectangle. Feed
   * one of them the CSS box instead and the gutters differ by the device-pixel
   * ratio, which renders as furniture that is close to, but not on, the frame.
   */
  const framed = useMemo<FramedSeries | null>(() => {
    if (!framing || !axisDomain) return null;
    if (!(canvas.width > 0 && canvas.height > 0)) return null;
    try {
      return frameSeries(axisDomain, canvas, framing.insets);
    } catch (e) {
      // `PlotBoxError` — the gutters do not fit the canvas (a 64px price band on
      // a 40px-wide canvas mid-resize). Keep the seeded framing and say so
      // rather than throwing out of a render.
      console.warn('[showcase] plot box refused this canvas; keeping the seeded framing:', e);
      return null;
    }
  }, [framing, axisDomain, canvas.width, canvas.height]);

  // `applyView` runs from a callback (the replay loop) and needs the CURRENT
  // frame at that instant, not the one captured when the callback was made.
  const framedRef = useRef<FramedSeries | null>(null);
  framedRef.current = framed;

  // (Re)apply the scene whenever the selected view changes or the host appears.
  useEffect(() => {
    if (!host || !view) return;
    appliedRef.current = applyView(host, view, appliedRef.current, framing, framedRef.current);
    setProgress(0);
    setPlaying(true);
    setEpoch((e) => e + 1);
  }, [host, view, framing]);

  // Re-fit when the frame moves — the domain grew, or the canvas resized. The
  // manifest is NOT re-applied for this: only the two commands change.
  useEffect(() => {
    if (!host || !framing || !framed) return;
    applyFraming(host, framing, framed);
  }, [host, framing, framed, epoch]);

  // Reset + restart helper (shared by loop completion and the restart button).
  const resetAndReplay = useCallback(() => {
    if (!host || !view) return;
    appliedRef.current = applyView(host, view, appliedRef.current, framing, framedRef.current);
    setProgress(0);
    setEpoch((e) => e + 1);
  }, [host, view, framing]);

  const restart = useCallback(() => {
    setPlaying(true);
    resetAndReplay();
  }, [resetAndReplay]);

  const onComplete = useCallback(() => {
    if (loop) resetAndReplay();
  }, [loop, resetAndReplay]);

  // `epoch` in the records identity forces useReplay to re-arm on reset/restart.
  // We pass the same records object; the effect re-runs because `playing`/the
  // remount via key isn't available here, so we gate via a wrapper records ref.
  const records = view?.records ?? null;
  // Re-arm useReplay on epoch change by toggling a derived "session" — simplest
  // is to key the records object identity. We clone a shallow wrapper per epoch.
  const sessionRecords = useReplaySession(records, epoch);

  useReplay(host, sessionRecords, {
    playing,
    onProgress: setProgress,
    onComplete,
    growth: view?.growth,
    growthSeries: view?.growthSeries,
    // A framed view does NOT get the xAnchor re-derivation: it writes its own
    // `setTransform` on the first record of every pass, from `view.json`'s
    // hand-typed 150-index window, and would overwrite the fit once per loop.
    // Fitting the measured x domain is the same idea, measured (see framing.ts).
    xAnchor: framing ? undefined : view?.xAnchor,
    onBatch,
  });

  return {
    axisDomain,
    timeBasis,
    progress,
    playing,
    setPlaying,
    restart,
    loop,
    setLoop,
    sceneEpoch: epoch,
    framed,
    framingResolution,
  };
}

/**
 * Return a records reference that changes identity whenever `epoch` changes, so
 * useReplay's effect (keyed on the records object) re-arms for a fresh timeline
 * pass after a scene reset. Same underlying frames — only the wrapper identity
 * changes.
 */
function useReplaySession(records: Parameters<typeof useReplay>[1], epoch: number) {
  // Compare against the ORIGINAL records (`src`) — not the returned clone — so a
  // stable records prop across renders does NOT keep minting fresh identities
  // (which would re-arm useReplay every frame). Only an epoch change or a new
  // records object yields a new session.
  const ref = useRef<{ src: typeof records; session: typeof records; epoch: number } | null>(null);
  if (!ref.current || ref.current.epoch !== epoch || ref.current.src !== records) {
    ref.current = { src: records, session: records ? { ...records } : null, epoch };
  }
  return ref.current.session;
}
