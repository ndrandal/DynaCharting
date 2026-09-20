/* apps/showcase/src/chrome/frameStats.ts
 *
 * A tiny pub-sub hub that bridges the EngineHost's HUD sink (setFps/setStats —
 * EngineHostHudSink) to React. ShowcaseEngine constructs the EngineHost with a
 * `makeHudSink(hub)` sink; the engine pushes fps (every ~250ms) and per-frame
 * stats (renderCpuMs / renderCpuMsP95 / readbackMs, every RENDERED frame) into
 * the hub from inside its own rAF loop — so reading them perturbs NOTHING (no
 * extra timers, no render). The FPS HUD subscribes via useFrameStats().
 *
 * ENC-1265 — `fps` and the ms figures are now measurements of DIFFERENT things,
 * and that is the point. `fps` is the rate the engine's rAF callback runs on the
 * MAIN THREAD (EngineHost.updateHud ticks whether or not a render happened), so
 * it is a responsiveness number. `renderCpuMs` is CPU wall-clock inside
 * DawnSceneRenderer::render. Before ENC-1265 there was only one measurement
 * here: `frameMs` was never assigned anywhere in core/, so the HUD displayed
 * `1000 / fps` under an `ms` label beside the `fps` it was computed from.
 */

import type { EngineHostHudSink, EngineStats } from '@repo/dc-wasm';

export interface FrameStats {
  /** Main-thread rAF callback rate — a responsiveness number, not a render rate. */
  fps: number;
  /** CPU wall-clock inside the renderer's scene walk + encode + submit. 0 until
   *  the first frame is actually rendered; never substitute arithmetic on `fps`. */
  renderCpuMs: number;
  renderCpuMsP95: number;
  /** CPU wall-clock of the per-frame full-framebuffer readback. */
  readbackMs: number;
}

type Listener = (s: FrameStats) => void;

export class FrameStatsHub {
  private stats: FrameStats = {
    fps: 0,
    renderCpuMs: 0,
    renderCpuMsP95: 0,
    readbackMs: 0,
  };
  private listeners = new Set<Listener>();

  get(): FrameStats {
    return this.stats;
  }

  setFps(fps: number): void {
    this.stats = { ...this.stats, fps };
    this.emit();
  }

  setStats(s: EngineStats): void {
    this.stats = {
      ...this.stats,
      renderCpuMs: s.renderCpuMs,
      renderCpuMsP95: s.renderCpuMsP95,
      readbackMs: s.readbackMs,
    };
    this.emit();
  }

  subscribe(fn: Listener): () => void {
    this.listeners.add(fn);
    return () => this.listeners.delete(fn);
  }

  private emit(): void {
    for (const fn of this.listeners) fn(this.stats);
  }
}

/**
 * Build an EngineHostHudSink that forwards the engine's frame metrics into a
 * FrameStatsHub. setGl/setMem are required by the sink contract but unused here.
 */
export function makeHudSink(hub: FrameStatsHub): EngineHostHudSink {
  return {
    setFps: (fps) => hub.setFps(fps),
    setStats: (s) => hub.setStats(s),
    setGl: () => {},
    setMem: () => {},
  };
}
