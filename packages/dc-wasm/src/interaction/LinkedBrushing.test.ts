/* packages/dc-wasm/src/interaction/LinkedBrushing.test.ts — ENC-639 (F2)
 *
 * End-to-end proof of cross-view linking: two REAL EngineHosts share ONE
 * SignalStore, and a brush dragged in view A live-filters the rows of view B.
 *
 * The drive is 100% the production path — `EngineHost.attachInteraction({ store })`
 * on BOTH hosts, then synthetic pointer events on view A's target (the same
 * injection style as EventSurface.test.ts / EngineHost.blit.test.ts, so no DOM,
 * jsdom, or wasm is needed). A brush drag never calls pick(), so the hosts need no
 * live wasm core for this flow — the link rides entirely on the shared store's
 * `subscribeTo` fan-out + `matchesValue` predicate.
 */

import { describe, it, expect, vi } from "vitest";
import { EngineHost } from "../EngineHost";
import { SignalStore, type BrushRect, type IntervalSelection } from "./SignalStore";
import { LinkedBrushingDemo } from "./LinkedBrushingDemo";

/** A fake DOM target: records listeners and dispatches synthetic events. */
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
      getBoundingClientRect: () => ({ left: 0, top: 0, width: 800, height: 600 }),
    } as unknown as EventTarget,
    dispatch(type: string, ev: Record<string, unknown>) {
      for (const fn of listeners.get(type) ?? []) fn(ev as unknown as Event);
    },
    count(type: string) {
      return (listeners.get(type) ?? []).length;
    },
  };
}

interface Row {
  id: number;
  /** the linking field the brush filters on (identity data-space here) */
  x: number;
  label: string;
}

/** View B's dataset: x spread across the pixel range so an identity brush over
 *  [lo,hi] pixels selects rows with x in [lo,hi]. */
const DATA_B: Row[] = [
  { id: 0, x: 50, label: "a" },
  { id: 1, x: 120, label: "b" },
  { id: 2, x: 200, label: "c" },
  { id: 3, x: 260, label: "d" },
  { id: 4, x: 340, label: "e" },
  { id: 5, x: 480, label: "f" },
];

/** Drag a brush on view A's target from px x0 → x1 (crossing the 4px threshold). */
function brush(a: ReturnType<typeof makeTarget>, x0: number, x1: number, live = true) {
  a.dispatch("pointerdown", { offsetX: x0, offsetY: 100, button: 0 });
  if (live) {
    a.dispatch("pointermove", { offsetX: x1, offsetY: 120 });
  }
  a.dispatch("pointerup", { offsetX: x1, offsetY: 120, button: 0 });
}

function makeDemo(store?: SignalStore) {
  const hostA = new EngineHost();
  const hostB = new EngineHost();
  const a = makeTarget();
  const b = makeTarget();
  const demo = new LinkedBrushingDemo<Row>({
    dataB: DATA_B,
    fieldB: (r) => r.x,
    brushField: "x",
    store,
  });
  demo.link(hostA, a.target, hostB, b.target);
  return { demo, hostA, hostB, a, b };
}

describe("LinkedBrushingDemo — brush in view A filters view B (ENC-639)", () => {
  it("both hosts share the ONE SignalStore", () => {
    const shared = new SignalStore();
    const { demo, hostA, hostB } = makeDemo(shared);
    expect(demo.store).toBe(shared);
    expect(hostA.interactionSurface()?.store).toBe(shared);
    expect(hostB.interactionSurface()?.store).toBe(shared);
    // The two surfaces are distinct objects but write/read the same store.
    expect(hostA.interactionSurface()).not.toBe(hostB.interactionSurface());
  });

  it("starts unfiltered (empty interval ⇒ every row visible)", () => {
    const { demo } = makeDemo();
    expect(demo.filteredB()).toEqual(DATA_B);
    expect(demo.brushInterval()).toBeNull();
  });

  it("a brush drag in A live-filters B to rows inside the interval", () => {
    const { demo, a } = makeDemo();

    // Observe filtered sets pushed to a B-side subscriber (initial + each change).
    const seen: number[][] = [];
    demo.onFilterChange((rows) => seen.push(rows.map((r) => r.id)));
    expect(seen[0]).toEqual([0, 1, 2, 3, 4, 5]); // immediate current set

    // Press, then move to 300 — liveBrush writes the interval MID-drag (before up).
    a.dispatch("pointerdown", { offsetX: 100, offsetY: 100, button: 0 });
    a.dispatch("pointermove", { offsetX: 300, offsetY: 120 });

    // Live: B is already filtered to x∈[100,300] → ids 1,2,3.
    expect(demo.filteredB().map((r) => r.id)).toEqual([1, 2, 3]);
    expect(demo.brushInterval()).toEqual({ field: "x", lo: 100, hi: 300 });
    // The shared store carries the brush rect + paired interval.
    const shared = demo.store;
    expect((shared.getAs("brush", "brush") as BrushRect).x1).toBe(300);
    expect(shared.getAs("brush.interval", "interval") as IntervalSelection).toMatchObject({
      field: "x",
      lo: 100,
      hi: 300,
    });

    // Widen the drag live → filter grows to include id 4 (x=340).
    a.dispatch("pointermove", { offsetX: 360, offsetY: 130 });
    expect(demo.filteredB().map((r) => r.id)).toEqual([1, 2, 3, 4]);

    a.dispatch("pointerup", { offsetX: 360, offsetY: 130, button: 0 });
    expect(demo.filteredB().map((r) => r.id)).toEqual([1, 2, 3, 4]);

    // The subscriber saw the live progression: all → [1,2,3] → [1,2,3,4].
    expect(seen).toEqual([
      [0, 1, 2, 3, 4, 5],
      [1, 2, 3],
      [1, 2, 3, 4],
    ]);
  });

  it("Escape in view A clears the interval → B shows every row again", () => {
    const { demo, a } = makeDemo();
    brush(a, 100, 260); // ids 1,2,3
    expect(demo.filteredB().map((r) => r.id)).toEqual([1, 2, 3]);

    a.dispatch("keydown", { key: "Escape", preventDefault: () => {} });
    expect(demo.filteredB()).toEqual(DATA_B);
    expect(demo.brushInterval()).toBeNull();
  });

  it("a fresh brush replaces the previous filter (no stale union)", () => {
    const { demo, a } = makeDemo();
    brush(a, 40, 130); // x∈[40,130] → ids 0,1
    expect(demo.filteredB().map((r) => r.id)).toEqual([0, 1]);
    brush(a, 250, 500); // x∈[250,500] → ids 3,4,5
    expect(demo.filteredB().map((r) => r.id)).toEqual([3, 4, 5]);
  });

  it("respects a toDataSpace inverse (brush pixels → real field values)", () => {
    // View A pixels are 2× the data-space field (e.g. a zoom of 2). The brush must
    // filter B on DATA values, not raw pixels.
    const hostA = new EngineHost();
    const hostB = new EngineHost();
    const a = makeTarget();
    const b = makeTarget();
    const demo = new LinkedBrushingDemo<Row>({
      dataB: DATA_B,
      fieldB: (r) => r.x,
      brushField: "x",
      toDataSpace: (px, py) => ({ x: px / 2, y: py }),
    });
    demo.link(hostA, a.target, hostB, b.target);

    // Drag pixels 200→520 ⇒ data 100→260 ⇒ ids 1,2,3.
    brush(a, 200, 520);
    expect(demo.brushInterval()).toEqual({ field: "x", lo: 100, hi: 260 });
    expect(demo.filteredB().map((r) => r.id)).toEqual([1, 2, 3]);
  });

  it("dispose() detaches both surfaces and stops filtering", () => {
    const { demo, a, b } = makeDemo();
    const cb = vi.fn();
    demo.onFilterChange(cb);
    cb.mockClear();

    demo.dispose();
    expect(a.count("pointermove")).toBe(0);
    expect(b.count("pointermove")).toBe(0);

    // A post-dispose brush drives nothing (listeners gone, surfaces detached).
    brush(a, 100, 300);
    expect(cb).not.toHaveBeenCalled();
  });
});
