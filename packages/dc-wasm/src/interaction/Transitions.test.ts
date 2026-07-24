/* ENC-640 (G1): the TS transition stack (FrameClock + AnimationController mirror)
 * must produce smooth enter/exit/update keyed by durable row id — object constancy.
 * These drive it with a fixed dt step (the C++ test style) and pin the [0,1]
 * progress, the enter→stable→drop lifecycle, and the idle/animating signal.
 */

import { describe, it, expect } from "vitest";
import { TransitionController, ease } from "./Transitions";

describe("ease (dc::EasingType mirror)", () => {
  it("clamps the endpoints and is monotone for linear", () => {
    expect(ease("linear", -1)).toBe(0);
    expect(ease("linear", 2)).toBe(1);
    expect(ease("linear", 0.5)).toBeCloseTo(0.5, 6);
  });
  it("easeOutCubic starts fast, easeInQuad starts slow", () => {
    expect(ease("easeOutCubic", 0.25)).toBeGreaterThan(0.25);
    expect(ease("easeInQuad", 0.25)).toBeLessThan(0.25);
  });
  it("easeOutBack overshoots above 1 mid-way", () => {
    // Back easing is defined to overshoot; ensure it exceeds a linear ramp somewhere.
    const overshoots = [0.6, 0.7, 0.8].some((t) => ease("easeOutBack", t) > 1);
    expect(overshoots).toBe(true);
  });
});

describe("TransitionController — enter/exit/update (ENC-640)", () => {
  it("untracked rows read progress 1 (fully present, no transition)", () => {
    const tc = new TransitionController();
    expect(tc.progressOf(999)).toBe(1);
    expect(tc.isTracked(999)).toBe(false);
  });

  it("ENTER tweens 0→1 over the enter duration then settles STABLE", () => {
    const tc = new TransitionController({ enterSeconds: 1, enterEasing: "linear" });
    tc.syncRows([1, 2]);
    // Freshly entered → starts near 0 and is animating.
    expect(tc.progressOf(1)).toBe(0);
    expect(tc.isAnimating()).toBe(true);
    expect(tc.phaseOf(1)).toBe("enter");

    let animating = tc.tick(0.5);
    expect(animating).toBe(true);
    expect(tc.progressOf(1)).toBeCloseTo(0.5, 5);
    expect(tc.progressOf(2)).toBeCloseTo(0.5, 5);

    animating = tc.tick(0.5);
    expect(tc.progressOf(1)).toBeCloseTo(1, 5);
    expect(tc.phaseOf(1)).toBe("stable");
    // Both settled → nothing left animating → host can idle.
    expect(animating).toBe(false);
    expect(tc.isAnimating()).toBe(false);
  });

  it("EXIT tweens →0 then DROPS the row (object constancy released)", () => {
    const tc = new TransitionController({
      enterSeconds: 0,
      exitSeconds: 1,
      exitEasing: "linear",
    });
    tc.syncRows([1, 2]);
    tc.tick(0); // zero-duration enter settles immediately
    expect(tc.progressOf(1)).toBe(1);
    expect(tc.isAnimating()).toBe(false);

    // Row 2 leaves the live set → EXIT.
    tc.syncRows([1]);
    expect(tc.phaseOf(2)).toBe("exit");
    expect(tc.isAnimating()).toBe(true);
    // Still tracked + rendered while it fades out.
    expect(tc.trackedRows().sort()).toEqual([1, 2]);

    tc.tick(0.5);
    expect(tc.progressOf(2)).toBeCloseTo(0.5, 5);
    tc.tick(0.5);
    // Fully exited → dropped; progressOf falls back to the untracked default (1).
    expect(tc.isTracked(2)).toBe(false);
    expect(tc.trackedRows()).toEqual([1]);
    expect(tc.isAnimating()).toBe(false);
  });

  it("re-entering a mid-exit row REVERSES it from its current progress (no popping)", () => {
    const tc = new TransitionController({
      enterSeconds: 1,
      exitSeconds: 1,
      enterEasing: "linear",
      exitEasing: "linear",
    });
    tc.syncRows([5]);
    tc.tick(1); // 5 fully entered (progress 1, stable)
    expect(tc.progressOf(5)).toBeCloseTo(1, 5);

    tc.syncRows([]); // 5 starts exiting
    tc.tick(0.5); // progress ~0.5, phase exit
    expect(tc.progressOf(5)).toBeCloseTo(0.5, 5);
    expect(tc.phaseOf(5)).toBe("exit");

    tc.syncRows([5]); // comes back → re-enter from 0.5, not a jump to 0
    expect(tc.phaseOf(5)).toBe("enter");
    expect(tc.progressOf(5)).toBeCloseTo(0.5, 5);
    tc.tick(0.5); // 0.5 → 1 over 1s ⇒ +0.25
    expect(tc.progressOf(5)).toBeCloseTo(0.75, 5);
  });

  it("syncRows is idempotent for an unchanged set", () => {
    const tc = new TransitionController({ enterSeconds: 1 });
    tc.syncRows([1, 2, 3]);
    tc.tick(0.4);
    const p = tc.progressOf(2);
    tc.syncRows([1, 2, 3]); // same set again
    expect(tc.progressOf(2)).toBe(p); // no restart
    expect(tc.activeCount()).toBe(3);
  });

  it("ignores negative row ids (no row-id threading)", () => {
    const tc = new TransitionController();
    tc.syncRows([-1, 4]);
    expect(tc.isTracked(-1)).toBe(false);
    expect(tc.isTracked(4)).toBe(true);
  });
});
