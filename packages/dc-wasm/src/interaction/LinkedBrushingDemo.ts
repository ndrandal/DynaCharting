/* packages/dc-wasm/src/interaction/LinkedBrushingDemo.ts — ENC-639 (F2)
 *
 * The linked-brushing demo: two coordinated views that share ONE SignalStore so a
 * brush in view A live-filters the rows shown in view B. It is the end-to-end proof
 * of cross-view linking over the ENC-634 (D4) interaction surface — the pattern the
 * SPEC (phase-F item 2) calls for and that the SignalStore module note names
 * explicitly ("construct ONE SignalStore and pass it to attachInteraction on
 * multiple EngineHosts").
 *
 * HOW THE LINK WORKS (no per-instance pick required):
 *   - Both views attach an EventSurface over the SAME shared store
 *     (`attachInteraction({ store })`).
 *   - View A brushes in x-mode. EventSurface writes the brush rect AND a paired,
 *     typed interval signal `"<brush>.interval"` carrying `{ field, lo, hi }` in
 *     DATA space (via A's `toDataSpace` inverse).
 *   - The shared store fans that mutation out through `subscribeTo`. View B recomputes
 *     which of its rows fall in the interval using the store's own value predicate
 *     `matchesValue` — i.e. it filters on the brushed FIELD VALUE, not on a row id.
 *     This is deliberate: the prebuilt wasm `pick` returns only a per-draw-item id,
 *     not a per-instance row id (that is a later phase, ENC-640), so a value/interval
 *     predicate is the correct cross-filter primitive today.
 *   - An empty / cleared interval (Escape, or a zero-width brush) imposes no
 *     constraint, so B shows every row again — matching `matchesValue`'s "empty ⇒
 *     matches everything" contract.
 *
 * This module owns no DOM and no wasm: the two views are any objects that expose
 * `attachInteraction(target, options)` (a real `EngineHost` satisfies `LinkableView`),
 * and the targets are any `EventTarget`. That keeps the whole demo unit-testable in
 * the node test env with synthetic events + fake targets, exactly like the ENC-634
 * EventSurface tests — while the SAME code drives two real `EngineHost`s + canvases
 * in the browser (apps layer) unchanged.
 */

import type { EventSurface, EventSurfaceOptions, DataSpaceMapper } from "./EventSurface";
import { SignalStore, emptyInterval } from "./SignalStore";

/** The minimal view surface the demo drives: attach an interaction EventSurface to a
 *  DOM target. `EngineHost.attachInteraction` satisfies this exactly. */
export interface LinkableView {
  attachInteraction(target: EventTarget, options?: EventSurfaceOptions): EventSurface;
}

/** Notified with view B's current filtered rows whenever the brush interval changes. */
export type FilterListener<T> = (rows: readonly T[]) => void;

export interface LinkedBrushingConfig<T> {
  /** The rows rendered by view B — the set the brush filters down. */
  dataB: readonly T[];
  /** Extract the linking scalar (the brushed field's value) from a view-B row. */
  fieldB: (row: T) => number;
  /** Field id tagged onto A's brush interval so the link is explicit. Default "x". */
  brushField?: string;
  /** Pixel→data-space mapper for A's brush, so the interval lands in the same space
   *  as `fieldB` values. Default identity (brush carries pixel coords). */
  toDataSpace?: DataSpaceMapper;
  /** Base brush signal name; the paired interval is "<brushSignal>.interval".
   *  Default "brush". */
  brushSignal?: string;
  /** Shared signal sink. Created if omitted; read it back via `.store`. Passing one
   *  in lets several demos / hosts coordinate on the same store. */
  store?: SignalStore;
}

/**
 * Wires two views to one shared SignalStore and maintains view B's live filtered
 * row set off view A's x-brush. Construct → `link(hostA, targetA, hostB, targetB)`
 * → read `filteredB()` / subscribe via `onFilterChange`.
 */
export class LinkedBrushingDemo<T> {
  /** The single shared store both views mutate/read (the whole point of the demo). */
  readonly store: SignalStore;

  private readonly dataB: readonly T[];
  private readonly fieldB: (row: T) => number;
  private readonly brushField: string;
  private readonly brushSignal: string;
  private readonly intervalSignal: string;
  private readonly toDataSpace?: DataSpaceMapper;

  private surfaceA: EventSurface | null = null;
  private surfaceB: EventSurface | null = null;
  private unsubInterval: (() => void) | null = null;

  private filtered: readonly T[];
  private readonly listeners = new Set<FilterListener<T>>();

  constructor(cfg: LinkedBrushingConfig<T>) {
    this.store = cfg.store ?? new SignalStore();
    this.dataB = cfg.dataB;
    this.fieldB = cfg.fieldB;
    this.brushField = cfg.brushField ?? "x";
    this.brushSignal = cfg.brushSignal ?? "brush";
    this.intervalSignal = `${this.brushSignal}.interval`;
    this.toDataSpace = cfg.toDataSpace;

    // Define the interval signal up front (empty ⇒ no constraint) so B's predicate
    // reads a defined value and subscribers attach before the first brush.
    if (!this.store.has(this.intervalSignal)) {
      this.store.define(this.intervalSignal, emptyInterval());
    }

    // With an empty interval, every row passes.
    this.filtered = [...this.dataB];

    // The fan-out edge: any change to the brushed interval re-filters view B live.
    this.unsubInterval = this.store.subscribeTo(this.intervalSignal, () => this.recompute());
  }

  /**
   * Attach both views to the shared store. View A drives the x-brush; view B only
   * reads (its own hover/selection still work, but the LINK is via the store). Any
   * previous attachment on either host is detached by `attachInteraction` itself.
   * Returns `this` for chaining.
   */
  link(
    hostA: LinkableView,
    targetA: EventTarget,
    hostB: LinkableView,
    targetB: EventTarget,
  ): this {
    this.surfaceA = hostA.attachInteraction(targetA, {
      store: this.store,
      brushSignal: this.brushSignal,
      brushMode: "x",
      brushField: this.brushField,
      liveBrush: true,
      ...(this.toDataSpace ? { toDataSpace: this.toDataSpace } : {}),
    });
    this.surfaceB = hostB.attachInteraction(targetB, {
      store: this.store,
      brushSignal: this.brushSignal,
    });
    // Reflect any interval already present (e.g. a pre-seeded store) into B.
    this.recompute();
    return this;
  }

  /** View B's current filtered rows (those whose `fieldB` falls in A's brush). */
  filteredB(): readonly T[] {
    return this.filtered;
  }

  /** The active brushed interval, or null when there is no constraint. */
  brushInterval(): { field: string | null; lo: number; hi: number } | null {
    const iv = this.store.getAs(this.intervalSignal, "interval");
    if (!iv || iv.field === null) return null;
    return { field: iv.field, lo: iv.lo, hi: iv.hi };
  }

  /** Subscribe to view B's filtered set. Fires on every brush change. Returns an
   *  unsubscribe fn. The current set is delivered immediately. */
  onFilterChange(listener: FilterListener<T>): () => void {
    this.listeners.add(listener);
    listener(this.filtered);
    return () => {
      this.listeners.delete(listener);
    };
  }

  /** The view-A / view-B EventSurfaces once linked (null before `link`). */
  surfaces(): { a: EventSurface | null; b: EventSurface | null } {
    return { a: this.surfaceA, b: this.surfaceB };
  }

  /** Detach both surfaces and stop tracking. The store is left intact. Idempotent. */
  dispose(): void {
    this.surfaceA?.detach();
    this.surfaceB?.detach();
    this.surfaceA = null;
    this.surfaceB = null;
    this.unsubInterval?.();
    this.unsubInterval = null;
    this.listeners.clear();
  }

  private recompute(): void {
    // Filter B on the value predicate the store already implements: empty/cleared
    // interval ⇒ matchesValue returns true for all rows ⇒ B shows everything.
    const next = this.dataB.filter((row) =>
      this.store.matchesValue(this.intervalSignal, this.fieldB(row)),
    );
    // Only churn listeners when the set actually changes membership.
    if (sameRows(next, this.filtered)) return;
    this.filtered = next;
    for (const l of this.listeners) l(next);
  }
}

/** Reference-identity membership equality (order-sensitive, cheap): the filtered set
 *  is always a stable-order subsequence of `dataB`, so length + per-slot identity is
 *  a sound "did the selection change" test. */
function sameRows<T>(a: readonly T[], b: readonly T[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
}
