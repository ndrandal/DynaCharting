/* packages/dc-wasm/src/interaction/SignalStore.ts — ENC-634 (D4)
 *
 * The TS-host mirror of the C++ `dc::SignalStore`
 * (core/include/dc/interaction/SignalStore.hpp, ENC-623/B1). It is the *sink* the
 * browser event surface (EventSurface.ts) writes into: the mutable runtime state
 * of the interaction layer — selections (point / interval / multi), hover, brush
 * rect, camera, and a transition clock — each keyed by a string signal name.
 *
 * WHY A TS STORE (and not the C++ one): the C++ SignalStore / HoverManager /
 * SelectionState / BrushGesture live in the `dc` core, but the prebuilt
 * dc_engine_host.wasm exposes only applyControl + pick (see ../wasm.ts) — no
 * Embind bindings for the interaction primitives — and emcc is unavailable here to
 * rebuild the module. The SPEC (open-Q#4, component §6, phase-D item 4) places the
 * event→signal wiring and the cross-view store in the TS host/session layer, so a
 * faithful JS-side SignalStore is the intended home, not a shim. When the wasm
 * later gains per-instance pick (Phase C, PickResult.rowId), the SAME store types
 * carry it — selection/hover already reserve `rowId`.
 *
 * DOWNSTREAM (this is the public event surface for the interaction layer):
 *   - ENC-639 (linked brushing): construct ONE SignalStore and pass it to
 *     `attachInteraction({ store })` on multiple EngineHosts. A brush in view A
 *     mutates the shared signal; view B's subscriber re-renders. `subscribe()` is
 *     the fan-out edge (the TS analogue of ReactiveGraph::markSignalDirty).
 *   - ENC-640 (per-instance pick/hover/brush): read `hover`/`selection` signals per
 *     host; the predicate helpers (`matchesRow`, `matchesValue`, `isEmpty`) mirror
 *     the C++ predicate model so a filter can de-emphasize non-matching instances.
 *
 * Pure, dependency-free, DOM-free: unit-testable in the node test env.
 */

/** kInvalidId analogue: "no row / field constraint". C++ uses kInvalidId; JS uses null. */
export type MaybeId = number | null;

/** A single durable row (RowIdentity) is selected. rowId null == empty. */
export interface PointSelection {
  kind: "point";
  rowId: MaybeId;
}

/** A closed interval [lo,hi] over one named field. field null == empty. */
export interface IntervalSelection {
  kind: "interval";
  field: string | null;
  lo: number;
  hi: number;
}

/** A disjunction (OR): any interval over its field, and/or any explicitly-selected
 *  row (shift/ctrl-click unions). Empty when both are empty. `rows` holds row ids
 *  (or, until per-instance pick lands, drawItemIds — see module note). */
export interface MultiSelection {
  kind: "multi";
  intervals: Array<{ field: string | null; lo: number; hi: number }>;
  rows: number[];
}

/** A 2-D brush rectangle in DATA space. Zero-area == empty. */
export interface BrushRect {
  kind: "brush";
  x0: number;
  y0: number;
  x1: number;
  y1: number;
}

/** The hovered row / draw item, or inactive. `rowId` is populated once the wasm
 *  exposes per-instance pick (Phase C); until then `drawItemId` carries the hit. */
export interface HoverState {
  kind: "hover";
  rowId: MaybeId;
  drawItemId: MaybeId;
  active: boolean;
}

/** Pan/zoom viewport state (never a selection constraint). */
export interface CameraState {
  kind: "camera";
  panX: number;
  panY: number;
  zoom: number;
}

/** A transition clock in [0,1] for the data-bound animation layer (Phase E). */
export interface TransitionClock {
  kind: "clock";
  t: number;
}

export type SignalValue =
  | PointSelection
  | IntervalSelection
  | MultiSelection
  | BrushRect
  | HoverState
  | CameraState
  | TransitionClock;

/** Notified after a signal is defined / set / cleared. `value` is the new value
 *  (undefined on `remove`). The analogue of ReactiveGraph::markSignalDirty. */
export type SignalListener = (signalId: string, value: SignalValue | undefined) => void;

/** Factory helpers for the empty form of each signal kind (mirrors the C++
 *  default-constructed structs), so callers `define(id, emptyBrush())` etc. */
export const emptyPoint = (): PointSelection => ({ kind: "point", rowId: null });
export const emptyInterval = (): IntervalSelection => ({
  kind: "interval",
  field: null,
  lo: 0,
  hi: 0,
});
export const emptyMulti = (): MultiSelection => ({ kind: "multi", intervals: [], rows: [] });
export const emptyBrush = (): BrushRect => ({ kind: "brush", x0: 0, y0: 0, x1: 0, y1: 0 });
export const inactiveHover = (): HoverState => ({
  kind: "hover",
  rowId: null,
  drawItemId: null,
  active: false,
});
export const identityCamera = (): CameraState => ({ kind: "camera", panX: 0, panY: 0, zoom: 1 });
export const zeroClock = (): TransitionClock => ({ kind: "clock", t: 0 });

/** Reset a signal value to its empty/inactive form. Camera & clock are returned
 *  unchanged (they are never selection constraints — matches C++ clear()). */
function clearedValue(v: SignalValue): SignalValue {
  switch (v.kind) {
    case "point":
      return emptyPoint();
    case "interval":
      return emptyInterval();
    case "multi":
      return emptyMulti();
    case "brush":
      return emptyBrush();
    case "hover":
      return inactiveHover();
    case "camera":
    case "clock":
      return v;
  }
}

/** True when a value imposes no constraint (filters nothing): undefined signal,
 *  empty selection, and always camera/clock. Mirrors C++ SignalStore::isEmpty. */
function valueIsEmpty(v: SignalValue): boolean {
  switch (v.kind) {
    case "point":
      return v.rowId === null;
    case "interval":
      return v.field === null;
    case "multi":
      return v.intervals.length === 0 && v.rows.length === 0;
    case "brush":
      return v.x0 === v.x1 && v.y0 === v.y1;
    case "hover":
      return !v.active;
    case "camera":
    case "clock":
      return true;
  }
}

/**
 * Typed container of named mutable signals + per-signal predicates.
 * Faithful to `dc::SignalStore`: define / has / size / get / set / clear / remove
 * + isEmpty / matchesRow / matchesValue. Adds `subscribe` (the ReactiveGraph
 * fan-out edge, TS-side) which downstream linked-brushing (ENC-639) rides.
 */
export class SignalStore {
  private signals = new Map<string, SignalValue>();
  private globalListeners = new Set<SignalListener>();
  private perSignal = new Map<string, Set<SignalListener>>();

  /** Define (or redefine) a signal with an initial value, then notify. */
  define(signalId: string, value: SignalValue): void {
    this.signals.set(signalId, value);
    this.notify(signalId, value);
  }

  has(signalId: string): boolean {
    return this.signals.has(signalId);
  }

  size(): number {
    return this.signals.size;
  }

  /** Current value, or undefined if the signal is not defined. */
  get(signalId: string): SignalValue | undefined {
    return this.signals.get(signalId);
  }

  /** Typed view: undefined if not defined or the wrong kind. */
  getAs<K extends SignalValue["kind"]>(
    signalId: string,
    kind: K,
  ): Extract<SignalValue, { kind: K }> | undefined {
    const v = this.signals.get(signalId);
    return v && v.kind === kind ? (v as Extract<SignalValue, { kind: K }>) : undefined;
  }

  /** Set a new value and notify. Returns false (no-op) if the signal is undefined. */
  set(signalId: string, value: SignalValue): boolean {
    if (!this.signals.has(signalId)) return false;
    this.signals.set(signalId, value);
    this.notify(signalId, value);
    return true;
  }

  /** define-or-set: set if present, else define. Always notifies. Convenience for
   *  the event surface, which does not care whether the signal pre-existed. */
  put(signalId: string, value: SignalValue): void {
    this.signals.set(signalId, value);
    this.notify(signalId, value);
  }

  /** Reset a selection-type signal to empty and notify. No-op (false) if undefined. */
  clear(signalId: string): boolean {
    const v = this.signals.get(signalId);
    if (!v) return false;
    const next = clearedValue(v);
    this.signals.set(signalId, next);
    this.notify(signalId, next);
    return true;
  }

  /** Erase the signal entirely and notify listeners with `undefined`. */
  remove(signalId: string): void {
    if (!this.signals.delete(signalId)) return;
    this.notify(signalId, undefined);
  }

  // ---- predicates (mirror dc::SignalStore) --------------------------------

  /** No active constraint (filters nothing)? True for undefined / empty / camera / clock. */
  isEmpty(signalId: string): boolean {
    const v = this.signals.get(signalId);
    return v ? valueIsEmpty(v) : true;
  }

  /** Does `rowId` satisfy the row-id part of the selection? Empty/none -> true.
   *  (Value/interval-only constraints are not judged here — see matchesValue.) */
  matchesRow(signalId: string, rowId: number): boolean {
    const v = this.signals.get(signalId);
    if (!v) return true;
    switch (v.kind) {
      case "point":
        return v.rowId === null || v.rowId === rowId;
      case "multi":
        return v.rows.length === 0 || v.rows.includes(rowId);
      case "hover":
        return !v.active || v.rowId === null || v.rowId === rowId;
      default:
        return true;
    }
  }

  /** Does scalar `value` satisfy the value/interval part of the selection?
   *  Empty/none -> true. (Row-id-only constraints are not judged here.) */
  matchesValue(signalId: string, value: number): boolean {
    const v = this.signals.get(signalId);
    if (!v) return true;
    switch (v.kind) {
      case "interval":
        return v.field === null || (value >= v.lo && value <= v.hi);
      case "multi": {
        if (v.intervals.length === 0) return true;
        return v.intervals.some((iv) => iv.field !== null && value >= iv.lo && value <= iv.hi);
      }
      case "brush":
        return true; // 2-D geometry; use bounds directly, not a scalar test.
      default:
        return true;
    }
  }

  // ---- subscription (the TS-side markSignalDirty fan-out) ------------------

  /** Subscribe to ALL signal mutations. Returns an unsubscribe fn. */
  subscribe(listener: SignalListener): () => void {
    this.globalListeners.add(listener);
    return () => this.globalListeners.delete(listener);
  }

  /** Subscribe to mutations of ONE signal. Returns an unsubscribe fn. */
  subscribeTo(signalId: string, listener: SignalListener): () => void {
    let set = this.perSignal.get(signalId);
    if (!set) {
      set = new Set();
      this.perSignal.set(signalId, set);
    }
    set.add(listener);
    return () => {
      set?.delete(listener);
    };
  }

  private notify(signalId: string, value: SignalValue | undefined): void {
    for (const l of this.globalListeners) l(signalId, value);
    const set = this.perSignal.get(signalId);
    if (set) for (const l of set) l(signalId, value);
  }
}
