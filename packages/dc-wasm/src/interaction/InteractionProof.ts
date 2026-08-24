/* packages/dc-wasm/src/interaction/InteractionProof.ts — ENC-640 (G1)
 *
 * THE END-TO-END PROOF DEMO of the interaction layer on an INSTANCED view (one
 * DrawItem, many instances — the treemap / scatter shape, exactly the C4 golden's
 * ENC-630 4×4 instanced grid). It composes the surfaces the earlier phases landed
 * into the four proof behaviours the ticket calls for:
 *
 *   1. CLICK  → per-instance DRILL-DOWN   (SelectionState → `selection` signal)
 *   2. HOVER  → highlight + TOOLTIP       (HoverManager  → `hover` signal)
 *   3. BRUSH  → FILTER / de-emphasis      (BrushGesture  → interval predicate)
 *   4. DATA   → smooth ENTER/EXIT/UPDATE TRANSITIONS keyed by RowIdentity
 *
 * WHY A CPU-SIDE PER-INSTANCE HIT-TEST (and why that is NOT a shim): the prebuilt
 * dc_engine_host.wasm's pick(w,h,x,y) returns the drawItemId ONLY — the per-instance
 * row-id pass (Phase C: PickResult.rowId, ENC-627/628/630) is not compiled into the
 * shipped artifact and emcc is unavailable here to add it. SPEC open-Q#1/#4 place the
 * per-instance decode + hit→signal routing in the host layer, and the ticket
 * explicitly sanctions deriving the instance "via your own CPU-side hit-test against
 * the instance buffer you uploaded". So this class hit-tests the SAME instance
 * geometry it would upload to the engine, resolves the durable rowId, and threads it
 * through the SAME `HoverState.rowId` / selection lanes SignalStore already reserves
 * for the day the wasm gains per-instance pick. The value/brush channel needs no
 * pick at all — it rides the real EventSurface interval predicate end-to-end.
 *
 * Pure + DOM-free at its core (every behaviour is a plain method over data-space
 * coords); `attach()` is the thin real-surface wiring that drives those methods
 * from the real EventSurface brush routing + real pointer events.
 */

import type { PickEngine, ListenTarget, DataSpaceMapper } from "./EventSurface";
import { EventSurface, type EventSurfaceOptions } from "./EventSurface";
import {
  SignalStore,
  type HoverState,
  type MultiSelection,
} from "./SignalStore";
import {
  TransitionController,
  type TransitionOptions,
  type TransitionPhase,
} from "./Transitions";

/** One instance of an instanced mark: a durable row id, an axis-aligned bounding
 *  rect in DATA space (treemap cell / scatter-point bbox) for hit-testing, and the
 *  scalar field value the brush filters on. Mirrors an `EncodeResult` instance +
 *  its `instanceRowIds` entry. */
export interface InstanceDatum {
  /** Durable source row id (RowIdentity). Object constancy keys off this. */
  rowId: number;
  x0: number;
  y0: number;
  x1: number;
  y1: number;
  /** Scalar the brush-to-filter predicate tests (e.g. the x-axis field value). */
  value: number;
  /** Optional label surfaced in the hover tooltip. */
  label?: string;
}

/** An instanced view: the single instanced DrawItem the wasm pick returns, plus its
 *  instances. `filterField` is the field id the interval brush cross-filters on. */
export interface InstancedView {
  drawItemId: number;
  instances: InstanceDatum[];
  filterField?: string;
}

/** What the tooltip renders for the hovered instance. null when nothing is hovered. */
export interface TooltipInfo {
  rowId: number;
  value: number;
  label?: string;
  /** Anchor in data space (rect center). */
  x: number;
  y: number;
}

/** The resolved per-instance visual state the render side pushes back to the engine
 *  each frame (the "re-encode reads the predicate" output). */
export interface InstanceVisualState {
  rowId: number;
  /** Passes the brush predicate (inside the brushed interval / rect). */
  included: boolean;
  /** Pointer is over this instance. */
  hovered: boolean;
  /** In the current selection (click drill-down / toggle union). */
  selected: boolean;
  /** Transition progress in [0,1] (enter/exit fade+scale). */
  progress: number;
  transitionPhase: TransitionPhase | null;
  /** Final opacity: transition fade × brush de-emphasis, hover/selection boosted. */
  alpha: number;
  /** Final scale: transition scale (enter/exit); selection/hover may enlarge. */
  scale: number;
}

export interface InteractionProofOptions {
  /** Shared signal sink (pass one across hosts for linked brushing, ENC-639). */
  store?: SignalStore;
  hoverSignal?: string;
  selectionSignal?: string;
  brushSignal?: string;
  /** Field id carried on the interval brush so it cross-filters on `value`. */
  filterField?: string;
  /** Opacity of instances that FAIL the brush predicate (de-emphasis, not hide).
   *  Default 0.15 — the "filter/de-emphasise non-selected rows" behaviour. */
  deemphasizedAlpha?: number;
  /** Extra scale applied to a hovered/selected instance (highlight). Default 1.12. */
  emphasisScale?: number;
  /** Enter/exit transition tuning (see TransitionController). */
  transition?: TransitionOptions;
  /** Selection replace-vs-union default; shift/ctrl/meta always toggles. */
  selectionMode?: "single" | "toggle";
}

/** Result of `attach()` — the live EventSurface plus a single teardown. */
export interface AttachedProof {
  surface: EventSurface;
  detach(): void;
}

const clamp01 = (v: number): number => (v < 0 ? 0 : v > 1 ? 1 : v);

/**
 * The proof orchestrator. Construct with a view + a shared `SignalStore`; drive it
 * with `hoverAtData` / `clickAtData` / `tick`, or wire it to real DOM + the real
 * EventSurface brush routing with `attach()`. Read `instanceState(rowId)` (or
 * `visualStates()`) each frame for the per-instance render state.
 */
export class InteractionProof {
  readonly store: SignalStore;
  private view: InstancedView;
  private readonly hoverSignal: string;
  private readonly selectionSignal: string;
  private readonly brushSignal: string;
  private readonly intervalSignal: string;
  private readonly filterField: string | null;
  private readonly deemphAlpha: number;
  private readonly emphasisScale: number;
  private readonly selectionMode: "single" | "toggle";
  private readonly transitions: TransitionController;

  private hoveredRow: number | null = null;

  constructor(view: InstancedView, options: InteractionProofOptions = {}) {
    this.view = view;
    this.store = options.store ?? new SignalStore();
    this.hoverSignal = options.hoverSignal ?? "hover";
    this.selectionSignal = options.selectionSignal ?? "selection";
    this.brushSignal = options.brushSignal ?? "brush";
    this.intervalSignal = `${this.brushSignal}.interval`;
    this.filterField = options.filterField ?? view.filterField ?? null;
    this.deemphAlpha = options.deemphasizedAlpha ?? 0.15;
    this.emphasisScale = options.emphasisScale ?? 1.12;
    this.selectionMode = options.selectionMode ?? "single";
    this.transitions = new TransitionController(options.transition);

    // Define the signals so subscribers/predicates read a defined value up front
    // (same contract as EventSurface's constructor).
    if (!this.store.has(this.hoverSignal))
      this.store.define(this.hoverSignal, {
        kind: "hover",
        rowId: null,
        drawItemId: null,
        active: false,
      });
    if (!this.store.has(this.selectionSignal))
      this.store.define(this.selectionSignal, { kind: "multi", intervals: [], rows: [] });

    // Seed transitions with the initial row set (all ENTER on first mount).
    this.transitions.syncRows(this.rowIds());
  }

  // ---- data / transitions -------------------------------------------------

  private rowIds(): number[] {
    return this.view.instances.map((i) => i.rowId);
  }

  /**
   * Replace the instance set (a data append / eviction / update). Diffs durable row
   * ids old→new and starts ENTER/EXIT tweens (object constancy) — the render side
   * animates them via `tick`. Instances that persist keep their identity (no
   * popping). Exiting rows are still rendered (fading out) until their tween ends.
   */
  setInstances(instances: InstanceDatum[]): void {
    this.view = { ...this.view, instances };
    this.transitions.syncRows(this.rowIds());
    // Drop hover if the hovered row left the live set.
    if (this.hoveredRow !== null && !instances.some((i) => i.rowId === this.hoveredRow)) {
      this.setHover(null);
    }
  }

  /** Advance transitions one frame (FrameClock heartbeat). Returns true if still
   *  animating (host should schedule another frame). */
  tick(dtSeconds: number): boolean {
    return this.transitions.tick(dtSeconds);
  }

  isAnimating(): boolean {
    return this.transitions.isAnimating();
  }

  // ---- per-instance CPU hit-test (the wasm pick workaround) ---------------

  /**
   * Resolve the instance under a DATA-space point, or null. Iterates in reverse so
   * the LAST-drawn (topmost) instance wins on overlap — the CPU analogue of the
   * pick pass's front-to-back id resolution. This is the per-instance decode the
   * shipped wasm cannot do (pick → drawItemId only, see file header).
   */
  resolveInstanceAt(dataX: number, dataY: number): InstanceDatum | null {
    const inst = this.view.instances;
    for (let i = inst.length - 1; i >= 0; i--) {
      const d = inst[i];
      const minX = Math.min(d.x0, d.x1);
      const maxX = Math.max(d.x0, d.x1);
      const minY = Math.min(d.y0, d.y1);
      const maxY = Math.max(d.y0, d.y1);
      if (dataX >= minX && dataX <= maxX && dataY >= minY && dataY <= maxY) return d;
    }
    return null;
  }

  // ---- hover (HoverManager → hover signal) --------------------------------

  /**
   * Update hover from a DATA-space pointer position: CPU-resolve the instance,
   * write the `hover` signal (WITH the resolved rowId — the reserved per-instance
   * lane, populated CPU-side today) and return the tooltip payload (null on a miss).
   * Fires only on a CHANGE of hovered row (HoverManager enter/exit contract).
   */
  hoverAtData(dataX: number, dataY: number): TooltipInfo | null {
    const inst = this.resolveInstanceAt(dataX, dataY);
    this.setHover(inst ? inst.rowId : null);
    return inst ? this.tooltipFor(inst) : null;
  }

  private setHover(rowId: number | null): void {
    if (rowId === this.hoveredRow) return;
    this.hoveredRow = rowId;
    const h: HoverState =
      rowId === null
        ? { kind: "hover", rowId: null, drawItemId: null, active: false }
        : { kind: "hover", rowId, drawItemId: this.view.drawItemId, active: true };
    this.store.put(this.hoverSignal, h);
  }

  /** The row currently hovered (null if none). */
  hovered(): number | null {
    return this.hoveredRow;
  }

  /** Tooltip payload for the hovered instance, or null. */
  tooltip(): TooltipInfo | null {
    if (this.hoveredRow === null) return null;
    const inst = this.view.instances.find((i) => i.rowId === this.hoveredRow);
    return inst ? this.tooltipFor(inst) : null;
  }

  private tooltipFor(d: InstanceDatum): TooltipInfo {
    return {
      rowId: d.rowId,
      value: d.value,
      label: d.label,
      x: (d.x0 + d.x1) / 2,
      y: (d.y0 + d.y1) / 2,
    };
  }

  // ---- click drill-down (SelectionState → selection signal) ---------------

  /**
   * Apply a click at a DATA-space point: CPU-resolve the instance and update the
   * `selection` signal by durable rowId (drill-down). `single` replaces, `toggle`
   * (or shift/ctrl/meta) unions. A click on empty space clears in single mode, no-op
   * in toggle. Returns the drilled-into instance, or null on a miss.
   */
  clickAtData(dataX: number, dataY: number, forceToggle = false): InstanceDatum | null {
    const inst = this.resolveInstanceAt(dataX, dataY);
    const toggle = forceToggle || this.selectionMode === "toggle";
    const cur = this.store.getAs(this.selectionSignal, "multi");
    const rows = cur ? [...cur.rows] : [];
    if (!inst) {
      if (!toggle) this.setSelectionRows([]);
      return null;
    }
    if (toggle) {
      const at = rows.indexOf(inst.rowId);
      if (at >= 0) rows.splice(at, 1);
      else rows.push(inst.rowId);
      this.setSelectionRows(rows);
    } else {
      this.setSelectionRows([inst.rowId]);
    }
    return inst;
  }

  private setSelectionRows(rows: number[]): void {
    const v: MultiSelection = { kind: "multi", intervals: [], rows };
    this.store.put(this.selectionSignal, v);
  }

  /** The currently selected row ids (drill-down result). */
  selectedRows(): number[] {
    const cur = this.store.getAs(this.selectionSignal, "multi");
    return cur ? [...cur.rows] : [];
  }

  isSelected(rowId: number): boolean {
    const cur = this.store.getAs(this.selectionSignal, "multi");
    return cur ? cur.rows.includes(rowId) : false;
  }

  // ---- brush-to-filter (interval / rect predicate) ------------------------

  /**
   * Does an instance PASS the current brush predicate? Reads the live signal store
   * (whatever the real EventSurface brush wrote): an interval brush filters on the
   * scalar `value` via SignalStore.matchesValue; a 2-D rect brush filters on the
   * instance CENTER being inside the rect. No brush → everything passes. This is the
   * selection-filter transform read — zero manifest grammar change (SPEC C).
   */
  isIncluded(rowId: number): boolean {
    const inst = this.view.instances.find((i) => i.rowId === rowId);
    if (!inst) return false;
    // Interval brush (x/y band): value predicate carrying the field id.
    const iv = this.store.getAs(this.intervalSignal, "interval");
    if (iv && iv.field !== null) {
      return this.store.matchesValue(this.intervalSignal, inst.value);
    }
    // 2-D rect brush: geometric containment of the instance center.
    const br = this.store.getAs(this.brushSignal, "brush");
    if (br && !(br.x0 === br.x1 && br.y0 === br.y1)) {
      const cx = (inst.x0 + inst.x1) / 2;
      const cy = (inst.y0 + inst.y1) / 2;
      const inX = cx >= Math.min(br.x0, br.x1) && cx <= Math.max(br.x0, br.x1);
      const inY = cy >= Math.min(br.y0, br.y1) && cy <= Math.max(br.y0, br.y1);
      return inX && inY;
    }
    return true; // no active brush → no constraint
  }

  /** True iff any brush constraint is currently active (something is being filtered). */
  hasActiveBrush(): boolean {
    const iv = this.store.getAs(this.intervalSignal, "interval");
    if (iv && iv.field !== null) return true;
    const br = this.store.getAs(this.brushSignal, "brush");
    return !!br && !(br.x0 === br.x1 && br.y0 === br.y1);
  }

  // ---- combined per-instance visual state (the re-encode output) ----------

  /**
   * The resolved render state for one row: transition progress × brush de-emphasis,
   * with hover/selection emphasis. `alpha` and `scale` are what the render side
   * multiplies into the per-instance opacity / size lanes (InstanceTransition E3).
   * Exiting rows (not in the live set but still tracked) fade out at full inclusion.
   */
  instanceState(rowId: number): InstanceVisualState {
    const progress = clamp01(this.transitions.progressOf(rowId));
    const phase = this.transitions.phaseOf(rowId);
    const live = this.view.instances.some((i) => i.rowId === rowId);
    const included = live ? this.isIncluded(rowId) : true;
    const hovered = this.hoveredRow === rowId;
    const selected = this.isSelected(rowId);

    // Brush de-emphasis: excluded rows dim to deemphAlpha, unless the pointer/
    // selection is on them (emphasis wins so you can still inspect a filtered row).
    let alpha = progress;
    if (!included && !hovered && !selected) alpha *= this.deemphAlpha;

    // Enter/exit scale rides the transition progress; hover/selection enlarge.
    let scale = progress;
    if (hovered || selected) scale = Math.max(scale, 1) * this.emphasisScale;

    return {
      rowId,
      included,
      hovered,
      selected,
      progress,
      transitionPhase: phase,
      alpha: clamp01(alpha),
      scale,
    };
  }

  /**
   * Visual state for every row that should render this frame: the live instances
   * PLUS any still-exiting rows the transition controller is fading out (object
   * constancy — they keep rendering until their exit tween completes).
   */
  visualStates(): InstanceVisualState[] {
    const ids = new Set<number>(this.rowIds());
    for (const id of this.transitions.trackedRows()) ids.add(id);
    return [...ids].map((id) => this.instanceState(id));
  }

  // ---- real-surface wiring ------------------------------------------------

  /**
   * Wire the proof to a real DOM target + pick engine. Constructs the ENC-634
   * EventSurface (so the BRUSH channel runs end-to-end through the real
   * pointer→pick→interval-signal routing, sharing this proof's store), then adds
   * pointermove/pointerup taps that CPU-resolve the per-instance hover/click the
   * shipped wasm pick cannot (see header). `toDataSpace` maps element-local pixels →
   * data space and is shared with the brush so both channels agree on coordinates.
   *
   * Returns the surface + a single `detach()` that tears down both.
   */
  attach(
    target: ListenTarget,
    engine: PickEngine,
    toDataSpace: DataSpaceMapper,
    surfaceOptions: EventSurfaceOptions = {},
  ): AttachedProof {
    // EventSurface owns the BRUSH channel end-to-end (shared brushSignal → the
    // interval predicate this proof reads). Its OWN drawItem-level hover/selection
    // are namespaced onto sibling signals so they never clobber the proof's
    // per-instance hover/selection (which carry the CPU-resolved rowId).
    const surface = new EventSurface(target, engine, {
      ...surfaceOptions,
      store: this.store,
      hoverSignal: `${this.hoverSignal}.drawitem`,
      selectionSignal: `${this.selectionSignal}.drawitem`,
      brushSignal: this.brushSignal,
      brushMode: surfaceOptions.brushMode ?? "x",
      brushField: this.filterField,
      toDataSpace,
    });

    // Per-instance taps. EventSurface owns the drawItem-level hover pick + brush;
    // these refine to the instance CPU-side (real pointer coords, real geometry).
    let pressX = 0;
    let pressY = 0;
    let pressToggle = false;
    let moved = false;
    const DRAG = surfaceOptions.dragThresholdPx ?? 4;

    const localPx = (e: {
      offsetX?: number;
      offsetY?: number;
      clientX?: number;
      clientY?: number;
    }): { x: number; y: number } => {
      if (typeof e.offsetX === "number" && typeof e.offsetY === "number") {
        return { x: e.offsetX, y: e.offsetY };
      }
      const rect = target.getBoundingClientRect?.();
      const cx = e.clientX ?? 0;
      const cy = e.clientY ?? 0;
      return rect ? { x: cx - rect.left, y: cy - rect.top } : { x: cx, y: cy };
    };

    const onMove = (ev: Event): void => {
      const px = localPx(ev as unknown as { offsetX?: number; offsetY?: number });
      if (
        Math.hypot(px.x - pressX, px.y - pressY) >= DRAG &&
        (ev as unknown as { buttons?: number }).buttons
      ) {
        moved = true; // a drag (brush) is in progress — don't hover-highlight
        return;
      }
      const d = toDataSpace(px.x, px.y);
      this.hoverAtData(d.x, d.y);
    };
    const onDown = (ev: Event): void => {
      const e = ev as unknown as {
        offsetX?: number;
        offsetY?: number;
        shiftKey?: boolean;
        ctrlKey?: boolean;
        metaKey?: boolean;
      };
      const px = localPx(e);
      pressX = px.x;
      pressY = px.y;
      pressToggle = Boolean(e.shiftKey || e.ctrlKey || e.metaKey);
      moved = false;
    };
    const onUp = (ev: Event): void => {
      const px = localPx(ev as unknown as { offsetX?: number; offsetY?: number });
      const isDrag = moved || Math.hypot(px.x - pressX, px.y - pressY) >= DRAG;
      moved = false;
      if (isDrag) return; // a brush, not a click — EventSurface handled it
      const d = toDataSpace(px.x, px.y);
      this.clickAtData(d.x, d.y, pressToggle);
    };
    const onLeave = (): void => {
      this.setHover(null);
    };

    target.addEventListener("pointermove", onMove);
    target.addEventListener("pointerdown", onDown);
    target.addEventListener("pointerup", onUp);
    target.addEventListener("pointerleave", onLeave);

    let detached = false;
    return {
      surface,
      detach: () => {
        if (detached) return;
        detached = true;
        target.removeEventListener("pointermove", onMove);
        target.removeEventListener("pointerdown", onDown);
        target.removeEventListener("pointerup", onUp);
        target.removeEventListener("pointerleave", onLeave);
        surface.detach();
      },
    };
  }
}
