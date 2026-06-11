/* apps/showcase/src/chrome/frameStats.ts
 *
 * A tiny pub-sub hub that bridges the EngineHost's HUD sink (setFps/setStats —
 * EngineHostHudSink) to React. ShowcaseEngine constructs the EngineHost with a
 * `makeHudSink(hub)` sink; the engine pushes fps (every ~250ms) and per-frame
 * stats (frameMs / frameMsP95, every rendered frame) into the hub from inside
 * its own rAF loop — so reading them perturbs NOTHING (no extra timers, no
 * render). The FPS HUD subscribes via useFrameStats().
 */

import type { EngineHostHudSink, EngineStats } from '@repo/dc-wasm';

export interface FrameStats {
  fps: number;
  frameMs: number;
  frameMsP95: number;
}

type Listener = (s: FrameStats) => void;

export class FrameStatsHub {
  private stats: FrameStats = { fps: 0, frameMs: 0, frameMsP95: 0 };
  private listeners = new Set<Listener>();

  get(): FrameStats {
    return this.stats;
  }

  setFps(fps: number): void {
    this.stats = { ...this.stats, fps };
    this.emit();
  }

  setStats(s: EngineStats): void {
    this.stats = { ...this.stats, frameMs: s.frameMs, frameMsP95: s.frameMsP95 };
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
