/* apps/showcase/src/chrome/FpsHud.tsx — ENC-563, corrected by ENC-1265
 *
 * A small designed FPS HUD pinned to a corner of the live view. It reads the
 * EngineHost's own frame metrics via the FrameStatsHub — measured INSIDE the
 * engine's existing rAF loop, so it does NOT perturb rendering (no extra render,
 * no competing timer). Mono / tabular-nums per the design tokens. Toggle with
 * the 'F' key.
 *
 * ENC-1265 — THE TWO FIGURES ARE TWO MEASUREMENTS, AND THE BADGE SAYS WHICH.
 *
 * It used to show `<fps> fps · <ms> ms`, where the ms half was
 * `frameMs > 0 ? frameMs : 1000 / fps`. `dc::Stats::frameMs` was assigned by
 * nothing anywhere in core/, so it was 0.0 on every frame and the fallback was
 * the ONLY branch ever taken. The badge therefore printed one measurement twice,
 * under two different unit labels: `1 fps · 1000.7 ms` is `1000/0.9993`, and the
 * `.7` is rounding, not information. That badge is burned into 22 committed
 * stills, and PERF-CLAIMS C1/C3 struck all of them.
 *
 * What the two figures mean now, and neither is a frame budget:
 *
 *   fps      how often the engine's rAF callback runs ON THE MAIN THREAD.
 *            EngineHost.tick calls updateHud every tick whether or not a render
 *            happened, so this is a RESPONSIVENESS number: a long synchronous
 *            WASM render, or the framebuffer readback, or a putImageData blit,
 *            or unrelated main-thread JS all depress it identically.
 *   cpu ms   CPU wall-clock inside DawnSceneRenderer::render — scene walk, draw
 *            encoding, render-pass submit. Excludes GPU execution, the
 *            framebuffer readback and the canvas blit.
 *
 * So they do NOT have to agree, and when they disagree that is the diagnosis:
 * `1 fps · 3 ms cpu` says the renderer is not the bottleneck. A derived number
 * could never have said that, which is the entire reason for the change.
 *
 * NO FALLBACK ARITHMETIC. If there is no measurement — the engine has not
 * rendered a frame yet — the ms half shows an em-dash. A blank is a true
 * statement; `1000 / fps` was not.
 *
 * When no hub is wired (or before the first fps sample) the FPS half still falls
 * back to a requestAnimationFrame delta timer, because that measures the same
 * quantity `fps` already means: the rate this page services rAF.
 */

import { useEffect, useState } from 'react';
import { FrameStatsHub, type FrameStats } from './frameStats';

const EMPTY: FrameStats = { fps: 0, renderCpuMs: 0, renderCpuMsP95: 0, readbackMs: 0 };

/** Subscribe to a hub's frame stats. Falls back to a rAF delta timer if the hub
 *  has produced no fps yet (e.g. engine not started) — same quantity, own
 *  measurement. It never synthesises the ms half. */
export function useFrameStats(hub: FrameStatsHub | null): FrameStats {
  const [stats, setStats] = useState<FrameStats>(() => hub?.get() ?? EMPTY);

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

const FPS_TITLE =
  'Main-thread requestAnimationFrame callback rate. A responsiveness number, ' +
  'not a render rate: the engine ticks this whether or not a frame was rendered.';

const CPU_MS_TITLE =
  'CPU wall-clock inside DawnSceneRenderer::render — scene walk, draw encoding ' +
  'and render-pass submit. Excludes GPU execution, framebuffer readback and the ' +
  'canvas blit. Not a frame budget, and not derived from fps.';

interface FpsHudProps {
  hub: FrameStatsHub | null;
  visible: boolean;
}

export function FpsHud({ hub, visible }: FpsHudProps) {
  const { fps, renderCpuMs } = useFrameStats(visible ? hub : null);
  if (!visible) return null;
  const fpsLabel = fps > 0 ? fps.toFixed(0) : '–';
  // ENC-1265: measured or nothing. There is deliberately no `1000 / fps` here.
  const msLabel = renderCpuMs > 0 ? renderCpuMs.toFixed(1) : '–';
  return (
    <div
      className="chrome-fps mono"
      role="status"
      aria-label="Render performance: main-thread rAF rate, and renderer CPU time per frame"
    >
      <span className="chrome-fps-value" title={FPS_TITLE}>
        {fpsLabel}
      </span>
      <span className="chrome-fps-unit" title={FPS_TITLE}>
        fps
      </span>
      <span className="chrome-fps-sep" aria-hidden>
        ·
      </span>
      <span className="chrome-fps-ms" title={CPU_MS_TITLE}>
        {msLabel}
      </span>
      <span className="chrome-fps-unit" title={CPU_MS_TITLE}>
        ms cpu
      </span>
    </div>
  );
}
