/* ENC-640 (G1): the end-to-end interaction PROOF on an instanced view. These tests
 * exercise the four acceptance behaviours on the C4-golden shape (a 4×4 instanced
 * grid, one DrawItem, 16 instances, durable row ids 1000+index — mirrors ENC-630):
 *
 *   1. click  → per-instance drill-down (selection signal, by rowId)
 *   2. hover  → highlight + tooltip     (hover signal, rowId populated CPU-side)
 *   3. brush  → filter / de-emphasis    (interval predicate; incl. cross-view)
 *   4. data   → smooth enter/exit/update transitions keyed by RowIdentity
 *
 * The pure methods are driven directly; the final block drives the REAL ENC-634
 * EventSurface via `attach()` with synthetic pointer events + a fake pick engine
 * (the EventSurface.test.ts injection style — no DOM/jsdom/wasm), proving the brush
 * routing and per-instance pointer routing wire together on the real surface.
 */

import { describe, it, expect, vi } from "vitest";
import { InteractionProof, type InstanceDatum } from "./InteractionProof";
import { SignalStore, type HoverState, type MultiSelection } from "./SignalStore";

/** A 4×4 instanced grid in data space [0,4]×[0,4]: cell (r,c) is the unit rect at
 *  (c,r)→(c+1,r+1), durable rowId 1000+r*4+c, filter value = the column (x field).
 *  Matches the ENC-630 C4 golden layout (row-major from bottom-left). */
function grid(): InstanceDatum[] {
  const out: InstanceDatum[] = [];
  for (let r = 0; r < 4; r++) {
    for (let c = 0; c < 4; c++) {
      out.push({
        rowId: 1000 + r * 4 + c,
        x0: c,
        y0: r,
        x1: c + 1,
        y1: r + 1,
        value: c,
        label: `r${r}c${c}`,
      });
    }
  }
  return out;
}

const DRAW_ITEM_ID = 42;
const makeProof = (store = new SignalStore(), opts = {}) =>
  new InteractionProof(
    { drawItemId: DRAW_ITEM_ID, instances: grid(), filterField: "x" },
    { store, filterField: "x", ...opts },
  );

/** A fake DOM target that records listeners + dispatches synthetic events. */
function makeTarget() {
  const listeners = new Map<string, Array<(ev: Event) => void>>();
  return {
    target: {
      addEventListener(type: string, fn: (ev: Event) => void) {
        const arr = listeners.get(type) ?? [];
        arr.push(fn);
        listeners.set(type, arr);
      },
      removeEventListener(type: string, fn: (ev: Event) => void) {
        const arr = listeners.get(type);
        if (!arr) return;
        const i = arr.indexOf(fn);
        if (i >= 0) arr.splice(i, 1);
      },
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 400, height: 400 }),
    },
    dispatch(type: string, ev: Record<string, unknown>) {
      for (const fn of listeners.get(type) ?? []) fn(ev as unknown as Event);
    },
    count(type: string) {
      return (listeners.get(type) ?? []).length;
    },
  };
}

const flush = () => new Promise<void>((r) => setTimeout(r, 0));

// ============================================================================
// 1. CLICK → per-instance drill-down
// ============================================================================
describe("ENC-640 · click drill-down (per-instance pick → selection signal)", () => {
  it("resolves the clicked cell to its durable rowId (CPU per-instance pick)", () => {
    const p = makeProof();
    // Center of cell (r0,c0) = data (0.5,0.5) → rowId 1000.
    const hit = p.clickAtData(0.5, 0.5);
    expect(hit?.rowId).toBe(1000);
    expect(p.selectedRows()).toEqual([1000]);
    // Center of cell (r2,c3) = (3.5,2.5) → rowId 1000 + 2*4+3 = 1011.
    p.clickAtData(3.5, 2.5);
    expect(p.selectedRows()).toEqual([1011]); // single mode replaces
    expect(p.isSelected(1011)).toBe(true);
    expect(p.isSelected(1000)).toBe(false);
  });

  it("a click on a gap/outside clears selection in single mode", () => {
    const p = makeProof();
    p.clickAtData(0.5, 0.5);
    expect(p.selectedRows()).toEqual([1000]);
    p.clickAtData(10, 10); // outside the grid
    expect(p.selectedRows()).toEqual([]);
  });

  it("toggle (shift/ctrl) unions multiple rows for multi-drill-down", () => {
    const p = makeProof();
    p.clickAtData(0.5, 0.5, true); // +1000
    p.clickAtData(1.5, 0.5, true); // +1001
    expect(p.selectedRows().sort()).toEqual([1000, 1001]);
    p.clickAtData(0.5, 0.5, true); // toggle 1000 off
    expect(p.selectedRows()).toEqual([1001]);
  });

  it("distinct cells resolve to distinct rows across the whole 4×4 grid", () => {
    const p = makeProof();
    const seen = new Set<number>();
    for (let r = 0; r < 4; r++) {
      for (let c = 0; c < 4; c++) {
        const hit = p.resolveInstanceAt(c + 0.5, r + 0.5);
        expect(hit?.rowId).toBe(1000 + r * 4 + c);
        seen.add(hit!.rowId);
      }
    }
    expect(seen.size).toBe(16); // every cell round-trips to a unique row
  });
});

// ============================================================================
// 2. HOVER → highlight + tooltip
// ============================================================================
describe("ENC-640 · hover highlight + tooltip", () => {
  it("hover writes the hover signal WITH the resolved rowId + returns a tooltip", () => {
    const store = new SignalStore();
    const p = makeProof(store);
    const tip = p.hoverAtData(2.5, 1.5); // cell (r1,c2) → row 1006
    expect(tip).toEqual({ rowId: 1006, value: 2, label: "r1c2", x: 2.5, y: 1.5 });
    const h = store.getAs("hover", "hover") as HoverState;
    expect(h.active).toBe(true);
    expect(h.rowId).toBe(1006); // per-instance lane populated CPU-side
    expect(h.drawItemId).toBe(DRAW_ITEM_ID);
    expect(p.hovered()).toBe(1006);
  });

  it("moving off an instance deactivates hover + clears the tooltip", () => {
    const store = new SignalStore();
    const p = makeProof(store);
    p.hoverAtData(0.5, 0.5);
    expect(p.tooltip()?.rowId).toBe(1000);
    const tip = p.hoverAtData(10, 10); // off the grid
    expect(tip).toBeNull();
    expect(p.tooltip()).toBeNull();
    expect((store.getAs("hover", "hover") as HoverState).active).toBe(false);
  });

  it("a hovered instance is emphasized (scale boost) in its visual state", () => {
    const p = makeProof();
    p.hoverAtData(0.5, 0.5);
    p.tick(1); // settle enter transitions to progress 1
    const s = p.instanceState(1000);
    expect(s.hovered).toBe(true);
    expect(s.scale).toBeGreaterThan(1); // emphasisScale applied
  });
});

// ============================================================================
// 3. BRUSH → filter / de-emphasis (via signal predicate)
// ============================================================================
describe("ENC-640 · brush-to-filter (interval predicate, de-emphasis)", () => {
  it("no brush ⇒ every instance is included at full opacity", () => {
    const p = makeProof();
    p.tick(1);
    expect(p.hasActiveBrush()).toBe(false);
    for (const s of p.visualStates()) {
      expect(s.included).toBe(true);
      expect(s.alpha).toBeCloseTo(1, 5);
    }
  });

  it("an x-interval brush includes matching columns, de-emphasizes the rest", () => {
    const store = new SignalStore();
    const p = makeProof(store);
    p.tick(1);
    // Brush columns with value in [1.5,3.5] → columns 2 and 3 (value 2,3).
    store.put("brush.interval", { kind: "interval", field: "x", lo: 1.5, hi: 3.5 });
    expect(p.hasActiveBrush()).toBe(true);

    // Column 0 (value 0) excluded → dimmed; column 2 (value 2) included → full.
    expect(p.isIncluded(1000)).toBe(false); // r0c0, value 0
    expect(p.isIncluded(1002)).toBe(true); // r0c2, value 2
    const dim = p.instanceState(1000);
    const lit = p.instanceState(1002);
    expect(dim.included).toBe(false);
    expect(dim.alpha).toBeLessThan(lit.alpha);
    expect(dim.alpha).toBeCloseTo(0.15, 5); // default deemphasizedAlpha
    expect(lit.alpha).toBeCloseTo(1, 5);
  });

  it("hovering/selecting a filtered-out row keeps it legible (emphasis wins)", () => {
    const store = new SignalStore();
    const p = makeProof(store);
    p.tick(1);
    store.put("brush.interval", { kind: "interval", field: "x", lo: 1.5, hi: 3.5 });
    p.hoverAtData(0.5, 0.5); // hover the excluded r0c0
    const s = p.instanceState(1000);
    expect(s.included).toBe(false);
    expect(s.alpha).toBeCloseTo(1, 5); // hover overrides the de-emphasis dim
  });

  it("a 2-D rect brush filters on instance-center geometry", () => {
    const store = new SignalStore();
    const p = makeProof(store);
    // Rect over the lower-left quadrant [0,2]×[0,2] → cells (r0..1, c0..1).
    store.put("brush", { kind: "brush", x0: 0, y0: 0, x1: 2, y1: 2 });
    expect(p.isIncluded(1000)).toBe(true); // r0c0 center (0.5,0.5)
    expect(p.isIncluded(1005)).toBe(true); // r1c1 center (1.5,1.5)
    expect(p.isIncluded(1002)).toBe(false); // r0c2 center (2.5,0.5) outside
    expect(p.isIncluded(1010)).toBe(false); // r2c2 center (2.5,2.5) outside
  });
});

// ============================================================================
// 3b. CROSS-VIEW linked brushing (shared store) — proof that one brush filters two
// ============================================================================
describe("ENC-640 · linked brushing across two views (shared SignalStore)", () => {
  it("a brush written once filters both views sharing the store", () => {
    const store = new SignalStore();
    const a = makeProof(store);
    const b = makeProof(store); // second instanced view on the SAME store
    a.tick(1);
    b.tick(1);
    // A brush (as EventSurface would write it) → both A and B filter identically.
    store.put("brush.interval", { kind: "interval", field: "x", lo: -0.5, hi: 0.5 });
    expect(a.isIncluded(1000)).toBe(true); // value 0 in range
    expect(b.isIncluded(1000)).toBe(true);
    expect(a.isIncluded(1002)).toBe(false); // value 2 out
    expect(b.isIncluded(1002)).toBe(false);
  });
});

// ============================================================================
// 4. TRANSITIONS on data change (enter/exit/update, RowIdentity)
// ============================================================================
describe("ENC-640 · enter/exit/update transitions on data change", () => {
  it("initial mount enters all rows (progress ramps 0→1)", () => {
    const p = makeProof(new SignalStore(), { transition: { enterSeconds: 1, enterEasing: "linear" } });
    // Before any tick, entering rows are at progress 0 (alpha 0 — invisible).
    expect(p.instanceState(1000).progress).toBe(0);
    expect(p.isAnimating()).toBe(true);
    p.tick(0.5);
    expect(p.instanceState(1000).progress).toBeCloseTo(0.5, 5);
    p.tick(0.5);
    expect(p.instanceState(1000).progress).toBeCloseTo(1, 5);
    expect(p.isAnimating()).toBe(false);
  });

  it("appending a row ENTERS only the new one; survivors stay put (object constancy)", () => {
    const p = makeProof(new SignalStore(), { transition: { enterSeconds: 1, enterEasing: "linear" } });
    p.tick(1); // all settled
    expect(p.isAnimating()).toBe(false);

    const rows = grid();
    rows.push({ rowId: 2000, x0: 0, y0: 4, x1: 1, y1: 5, value: 0, label: "new" });
    p.setInstances(rows);
    expect(p.instanceState(2000).progress).toBe(0); // new row enters from 0
    expect(p.instanceState(1000).progress).toBeCloseTo(1, 5); // survivor unchanged
    expect(p.isAnimating()).toBe(true);
    p.tick(1);
    expect(p.instanceState(2000).progress).toBeCloseTo(1, 5);
  });

  it("evicting a row EXITS it (still rendered while fading), then drops it", () => {
    const p = makeProof(new SignalStore(), { transition: { enterSeconds: 0, exitSeconds: 1, exitEasing: "linear" } });
    p.tick(0); // instant enter
    const kept = grid().filter((d) => d.rowId !== 1000); // evict row 1000
    p.setInstances(kept);
    expect(p.instanceState(1000).transitionPhase).toBe("exit");
    // Still appears in the render set while it fades.
    expect(p.visualStates().some((s) => s.rowId === 1000)).toBe(true);
    p.tick(0.5);
    expect(p.instanceState(1000).progress).toBeCloseTo(0.5, 5);
    p.tick(0.5);
    // Fully faded → dropped from the render set.
    expect(p.visualStates().some((s) => s.rowId === 1000)).toBe(false);
    expect(p.isAnimating()).toBe(false);
  });
});

// ============================================================================
// END-TO-END through the REAL ENC-634 EventSurface (attach + synthetic events)
// ============================================================================
describe("ENC-640 · end-to-end on the real EventSurface (attach)", () => {
  // A fake pick engine: any in-bounds pixel hits the instanced DrawItem; the
  // per-instance refinement is the proof's CPU hit-test (wasm pick = drawItemId only).
  const makeEngine = () => ({
    pickAsync: vi.fn(async (x: number, y: number) =>
      x >= 0 && x <= 400 && y >= 0 && y <= 400 ? { drawItemId: DRAW_ITEM_ID } : null,
    ),
  });
  // 400px canvas ↔ data [0,4]: 100px per data unit.
  const toDataSpace = (px: number, py: number) => ({ x: px / 100, y: py / 100 });

  it("attaches + detaches pointer listeners alongside the EventSurface", () => {
    const t = makeTarget();
    const p = makeProof();
    const a = p.attach(t.target, makeEngine(), toDataSpace);
    // EventSurface adds pointermove/down/up/leave/cancel(+keydown); the proof adds
    // its own move/down/up/leave taps → at least 2 listeners on the shared types.
    expect(t.count("pointermove")).toBeGreaterThanOrEqual(2);
    expect(t.count("pointerup")).toBeGreaterThanOrEqual(2);
    a.detach();
    expect(t.count("pointermove")).toBe(0);
    expect(t.count("pointerup")).toBe(0);
  });

  it("pointermove over a cell → per-instance hover + tooltip", async () => {
    const t = makeTarget();
    const store = new SignalStore();
    const p = makeProof(store);
    p.attach(t.target, makeEngine(), toDataSpace);
    t.dispatch("pointermove", { offsetX: 50, offsetY: 50 }); // data (0.5,0.5) → row 1000
    await flush();
    expect(p.hovered()).toBe(1000);
    expect(p.tooltip()?.rowId).toBe(1000);
    expect((store.getAs("hover", "hover") as HoverState).rowId).toBe(1000);
  });

  it("click (down+up, no drag) → per-instance drill-down selection", async () => {
    const t = makeTarget();
    const store = new SignalStore();
    const p = makeProof(store);
    p.attach(t.target, makeEngine(), toDataSpace);
    t.dispatch("pointerdown", { offsetX: 250, offsetY: 150, button: 0 }); // (2.5,1.5)
    t.dispatch("pointerup", { offsetX: 250, offsetY: 150, button: 0 });
    await flush();
    expect(p.selectedRows()).toEqual([1006]); // cell r1c2 → row 1006
  });

  it("drag → brush → interval predicate filters the instances end-to-end", async () => {
    const t = makeTarget();
    const store = new SignalStore();
    const p = makeProof(store);
    p.attach(t.target, makeEngine(), toDataSpace); // brushMode "x", field "x"
    p.tick(1); // settle enter transitions
    // Drag from px 150 (data x 1.5) to px 350 (data x 3.5) → interval [1.5,3.5].
    t.dispatch("pointerdown", { offsetX: 150, offsetY: 200, button: 0, buttons: 1 });
    t.dispatch("pointermove", { offsetX: 250, offsetY: 200, buttons: 1 });
    t.dispatch("pointermove", { offsetX: 350, offsetY: 200, buttons: 1 });
    t.dispatch("pointerup", { offsetX: 350, offsetY: 200, button: 0 });
    await flush();

    // The REAL EventSurface wrote brush.interval on field "x"; the proof filters on it.
    const iv = store.getAs("brush.interval", "interval");
    expect(iv?.field).toBe("x");
    expect(p.hasActiveBrush()).toBe(true);
    expect(p.isIncluded(1000)).toBe(false); // value 0 outside [1.5,3.5]
    expect(p.isIncluded(1002)).toBe(true); // value 2 inside
    expect(p.isIncluded(1003)).toBe(true); // value 3 inside
    // A drag does NOT leave a stray click-selection.
    expect(p.selectedRows()).toEqual([]);
  });

  it("the drawItem-level EventSurface signals stay namespaced (no clobber)", async () => {
    const t = makeTarget();
    const store = new SignalStore();
    const p = makeProof(store);
    p.attach(t.target, makeEngine(), toDataSpace);
    t.dispatch("pointerdown", { offsetX: 50, offsetY: 50, button: 0 });
    t.dispatch("pointerup", { offsetX: 50, offsetY: 50, button: 0 });
    await flush();
    // Proof's per-instance selection has the rowId; EventSurface's sibling signal
    // carries the drawItemId — they don't fight over "selection".
    expect((store.getAs("selection", "multi") as MultiSelection).rows).toEqual([1000]);
    const drawItemSel = store.getAs("selection.drawitem", "multi");
    expect(drawItemSel?.rows).toEqual([DRAW_ITEM_ID]);
  });
});
