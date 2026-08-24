/* ENC-634 (D4): the browser event surface must translate DOM pointer/keyboard
 * events into the right pick() calls and SignalStore mutations. These tests drive
 * the surface with SYNTHETIC events + a fake pick engine (no DOM/jsdom, no wasm) —
 * the same injection style as EngineHost.blit.test.ts — and assert the routed
 * signal state, so hover / selection / brush / keyboard behavior is pinned.
 */

import { describe, it, expect, vi } from "vitest";
import { EventSurface } from "./EventSurface";
import { SignalStore, type HoverState, type MultiSelection, type BrushRect } from "./SignalStore";
import { EngineHost } from "../EngineHost";

/** A fake DOM target: records listeners and lets tests dispatch synthetic events. */
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
    },
    dispatch(type: string, ev: Record<string, unknown>) {
      const arr = listeners.get(type) ?? [];
      for (const fn of arr) fn(ev as unknown as Event);
    },
    count(type: string) {
      return (listeners.get(type) ?? []).length;
    },
  };
}

/** A fake pick engine returning a scripted sequence of drawItemIds (null = miss). */
function makeEngine(ids: Array<number | null>) {
  let i = 0;
  const pickAsync = vi.fn(async (_x: number, _y: number) => {
    const id = ids[Math.min(i, ids.length - 1)];
    i++;
    return id === null ? null : { drawItemId: id };
  });
  return { engine: { pickAsync }, pickAsync };
}

/** Await all queued microtasks so async pick .then() handlers have run. */
const flush = () => new Promise<void>((r) => setTimeout(r, 0));

describe("EventSurface — DOM events → signals (ENC-634)", () => {
  it("attaches pointer + keyboard listeners and detaches them", () => {
    const t = makeTarget();
    const { engine } = makeEngine([]);
    const s = new EventSurface(t.target, engine, { store: new SignalStore() });
    expect(t.count("pointermove")).toBe(1);
    expect(t.count("pointerdown")).toBe(1);
    expect(t.count("pointerup")).toBe(1);
    expect(t.count("keydown")).toBe(1);
    s.detach();
    expect(t.count("pointermove")).toBe(0);
    expect(t.count("keydown")).toBe(0);
  });

  it("pointermove → pick → hover signal with enter/exit on change only", async () => {
    const t = makeTarget();
    const { engine, pickAsync } = makeEngine([7, 7, null]);
    const onHoverEnter = vi.fn();
    const onHoverExit = vi.fn();
    const s = new EventSurface(t.target, engine, { onHoverEnter, onHoverExit });

    t.dispatch("pointermove", { offsetX: 10, offsetY: 20 });
    await flush();
    let h = s.store.getAs("hover", "hover") as HoverState;
    expect(h.active).toBe(true);
    expect(h.drawItemId).toBe(7);
    expect(onHoverEnter).toHaveBeenCalledTimes(1);

    // Same id again → NO new enter (HoverManager fires only on change).
    t.dispatch("pointermove", { offsetX: 11, offsetY: 21 });
    await flush();
    expect(onHoverEnter).toHaveBeenCalledTimes(1);

    // Move to empty space → exit.
    t.dispatch("pointermove", { offsetX: 500, offsetY: 500 });
    await flush();
    h = s.store.getAs("hover", "hover") as HoverState;
    expect(h.active).toBe(false);
    expect(onHoverExit).toHaveBeenCalledTimes(1);
    expect(pickAsync).toHaveBeenCalledTimes(3);
  });

  it("pointerleave clears hover", async () => {
    const t = makeTarget();
    const { engine } = makeEngine([3]);
    const s = new EventSurface(t.target, engine);
    t.dispatch("pointermove", { offsetX: 5, offsetY: 5 });
    await flush();
    expect((s.store.getAs("hover", "hover") as HoverState).active).toBe(true);
    t.dispatch("pointerleave", {});
    expect((s.store.getAs("hover", "hover") as HoverState).active).toBe(false);
  });

  it("click (down/up, no drag) selects the picked item — single mode replaces", async () => {
    const t = makeTarget();
    const { engine } = makeEngine([42, 99]);
    const s = new EventSurface(t.target, engine, { selectionMode: "single" });

    t.dispatch("pointerdown", { offsetX: 100, offsetY: 100, button: 0 });
    t.dispatch("pointerup", { offsetX: 100, offsetY: 100, button: 0 });
    await flush();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([42]);

    // A second single-mode click REPLACES.
    t.dispatch("pointerdown", { offsetX: 200, offsetY: 100, button: 0 });
    t.dispatch("pointerup", { offsetX: 200, offsetY: 100, button: 0 });
    await flush();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([99]);
  });

  it("shift/ctrl-click toggles into a union selection", async () => {
    const t = makeTarget();
    const { engine } = makeEngine([1, 2, 1]);
    const s = new EventSurface(t.target, engine, { selectionMode: "single" });

    const click = (x: number, mods: Record<string, unknown> = {}) => {
      t.dispatch("pointerdown", { offsetX: x, offsetY: 50, button: 0, ...mods });
      t.dispatch("pointerup", { offsetX: x, offsetY: 50, button: 0, ...mods });
    };
    click(10, { shiftKey: true }); // id 1
    await flush();
    click(20, { shiftKey: true }); // id 2 → union
    await flush();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([1, 2]);
    click(10, { shiftKey: true }); // id 1 again → toggle OFF
    await flush();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([2]);
  });

  it("drag past threshold → brush signal (x-interval band + paired interval)", async () => {
    const t = makeTarget();
    const { engine, pickAsync } = makeEngine([5]);
    const s = new EventSurface(t.target, engine, { brushMode: "x", brushField: "price" });

    t.dispatch("pointerdown", { offsetX: 100, offsetY: 100, button: 0 });
    // Small move (below 4px threshold) → not yet a drag, no brush write.
    t.dispatch("pointermove", { offsetX: 102, offsetY: 100 });
    expect((s.store.getAs("brush", "brush") as BrushRect)).toEqual({
      kind: "brush",
      x0: 0,
      y0: 0,
      x1: 0,
      y1: 0,
    });
    // Cross the threshold → brush live-updates.
    t.dispatch("pointermove", { offsetX: 150, offsetY: 120 });
    let b = s.store.getAs("brush", "brush") as BrushRect;
    expect(b.x0).toBe(100);
    expect(b.x1).toBe(150);
    t.dispatch("pointerup", { offsetX: 160, offsetY: 130, button: 0 });
    b = s.store.getAs("brush", "brush") as BrushRect;
    expect(b.x1).toBe(160);
    // The paired interval carries the field + [lo,hi].
    const iv = s.store.getAs("brush.interval", "interval");
    expect(iv).toMatchObject({ field: "price", lo: 100, hi: 160 });
    // A drag must NOT pick/select (pointermove during press doesn't pick).
    expect(pickAsync).not.toHaveBeenCalled();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([]);
  });

  it("Escape clears selection + brush; arrows navigate a single selection", async () => {
    const t = makeTarget();
    const { engine } = makeEngine([10]);
    const s = new EventSurface(t.target, engine, { selectionMode: "single" });

    t.dispatch("pointerdown", { offsetX: 5, offsetY: 5, button: 0 });
    t.dispatch("pointerup", { offsetX: 5, offsetY: 5, button: 0 });
    await flush();
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([10]);

    const preventDefault = vi.fn();
    t.dispatch("keydown", { key: "ArrowRight", preventDefault });
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([11]);
    t.dispatch("keydown", { key: "ArrowLeft", preventDefault });
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([10]);
    expect(preventDefault).toHaveBeenCalled();

    t.dispatch("keydown", { key: "Escape", preventDefault });
    expect((s.store.getAs("selection", "multi") as MultiSelection).rows).toEqual([]);
  });

  it("a shared store is driven by two surfaces (linked-brushing basis, ENC-639)", async () => {
    const shared = new SignalStore();
    const a = makeTarget();
    const b = makeTarget();
    const sa = new EventSurface(a.target, makeEngine([1]).engine, { store: shared });
    const sb = new EventSurface(b.target, makeEngine([2]).engine, { store: shared });
    expect(sa.store).toBe(shared);
    expect(sb.store).toBe(shared);

    const seen: Array<string> = [];
    shared.subscribe((id) => seen.push(id));
    // Brush in view A mutates the shared brush signal.
    a.dispatch("pointerdown", { offsetX: 10, offsetY: 10, button: 0 });
    a.dispatch("pointermove", { offsetX: 80, offsetY: 40 });
    expect(seen).toContain("brush");
    expect((shared.getAs("brush", "brush") as BrushRect).x1).toBe(80);
  });
});

describe("EngineHost.attachInteraction (ENC-634)", () => {
  it("wires the surface to a passed target and routes pick → hover", async () => {
    const t = makeTarget();
    const host = new EngineHost();
    // pickAsync needs ready+core+canvas; inject a fake pick core + canvas.
    const h = host as unknown as Record<string, unknown>;
    h.ready = true;
    h.core = { pick: vi.fn(async () => 7) };
    h.canvas = { width: 800, height: 600 };

    const surface = host.attachInteraction(t.target as unknown as EventTarget);
    expect(host.interactionSurface()).toBe(surface);

    t.dispatch("pointermove", { offsetX: 10, offsetY: 20 });
    await flush();
    expect((surface.store.getAs("hover", "hover") as HoverState).drawItemId).toBe(7);

    // detachInteraction removes listeners.
    host.detachInteraction();
    expect(t.count("pointermove")).toBe(0);
    expect(host.interactionSurface()).toBeNull();
  });

  it("throws if no canvas is bound and no target is passed", () => {
    const host = new EngineHost();
    expect(() => host.attachInteraction()).toThrow(/no target/);
  });

  it("attachInteraction twice detaches the previous surface", () => {
    const t1 = makeTarget();
    const t2 = makeTarget();
    const host = new EngineHost();
    host.attachInteraction(t1.target as unknown as EventTarget);
    expect(t1.count("pointermove")).toBe(1);
    host.attachInteraction(t2.target as unknown as EventTarget);
    expect(t1.count("pointermove")).toBe(0);
    expect(t2.count("pointermove")).toBe(1);
  });
});
