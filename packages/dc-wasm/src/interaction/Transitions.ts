/* packages/dc-wasm/src/interaction/Transitions.ts — ENC-640 (G1)
 *
 * The TS-host mirror of the C++ data-bound transition stack that the prebuilt
 * dc_engine_host.wasm does not expose (emcc absent — same reason SignalStore.ts /
 * EventSurface.ts live TS-side). It faithfully re-implements the three C++ anim
 * primitives that landed in Phase E (ENC-635/636/637):
 *
 *   - FrameClock (ENC-635/E1)          → `TransitionController.tick(dtSeconds)`:
 *     the per-frame heartbeat. The host render loop calls it once per frame; it
 *     advances every active row tween and reports whether anything is still
 *     animating so the host can idle when nothing moves (FrameClock::isAnimating).
 *   - AnimationController (ENC-636/E2)  → `syncRows(currentRowIds)`: diff the
 *     DURABLE row ids old→new (object constancy, RowIdentity) and classify each:
 *       ENTER  — newly present → tween progress 0→1 (fade/scale in)
 *       EXIT   — departed       → tween progress 1→0, then drop (fade/scale out)
 *       STABLE — present in both → progress held at 1
 *   - Easing (ENC-635) + InstanceTransition (ENC-637/E3) → `progressOf(rowId)` is
 *     the per-row [0,1] the render side multiplies into a per-instance lane
 *     (opacity / size); see InteractionProof.instanceState().
 *
 * Pure, dependency-free, DOM-free: unit-testable in the node test env. dt is
 * supplied by the caller (a real frame delta in the host, a fixed step in tests),
 * exactly like the C++ FrameClock.
 */

/** Transition phase — mirrors dc::AnimationController::Phase. */
export type TransitionPhase = "enter" | "stable" | "exit";

/** Easing functions — the subset of dc::EasingType the transition layer uses.
 *  Each maps t∈[0,1] → [0,1] (EaseOutBack may overshoot, matching the C++). */
export type EasingName =
  | "linear"
  | "easeInQuad"
  | "easeOutQuad"
  | "easeInOutCubic"
  | "easeOutCubic"
  | "easeOutBack";

/** Evaluate an easing function at t, clamped to [0,1] on the ends (dc::ease). */
export function ease(name: EasingName, t: number): number {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  switch (name) {
    case "linear":
      return t;
    case "easeInQuad":
      return t * t;
    case "easeOutQuad":
      return t * (2 - t);
    case "easeInOutCubic":
      return t < 0.5
        ? 4 * t * t * t
        : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
    case "easeOutCubic": {
      const u = t - 1;
      return u * u * u + 1;
    }
    case "easeOutBack": {
      const c1 = 1.70158;
      const c3 = c1 + 1;
      const u = t - 1;
      return 1 + c3 * u * u * u + c1 * u * u;
    }
    default:
      return t;
  }
}

export interface TransitionOptions {
  /** Seconds for an ENTER tween 0→1. Default 0.3 (matches AnimationController). */
  enterSeconds?: number;
  /** Seconds for an EXIT tween 1→0. Default 0.3. */
  exitSeconds?: number;
  /** Easing for entering rows. Default "easeOutCubic". */
  enterEasing?: EasingName;
  /** Easing for exiting rows. Default "easeInQuad". */
  exitEasing?: EasingName;
}

interface RowTween {
  phase: TransitionPhase;
  /** Current eased progress in [0,1] (may briefly overshoot for easeOutBack). */
  progress: number;
  from: number;
  to: number;
  elapsed: number; // seconds into the tween
  duration: number; // seconds
  easing: EasingName;
}

/**
 * Per-row transition state keyed by durable row id (object constancy). Feed it the
 * current live row-id set each data refresh via `syncRows`, advance it once per
 * frame via `tick(dt)`, and read `progressOf(rowId)` on the render side.
 *
 * Faithful to dc::AnimationController + dc::FrameClock: `< 0` ids are ignored (no
 * row-id threading); an EXIT tween drops the row on completion; the set-sync is
 * idempotent when the row set is unchanged.
 */
export class TransitionController {
  private readonly rows = new Map<number, RowTween>();
  private readonly enterDur: number;
  private readonly exitDur: number;
  private readonly enterEasing: EasingName;
  private readonly exitEasing: EasingName;
  private elapsedSeconds = 0;
  private frameCount = 0;

  constructor(opts: TransitionOptions = {}) {
    this.enterDur = Math.max(0, opts.enterSeconds ?? 0.3);
    this.exitDur = Math.max(0, opts.exitSeconds ?? 0.3);
    this.enterEasing = opts.enterEasing ?? "easeOutCubic";
    this.exitEasing = opts.exitEasing ?? "easeInQuad";
  }

  /**
   * Reconcile the live row-id set (RowIdentity diff): ENTER newly-present rows
   * (0→1), EXIT departed rows (progress→0, dropped on completion), hold survivors
   * STABLE. Re-entering a row that was mid-exit reverses it from its current
   * progress. Idempotent when the set is unchanged.
   */
  syncRows(currentRowIds: readonly number[]): void {
    const current = new Set<number>();
    for (const id of currentRowIds) if (id >= 0) current.add(id);

    // ENTER / re-ENTER.
    for (const id of current) {
      const row = this.rows.get(id);
      if (!row) {
        // Newly present → fade/scale in from 0.
        this.rows.set(id, this.startTween("enter", 0, 1, this.enterDur, this.enterEasing));
      } else if (row.phase === "exit") {
        // Was leaving, now back → reverse from where it is (no popping).
        this.rows.set(
          id,
          this.startTween("enter", row.progress, 1, this.enterDur, this.enterEasing),
        );
      }
      // STABLE rows (phase enter/stable, present) are left to finish/hold.
    }

    // EXIT departed rows.
    for (const [id, row] of this.rows) {
      if (!current.has(id) && row.phase !== "exit") {
        this.rows.set(id, this.startTween("exit", row.progress, 0, this.exitDur, this.exitEasing));
      }
    }
  }

  private startTween(
    phase: TransitionPhase,
    from: number,
    to: number,
    duration: number,
    easing: EasingName,
  ): RowTween {
    // Zero-duration tween resolves immediately to its target.
    const progress = duration <= 0 ? to : from;
    return { phase, progress, from, to, elapsed: 0, duration, easing };
  }

  /**
   * Advance every active tween by `dtSeconds` (the FrameClock heartbeat). Fires no
   * callbacks — the render side polls `progressOf`. Rows that finish an EXIT are
   * dropped. Returns true if anything is still animating (host should render
   * again), false when settled (host may idle). Mirrors FrameClock::tick.
   */
  tick(dtSeconds: number): boolean {
    this.elapsedSeconds += Math.max(0, dtSeconds);
    this.frameCount++;
    const done: number[] = [];
    for (const [id, row] of this.rows) {
      if (row.phase === "stable") continue;
      if (row.duration <= 0) {
        row.progress = row.to;
      } else {
        row.elapsed += Math.max(0, dtSeconds);
        const t = Math.min(1, row.elapsed / row.duration);
        row.progress = row.from + (row.to - row.from) * ease(row.easing, t);
      }
      const finished = row.duration <= 0 || row.elapsed >= row.duration;
      if (finished) {
        if (row.phase === "exit") {
          done.push(id); // fully faded out → drop (object constancy released)
        } else {
          // ENTER complete → settle to STABLE, pin progress at the target.
          row.phase = "stable";
          row.progress = row.to;
        }
      }
    }
    for (const id of done) this.rows.delete(id);
    return this.isAnimating();
  }

  /** Per-row transition progress in [0,1]; 1 for an UNTRACKED row (fully present,
   *  no transition — matches InstanceTransition's "progress treated as 1"). */
  progressOf(rowId: number): number {
    const row = this.rows.get(rowId);
    return row ? row.progress : 1;
  }

  phaseOf(rowId: number): TransitionPhase | null {
    return this.rows.get(rowId)?.phase ?? null;
  }

  isTracked(rowId: number): boolean {
    return this.rows.has(rowId);
  }

  /** Rows tracked, INCLUDING ones still exiting (rendered while they fade out). */
  trackedCount(): number {
    return this.rows.size;
  }

  /** Count of rows with an in-flight (non-stable) tween. */
  activeCount(): number {
    let n = 0;
    for (const row of this.rows.values()) if (row.phase !== "stable") n++;
    return n;
  }

  /** True iff any tween is still in flight (FrameClock::isAnimating). */
  isAnimating(): boolean {
    return this.activeCount() > 0;
  }

  /** Ids of every tracked row (survivors + still-exiting), for the render walk. */
  trackedRows(): number[] {
    return [...this.rows.keys()];
  }

  elapsed(): number {
    return this.elapsedSeconds;
  }
  frame(): number {
    return this.frameCount;
  }
}
