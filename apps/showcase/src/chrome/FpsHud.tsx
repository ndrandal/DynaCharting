/* apps/showcase/src/chrome/FpsHud.tsx — ENC-563
 *
 * A small designed FPS HUD pinned to a corner of the live view. It reads the
 * EngineHost's own frame metrics via the FrameStatsHub (fps every ~250ms, frame
 * ms per rendered frame) — measured INSIDE the engine's existing rAF loop, so it
 * does NOT perturb rendering (no extra render, no competing timer). Mono /
 * tabular-nums per the design tokens. Toggle with the 'F' key.
 *
 * When no hub is wired (or before the first sample) it falls back to a
 * requestAnimationFrame delta timer so the HUD still shows a live number.
 */

import { useEffect, useState } from 'react';
import { FrameStatsHub, type FrameStats } from './frameStats';

/** Subscribe to a hub's frame stats. Falls back to a rAF delta timer if the hub
 *  has produced no fps yet (e.g. engine not started). */
export function useFrameStats(hub: FrameStatsHub | null): FrameStats {
  const [stats, setStats] = useState<FrameStats>(() => hub?.get() ?? { fps: 0, frameMs: 0, frameMsP95: 0 });

  useEffect(() => {
    if (!hub) return;
    setStats(hub.get());
    return hub.subscribe(setStats);
  }, [hub]);

  // Fallback rAF timer: only active while the engine hasn't reported fps yet.
  const hubLive = stats.fps > 0;
  useEffect(() => {
    if (hubLive) return;
    let raf = 0;
    let frames = 0;
    let last = performance.now();
    const tick = (t: number) => {
      frames++;
      const dt = t - last;
      if (dt >= 250) {
        setStats((s) => ({ ...s, fps: (frames * 1000) / dt }));
        frames = 0;
        last = t;
      }
      raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(raf);
  }, [hubLive]);

  return stats;
}

interface FpsHudProps {
  hub: FrameStatsHub | null;
  visible: boolean;
}

export function FpsHud({ hub, visible }: FpsHudProps) {
  const { fps, frameMs } = useFrameStats(visible ? hub : null);
  if (!visible) return null;
  const fpsLabel = fps > 0 ? fps.toFixed(0) : '–';
  // Prefer the engine's measured GPU frame time; if it reports 0 (sub-ms / not
  // surfaced) fall back to the rAF frame budget implied by the live fps.
  const effMs = frameMs > 0 ? frameMs : fps > 0 ? 1000 / fps : 0;
  const msLabel = effMs > 0 ? effMs.toFixed(1) : '–';
  return (
    <div className="chrome-fps mono" role="status" aria-label="Render performance">
      <span className="chrome-fps-value">{fpsLabel}</span>
      <span className="chrome-fps-unit">fps</span>
      <span className="chrome-fps-sep" aria-hidden>·</span>
      <span className="chrome-fps-ms">{msLabel}</span>
      <span className="chrome-fps-unit">ms</span>
    </div>
  );
}
