/* packages/dc-wasm/src/interaction/EventSurface.ts — ENC-634 (D4)
 *
 * The browser ENTRY POINT for the interaction layer: it attaches DOM pointer +
 * keyboard listeners to a canvas/element and translates each event into pick()
 * calls and SignalStore mutations. It is the TS-host realization of SPEC phase-D
 * item 4 ("surface pointer/keyboard events through the WASM EngineHost → signals")
 * and mirrors the behavior of the C++ interaction primitives that the prebuilt
 * wasm does not expose (emcc absent — see SignalStore.ts module note):
 *
 *   - HoverManager  → pointermove -> pick -> `hover` signal (enter/exit callbacks).
 *   - SelectionState → click (pointerdown/up without a drag) -> pick -> `selection`
 *                      signal, `single` (replace) or `toggle` (union) mode, with
 *                      shift/ctrl/meta forcing toggle. Arrow keys navigate; Escape
 *                      clears. (dc::SelectionState single/toggle + selectNext/Prev.)
 *   - BrushGesture  → pointerdown+drag -> begin/update/end -> `brush` signal as an
 *                      x-interval, y-interval, or 2-D rect (dc::BrushGesture.Mode),
 *                      live per-move by default. Escape cancels the drag.
 *
 * It depends on the engine only for ASYNC picking, via a tiny interface, so it is
 * unit-testable with a fake engine and synthetic events (no DOM/jsdom needed).
 */

import type { PickResult } from "../EngineHost";
import {
  SignalStore,
  type BrushRect,
  type HoverState,
  type MultiSelection,
} from "./SignalStore";

/** The minimal engine surface EventSurface needs: fresh async pick. EngineHost
 *  satisfies this (pickAsync). Kept tiny so tests inject a fake. */
export interface PickEngine {
  pickAsync(x: number, y: number): Promise<PickResult>;
}

/** The DOM listen target — the subset of EventTarget the surface uses. A real
 *  HTMLCanvasElement satisfies it; tests pass a minimal fake. */
export interface ListenTarget {
  addEventListener(
    type: string,
    listener: (ev: Event) => void,
    options?: boolean | AddEventListenerOptions,
  ): void;
  removeEventListener(
    type: string,
    listener: (ev: Event) => void,
    options?: boolean | EventListenerOptions,
  ): void;
  /** Optional: used to convert client coords → element-local pixels. */
  getBoundingClientRect?: () => { left: number; top: number; width: number; height: number };
}

/** Pixel → data-space mapper for the brush (BrushGesture works in data space).
 *  Default is identity (brush carries pixel coords); ENC-639/640 inject the
 *  pan/zoom inverse so brushes cross-filter on real field values. */
export type DataSpaceMapper = (px: number, py: number) => { x: number; y: number };

export type SelectionModeName = "single" | "toggle";
/** BrushGesture.Mode: x-interval band, y-interval band, or 2-D rect. */
export type BrushModeName = "x" | "y" | "rect";

export interface EventSurfaceOptions {
  /** Shared signal sink. Omit to create a private one (read via `store`). Pass a
   *  shared instance across hosts for linked brushing (ENC-639). */
  store?: SignalStore;
  /** Signal names (override to namespace per view). Defaults: hover/selection/brush. */
  hoverSignal?: string;
  selectionSignal?: string;
  brushSignal?: string;
  /** Selection replace-vs-union. Default "single"; shift/ctrl/meta always toggles. */
  selectionMode?: SelectionModeName;
  /** Brush geometry. Default "x" (a vertical cross-filter band). */
  brushMode?: BrushModeName;
  /** Field id attached to interval brushes so two views cross-filter on it. */
  brushField?: string | null;
  /** Write the brush signal on every move (default) or only on drag end. */
  liveBrush?: boolean;
  /** Pixels of pointer travel before a press becomes a brush drag (default 4). */
  dragThresholdPx?: number;
  /** Pixel→data-space mapper for the brush. Default identity. */
  toDataSpace?: DataSpaceMapper;
  /** Wire keyboard listeners (Escape / arrows). Default true. */
  enableKeyboard?: boolean;
  /** Fired when hover enters/moves to a draw item (drawItemId != null). */
  onHoverEnter?: (h: HoverState) => void;
  /** Fired when hover exits (pointer leaves an item / the canvas). */
  onHoverExit?: () => void;
}

/** A pointer-ish event we read. Real PointerEvent/MouseEvent satisfy it; tests
 *  pass a plain object. clientX/Y + button + modifiers are all we use. */
interface PointerLike {
  clientX?: number;
  clientY?: number;
  offsetX?: number;
  offsetY?: number;
  button?: number;
  shiftKey?: boolean;
  ctrlKey?: boolean;
  metaKey?: boolean;
  pointerId?: number;
  preventDefault?: () => void;
}

interface KeyLike {
  key?: string;
  preventDefault?: () => void;
}

const IDENTITY: DataSpaceMapper = (px, py) => ({ x: px, y: py });

/**
 * Attaches to a target on construction; call `detach()` to remove all listeners.
 * The routed state lands in `this.store`.
 */
export class EventSurface {
  readonly store: SignalStore;
  private readonly target: ListenTarget;
  private readonly engine: PickEngine;

  private readonly hoverSignal: string;
  private readonly selectionSignal: string;
  private readonly brushSignal: string;
  private readonly selectionMode: SelectionModeName;
  private readonly brushMode: BrushModeName;
  private readonly brushField: string | null;
  private readonly liveBrush: boolean;
  private readonly dragThreshold: number;
  private readonly toDataSpace: DataSpaceMapper;
  private readonly enableKeyboard: boolean;
  private readonly onHoverEnter?: (h: HoverState) => void;
  private readonly onHoverExit?: () => void;

  // Drag/press state.
  private pressing = false;
  private dragging = false;
  private pressStartPx = { x: 0, y: 0 };
  private brushStartData = { x: 0, y: 0 };
  private pressToggle = false; // shift/ctrl/meta held at press
  // Last hovered draw item, to emit enter/exit exactly on change (HoverManager).
  private hoveredId: number | null = null;
  // Monotonic token so a slow async pick can't clobber a newer hover result.
  private hoverSeq = 0;

  private detached = false;
  private readonly bound: Array<{ type: string; fn: (ev: Event) => void }> = [];

  constructor(target: ListenTarget, engine: PickEngine, options: EventSurfaceOptions = {}) {
    this.target = target;
    this.engine = engine;
    this.store = options.store ?? new SignalStore();
    this.hoverSignal = options.hoverSignal ?? "hover";
    this.selectionSignal = options.selectionSignal ?? "selection";
    this.brushSignal = options.brushSignal ?? "brush";
    this.selectionMode = options.selectionMode ?? "single";
    this.brushMode = options.brushMode ?? "x";
    this.brushField = options.brushField ?? null;
    this.liveBrush = options.liveBrush ?? true;
    this.dragThreshold = options.dragThresholdPx ?? 4;
    this.toDataSpace = options.toDataSpace ?? IDENTITY;
    this.enableKeyboard = options.enableKeyboard ?? true;
    this.onHoverEnter = options.onHoverEnter;
    this.onHoverExit = options.onHoverExit;

    // Define the signals up front (empty) so subscribers can attach before the
    // first event and predicates read a defined value.
    if (!this.store.has(this.hoverSignal))
      this.store.define(this.hoverSignal, { kind: "hover", rowId: null, drawItemId: null, active: false });
    if (!this.store.has(this.selectionSignal))
      this.store.define(this.selectionSignal, { kind: "multi", intervals: [], rows: [] });
    if (!this.store.has(this.brushSignal))
      this.store.define(this.brushSignal, { kind: "brush", x0: 0, y0: 0, x1: 0, y1: 0 });

    this.attach();
  }

  private attach(): void {
    this.on("pointermove", (e) => this.onPointerMove(e as unknown as PointerLike));
    this.on("pointerdown", (e) => this.onPointerDown(e as unknown as PointerLike));
    this.on("pointerup", (e) => this.onPointerUp(e as unknown as PointerLike));
    this.on("pointerleave", () => this.onPointerLeave());
    this.on("pointercancel", () => this.onPointerLeave());
    if (this.enableKeyboard) {
      this.on("keydown", (e) => this.onKeyDown(e as unknown as KeyLike));
    }
  }

  private on(type: string, fn: (ev: Event) => void): void {
    this.target.addEventListener(type, fn);
    this.bound.push({ type, fn });
  }

  /** Remove all DOM listeners. Idempotent. The store is left intact (a shared
   *  store outlives any one surface). */
  detach(): void {
    if (this.detached) return;
    this.detached = true;
    for (const { type, fn } of this.bound) this.target.removeEventListener(type, fn);
    this.bound.length = 0;
  }

  // ---- coordinate helpers -------------------------------------------------

  /** Element-local pixel coords from an event: prefer offsetX/Y, else derive from
   *  clientX/Y minus the target's bounding rect. */
  private localPx(e: PointerLike): { x: number; y: number } {
    if (typeof e.offsetX === "number" && typeof e.offsetY === "number") {
      return { x: e.offsetX, y: e.offsetY };
    }
    const cx = e.clientX ?? 0;
    const cy = e.clientY ?? 0;
    const rect = this.target.getBoundingClientRect?.();
    return rect ? { x: cx - rect.left, y: cy - rect.top } : { x: cx, y: cy };
  }

  // ---- hover (HoverManager) ----------------------------------------------

  private onPointerMove(e: PointerLike): void {
    if (this.pressing) {
      this.updateDrag(e);
      return;
    }
    const { x, y } = this.localPx(e);
    const seq = ++this.hoverSeq;
    void this.engine
      .pickAsync(x, y)
      .then((res) => {
        // Drop stale results (a newer move already superseded this pick).
        if (seq !== this.hoverSeq || this.detached) return;
        this.applyHover(res ? res.drawItemId : null);
      })
      .catch(() => {
        /* transient pick failure: leave hover unchanged */
      });
  }

  /** Update the hover signal + fire enter/exit ONLY on a change of hovered id
   *  (the HoverManager enter/exit contract). */
  private applyHover(drawItemId: number | null): void {
    if (drawItemId === this.hoveredId) return;
    this.hoveredId = drawItemId;
    if (drawItemId === null) {
      const h: HoverState = { kind: "hover", rowId: null, drawItemId: null, active: false };
      this.store.put(this.hoverSignal, h);
      this.onHoverExit?.();
    } else {
      const h: HoverState = { kind: "hover", rowId: null, drawItemId, active: true };
      this.store.put(this.hoverSignal, h);
      this.onHoverEnter?.(h);
    }
  }

  private onPointerLeave(): void {
    if (this.pressing) {
      // Treat a leave mid-drag as a cancel of the brush.
      this.cancelDrag();
    }
    this.hoverSeq++; // invalidate any in-flight pick
    this.applyHover(null);
  }

  // ---- press / click / brush (SelectionState + BrushGesture) --------------

  private onPointerDown(e: PointerLike): void {
    // Only the primary button drives select/brush (button 0; some synthetic
    // events omit it, treat undefined as primary).
    if (e.button !== undefined && e.button !== 0) return;
    this.pressing = true;
    this.dragging = false;
    this.pressToggle = Boolean(e.shiftKey || e.ctrlKey || e.metaKey);
    const px = this.localPx(e);
    this.pressStartPx = px;
    this.brushStartData = this.toDataSpace(px.x, px.y);
  }

  private updateDrag(e: PointerLike): void {
    const px = this.localPx(e);
    if (!this.dragging) {
      const dx = px.x - this.pressStartPx.x;
      const dy = px.y - this.pressStartPx.y;
      if (Math.hypot(dx, dy) < this.dragThreshold) return; // still a click, not a drag
      // Cross the threshold → this press is a brush drag (BrushGesture.begin).
      this.dragging = true;
    }
    const data = this.toDataSpace(px.x, px.y);
    if (this.liveBrush) this.writeBrush(this.brushStartData, data);
  }

  private onPointerUp(e: PointerLike): void {
    if (!this.pressing) return;
    const wasDragging = this.dragging;
    const px = this.localPx(e);
    this.pressing = false;
    this.dragging = false;

    if (wasDragging) {
      // BrushGesture.end — always write the final rect.
      const data = this.toDataSpace(px.x, px.y);
      this.writeBrush(this.brushStartData, data);
      return;
    }
    // No drag → a click: pick under the release point and update selection.
    const seq = ++this.hoverSeq; // a click also refreshes hover context
    void this.engine
      .pickAsync(px.x, px.y)
      .then((res) => {
        if (this.detached) return;
        this.applyClick(res ? res.drawItemId : null, this.pressToggle);
        // Keep hover consistent with what's now under the cursor.
        if (seq === this.hoverSeq) this.applyHover(res ? res.drawItemId : null);
      })
      .catch(() => {
        /* ignore transient pick failure */
      });
  }

  /** Apply a click to the selection signal (SelectionState single/toggle). A click
   *  on empty space in single mode clears; in toggle mode it is a no-op. */
  private applyClick(id: number | null, forceToggle: boolean): void {
    const toggle = forceToggle || this.selectionMode === "toggle";
    const cur = this.store.getAs(this.selectionSignal, "multi");
    const rows = cur ? [...cur.rows] : [];
    if (id === null) {
      if (!toggle) this.setSelectionRows([]); // single-mode click clears
      return;
    }
    if (toggle) {
      const at = rows.indexOf(id);
      if (at >= 0) rows.splice(at, 1);
      else rows.push(id);
      this.setSelectionRows(rows);
    } else {
      this.setSelectionRows([id]); // single: replace
    }
  }

  private setSelectionRows(rows: number[]): void {
    const v: MultiSelection = { kind: "multi", intervals: [], rows };
    this.store.put(this.selectionSignal, v);
  }

  /** Write the brush rect into the signal per mode (BrushGesture.writeSignal). For
   *  x/y interval modes we still emit a BrushRect but collapse the unused axis so a
   *  consumer reads it as a band; the field is carried on the store for x/y modes
   *  via a paired interval when needed. */
  private writeBrush(a: { x: number; y: number }, b: { x: number; y: number }): void {
    let rect: BrushRect;
    switch (this.brushMode) {
      case "x":
        // Vertical band spanning full y — collapse y to 0..0 sentinel meaning
        // "no y constraint"; consumers read [minX,maxX].
        rect = { kind: "brush", x0: a.x, y0: 0, x1: b.x, y1: 0 };
        break;
      case "y":
        rect = { kind: "brush", x0: 0, y0: a.y, x1: 0, y1: b.y };
        break;
      case "rect":
      default:
        rect = { kind: "brush", x0: a.x, y0: a.y, x1: b.x, y1: b.y };
        break;
    }
    this.store.put(this.brushSignal, rect);
    // For x/y interval modes, ALSO publish a typed interval carrying the field so a
    // cross-filter (ENC-639) can read a value predicate directly.
    if (this.brushMode === "x") {
      this.store.put(this.intervalSignalName(), {
        kind: "interval",
        field: this.brushField,
        lo: Math.min(a.x, b.x),
        hi: Math.max(a.x, b.x),
      });
    } else if (this.brushMode === "y") {
      this.store.put(this.intervalSignalName(), {
        kind: "interval",
        field: this.brushField,
        lo: Math.min(a.y, b.y),
        hi: Math.max(a.y, b.y),
      });
    }
  }

  /** Name of the interval signal paired with an x/y brush ("<brush>.interval"). */
  private intervalSignalName(): string {
    return `${this.brushSignal}.interval`;
  }

  private cancelDrag(): void {
    if (!this.pressing) return;
    this.pressing = false;
    this.dragging = false;
    // BrushGesture.cancel → clear the brush selection.
    this.store.clear(this.brushSignal);
    this.store.clear(this.intervalSignalName());
  }

  // ---- keyboard (SelectionState nav + Escape) -----------------------------

  private onKeyDown(e: KeyLike): void {
    switch (e.key) {
      case "Escape":
        e.preventDefault?.();
        if (this.pressing) this.cancelDrag();
        this.store.clear(this.selectionSignal);
        this.store.clear(this.brushSignal);
        this.store.clear(this.intervalSignalName());
        break;
      case "ArrowRight":
      case "ArrowDown":
        e.preventDefault?.();
        this.navigateSelection(+1);
        break;
      case "ArrowLeft":
      case "ArrowUp":
        e.preventDefault?.();
        this.navigateSelection(-1);
        break;
      default:
        break;
    }
  }

  /** Step the single-selection to the neighbouring draw item id (SelectionState
   *  selectNext/selectPrevious over the currently selected id). No-op if the
   *  selection is empty or multi. */
  private navigateSelection(delta: number): void {
    const cur = this.store.getAs(this.selectionSignal, "multi");
    if (!cur || cur.rows.length !== 1) return;
    const next = cur.rows[0] + delta;
    if (next < 0) return;
    this.setSelectionRows([next]);
  }
}
