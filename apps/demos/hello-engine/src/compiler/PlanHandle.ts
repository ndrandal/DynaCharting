// apps/demos/hello-engine/src/compiler/PlanHandle.ts
import type { WorkerSubscription } from "../protocol";

export type ApplyResult = { ok: true } | { ok: false; error: string };

export class PlanHandle {
  readonly commands: any[] = [];
  readonly disposeCommands: any[] = [];
  readonly subscriptions: WorkerSubscription[] = [];
  readonly initialBatches: ArrayBuffer[] = [];

  // Track what we created (useful for debugging and future validations)
  readonly created = {
    buffers: new Set<number>(),
    geometries: new Set<number>(),
    drawItems: new Set<number>(),
    transforms: new Set<number>(),
  };

  addCommand(cmd: any) {
    this.commands.push(cmd);
  }

  // Dispose commands are applied in-order, so we always push in the desired order.
  addDispose(cmd: any) {
    this.disposeCommands.push(cmd);
  }

  addSubscription(sub: WorkerSubscription) {
    this.subscriptions.push(sub);
  }

  addInitialBatch(buf: ArrayBuffer) {
    this.initialBatches.push(buf);
  }

  /**
   * Apply all creation commands. If a command fails mid-way, we immediately dispose
   * what we already created (best-effort), so the engine doesn't get wedged.
   */
  mount(apply: (cmd: any) => ApplyResult): ApplyResult {
    for (const c of this.commands) {
      const r = apply(c);
      if (!r.ok) {
        // rollback best-effort
        this.unmount(apply);
        return r;
      }
    }
    return { ok: true };
  }

  /**
   * Best-effort unmount. Never throws.
   * Assumes engine delete operations are safe even if partially created.
   */
  unmount(apply: (cmd: any) => ApplyResult): void {
    for (const c of this.disposeCommands) {
      try {
        const r = apply(c);
        if (!r.ok) {
          // Don't stop; keep going
          // eslint-disable-next-line no-console
          console.warn("PlanHandle unmount warning:", r.error, c);
        }
      } catch (e) {
        // eslint-disable-next-line no-console
        console.warn("PlanHandle unmount exception:", e, c);
      }
    }
  }
}
