/* Verifies the re-exported shared primitives (canonical home: @repo/dc-wasm)
 * are reachable via the authoring-kit surface AND that a Mark's output feeds
 * cleanly into dc-wasm's SceneBuilder — i.e. the package is consumable end to
 * end without reimplementing scale/tick/id/host-sequencing. */
import { describe, it, expect } from "vitest";
import {
  niceTicks,
  createIdAllocator,
  fitTransform,
  SceneBuilder,
  type SceneTarget,
} from "./index";
import { lineMark, candlesMark } from "./marks";

describe("re-exported niceTicks (from @repo/dc-wasm)", () => {
  it("produces round 1/2/5 ticks over a range", () => {
    expect(niceTicks(0, 100, 5)).toEqual([0, 20, 40, 60, 80, 100]);
    expect(niceTicks(0, 1, 5)).toEqual([0, 0.2, 0.4, 0.6, 0.8, 1]);
  });
});

describe("re-exported createIdAllocator (from @repo/dc-wasm)", () => {
  it("hands out strictly increasing ids across kinds and resets", () => {
    const ids = createIdAllocator();
    expect(ids.next()).toBe(1);
    expect(ids.nextFor("buffer")).toBe(2);
    expect(ids.nextFor("geometry")).toBe(3);
    expect(ids.peek()).toBe(4);
    ids.reset();
    expect(ids.next()).toBe(1);
  });
  it("honors a custom start", () => {
    expect(createIdAllocator(1000).next()).toBe(1000);
  });
});

describe("re-exported fitTransform (from @repo/dc-wasm)", () => {
  it("maps a data rect onto NDC with positive sy", () => {
    const t = fitTransform({ x: { min: 0, max: 10 }, y: { min: 0, max: 100 } });
    expect(t.sx).toBeCloseTo(0.2, 6); // 2/10
    expect(t.sy).toBeCloseTo(0.02, 6); // 2/100 (orientation-correct, positive)
  });
});

describe("Mark output feeds dc-wasm SceneBuilder", () => {
  it("addLineSegments consumes a lineMark's floats + style", () => {
    const controls: object[] = [];
    const batches: ArrayBuffer[] = [];
    const target: SceneTarget = {
      applyControl: (cmd) => {
        controls.push(cmd);
        return { ok: true };
      },
      applyDataBatch: (b) => {
        batches.push(b);
      },
    };
    const sb = new SceneBuilder(target);
    const pane = sb.pane();
    const layer = sb.layer(pane);
    const m = lineMark([
      { x: 0, y: 0 },
      { x: 1, y: 1 },
    ]);
    sb.addLineSegments(layer, m.floats, m.style as { r: number; g: number; b: number });
    // create/bind/style commands were emitted and geometry bytes pushed.
    expect(controls.some((c) => (c as { cmd?: string }).cmd === "bindDrawItem")).toBe(true);
    expect(batches.length).toBe(1);
  });

  it("candlesMark style shape matches SceneBuilder CandleStyle", () => {
    const m = candlesMark([{ x: 0, open: 1, high: 2, low: 0, close: 1.5 }]);
    const style = m.style as { colorUp: number[]; colorDown: number[] };
    expect(style.colorUp).toHaveLength(4);
    expect(style.colorDown).toHaveLength(4);
  });
});
