/* apps/showcase/src/engine/ShowcaseEngine.tsx
 *
 * Owns the lifecycle of one DynaCharting EngineHost, bound to a <canvas>. This
 * is the showcase's standalone analogue of customer-layer's EngineProvider, but
 * with NO auth / tenant / forum coupling — it is a pure render shell.
 *
 * The canvas callback ref drives the lifecycle: when React attaches the
 * <canvas>, we `new EngineHost()` + init + start; on unmount we shutdown. A
 * ResizeObserver keeps the canvas backing store matched to its CSS box so the
 * scene fills the viewport. The WASM module loads asynchronously (init() returns
 * synchronously and buffers commands until ready); we expose `ready` once
 * host.whenReady() resolves.
 */

import { useCallback, useEffect, useRef, useState } from 'react';
import { EngineHost } from '@repo/dc-wasm';
import { FrameStatsHub, makeHudSink } from '../chrome/frameStats';

export type EngineStatus = 'init' | 'ready' | 'error';

export interface UseShowcaseEngine {
  /** Pass to <canvas ref={canvasRef} />. Drives engine create/destroy. */
  canvasRef: (el: HTMLCanvasElement | null) => void;
  /** The live host instance once mounted (null before mount / after unmount). */
  host: EngineHost | null;
  /** 'init' until the WASM module finishes loading, then 'ready' or 'error'. */
  status: EngineStatus;
  /** Last error message, if status === 'error'. */
  error: string | null;
  /** Live frame-stats (fps / frameMs) sink for the FPS HUD. Stable identity. */
  statsHub: FrameStatsHub;
}

/**
 * onReady fires exactly once after the host's WASM module is loaded and the
 * canvas is sized — the right moment to apply the first manifest.
 */
export function useShowcaseEngine(onReady?: (host: EngineHost) => void): UseShowcaseEngine {
  const hostRef = useRef<EngineHost | null>(null);
  const canvasElRef = useRef<HTMLCanvasElement | null>(null);
  const resizeObsRef = useRef<ResizeObserver | null>(null);
  const onReadyRef = useRef(onReady);
  onReadyRef.current = onReady;
  // The frame-stats hub bridges the EngineHost HUD sink (fps/frameMs, emitted
  // from inside the engine's own rAF loop) to the FPS HUD — stable identity.
  const statsHubRef = useRef<FrameStatsHub | null>(null);
  if (!statsHubRef.current) statsHubRef.current = new FrameStatsHub();

  const [host, setHost] = useState<EngineHost | null>(null);
  const [status, setStatus] = useState<EngineStatus>('init');
  const [error, setError] = useState<string | null>(null);

  // Resize the canvas backing store to its CSS box. Returns true only when the
  // dimensions actually changed, so callers can avoid spurious re-render marks
  // (the WASM renderer allows just one async GPU op in flight at a time, so a
  // markDirty storm during init can race the first render and abort).
  const sizeCanvas = useCallback((canvas: HTMLCanvasElement): boolean => {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    const w = Math.max(1, Math.round(canvas.clientWidth * dpr));
    const h = Math.max(1, Math.round(canvas.clientHeight * dpr));
    let changed = false;
    if (canvas.width !== w) {
      canvas.width = w;
      changed = true;
    }
    if (canvas.height !== h) {
      canvas.height = h;
      changed = true;
    }
    return changed;
  }, []);

  const canvasRef = useCallback(
    (canvas: HTMLCanvasElement | null) => {
      if (canvas) {
        canvasElRef.current = canvas;
        sizeCanvas(canvas);
        try {
          // Pass a HUD sink so the engine reports fps/frameMs into the hub from
          // inside its existing rAF loop (no extra timers → no render perturbation).
          const h = new EngineHost({ hud: makeHudSink(statsHubRef.current!) });
          h.init(canvas);
          h.start();
          hostRef.current = h;
          setHost(h);
          setStatus('init');

          // Keep the backing store sized to the CSS box.
          const obs = new ResizeObserver(() => {
            if (canvasElRef.current) {
              // Only re-render when the size genuinely changed — the initial
              // observe callback fires at the current size and must not enqueue
              // a redundant render.
              if (sizeCanvas(canvasElRef.current)) hostRef.current?.markDirty();
            }
          });
          obs.observe(canvas);
          resizeObsRef.current = obs;

          // Await the async WASM load, then signal readiness.
          h.whenReady()
            .then(() => {
              if (hostRef.current !== h) return; // unmounted meanwhile
              setStatus('ready');
              onReadyRef.current?.(h);
            })
            .catch((e: unknown) => {
              setStatus('error');
              setError((e as Error)?.message ?? String(e));
            });
        } catch (e) {
          console.warn('[showcase] engine init failed:', (e as Error).message);
          hostRef.current = null;
          setHost(null);
          setStatus('error');
          setError((e as Error).message);
        }
      } else {
        resizeObsRef.current?.disconnect();
        resizeObsRef.current = null;
        hostRef.current?.shutdown();
        hostRef.current = null;
        canvasElRef.current = null;
        setHost(null);
        setStatus('init');
      }
    },
    [sizeCanvas],
  );

  // Defensive teardown if the component unmounts without the ref nulling.
  useEffect(() => {
    return () => {
      resizeObsRef.current?.disconnect();
      hostRef.current?.shutdown();
      hostRef.current = null;
    };
  }, []);

  return { canvasRef, host, status, error, statsHub: statsHubRef.current };
}
