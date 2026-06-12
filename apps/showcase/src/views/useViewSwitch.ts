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
 */

import { useCallback, useEffect, useRef, useState } from 'react';
import type { EngineHost } from '@repo/dc-wasm';
import { applyManifest, resetScene } from '../scene/sceneController';
import type { SceneManifest } from '../scene/commands';
import { useReplay } from '../engine/useReplay';
import type { ShowcaseView } from './registry';

/** Bake the view's transform onto its transform-creating manifest command. */
function bakeTransform(host: EngineHost, view: ShowcaseView): void {
  const t = view.meta.transform;
  const transformId = view.growth?.transformId;
  if (!t || transformId === undefined) return;
  host.applyControl({ cmd: 'setTransform', id: transformId, sx: t.sx, sy: t.sy, tx: t.tx, ty: t.ty });
  host.markDirty();
}

/** Apply a view's scene (reset prior → apply manifest → bake transform). */
function applyView(host: EngineHost, view: ShowcaseView, prev: SceneManifest | null): SceneManifest {
  resetScene(host, prev);
  const applied = applyManifest(host, view.manifest);
  bakeTransform(host, view);
  return applied;
}

export interface UseViewSwitch {
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
}

/**
 * Switch to `view` on `host`, apply its scene, and loop-replay its records.
 * `loop` (default true) drives ambient motion. Returns replay transport state.
 */
export function useViewSwitch(host: EngineHost | null, view: ShowcaseView | null, initialLoop = true): UseViewSwitch {
  const appliedRef = useRef<SceneManifest | null>(null);
  const [progress, setProgress] = useState(0);
  const [playing, setPlaying] = useState(true);
  const [loop, setLoop] = useState(initialLoop);
  // Bumping this key re-arms useReplay (a fresh timeline pass) after we reset
  // the scene — used for both the loop and explicit restart.
  const [epoch, setEpoch] = useState(0);

  // (Re)apply the scene whenever the selected view changes or the host appears.
  useEffect(() => {
    if (!host || !view) return;
    appliedRef.current = applyView(host, view, appliedRef.current);
    setProgress(0);
    setPlaying(true);
    setEpoch((e) => e + 1);
  }, [host, view]);

  // Reset + restart helper (shared by loop completion and the restart button).
  const resetAndReplay = useCallback(() => {
    if (!host || !view) return;
    appliedRef.current = applyView(host, view, appliedRef.current);
    setProgress(0);
    setEpoch((e) => e + 1);
  }, [host, view]);

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
    xAnchor: view?.xAnchor,
  });

  return { progress, playing, setPlaying, restart, loop, setLoop };
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
