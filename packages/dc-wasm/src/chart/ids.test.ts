import { describe, it, expect } from "vitest";
import { createIdAllocator, type ResourceKind } from "./ids";

const ALL_KINDS: ResourceKind[] = [
  "pane",
  "layer",
  "drawItem",
  "buffer",
  "geometry",
  "transform",
];

describe("createIdAllocator", () => {
  it("starts at 1 by default", () => {
    const a = createIdAllocator();
    expect(a.peek()).toBe(1);
    expect(a.next()).toBe(1);
    expect(a.next()).toBe(2);
  });

  it("hands out strictly increasing ids", () => {
    const a = createIdAllocator();
    const ids = Array.from({ length: 100 }, () => a.next());
    for (let i = 1; i < ids.length; i++) {
      expect(ids[i]).toBe(ids[i - 1] + 1);
    }
  });

  it("never repeats an id within a session", () => {
    const a = createIdAllocator();
    const seen = new Set<number>();
    for (let i = 0; i < 1000; i++) {
      const id = a.next();
      expect(seen.has(id)).toBe(false);
      seen.add(id);
    }
    expect(seen.size).toBe(1000);
  });

  it("guarantees no collisions across all 6 resource kinds (unified namespace)", () => {
    const a = createIdAllocator();
    const seen = new Set<number>();
    // Interleave allocations across every kind; they must all be distinct.
    for (let round = 0; round < 200; round++) {
      for (const kind of ALL_KINDS) {
        const id = a.nextFor(kind);
        expect(seen.has(id)).toBe(false);
        seen.add(id);
      }
    }
    expect(seen.size).toBe(200 * ALL_KINDS.length);
  });

  it("nextFor draws from the same sequence as next regardless of kind", () => {
    const a = createIdAllocator();
    expect(a.nextFor("pane")).toBe(1);
    expect(a.next()).toBe(2);
    expect(a.nextFor("buffer")).toBe(3);
    expect(a.nextFor("transform")).toBe(4);
    expect(a.next()).toBe(5);
  });

  it("peek reports the next id without consuming it", () => {
    const a = createIdAllocator();
    expect(a.peek()).toBe(1);
    expect(a.peek()).toBe(1);
    expect(a.next()).toBe(1);
    expect(a.peek()).toBe(2);
    a.nextFor("layer");
    expect(a.peek()).toBe(3);
  });

  it("never hands out 0 (reserved sentinel)", () => {
    const a = createIdAllocator();
    for (let i = 0; i < 50; i++) {
      expect(a.next()).toBeGreaterThan(0);
    }
  });

  it("reset returns the counter to its start", () => {
    const a = createIdAllocator();
    a.next();
    a.next();
    a.next();
    expect(a.peek()).toBe(4);
    a.reset();
    expect(a.peek()).toBe(1);
    expect(a.next()).toBe(1);
  });

  it("supports a custom start id", () => {
    const a = createIdAllocator(1000);
    expect(a.peek()).toBe(1000);
    expect(a.next()).toBe(1000);
    expect(a.next()).toBe(1001);
    a.reset();
    expect(a.next()).toBe(1000);
  });

  it("rejects invalid start ids", () => {
    expect(() => createIdAllocator(0)).toThrow(RangeError);
    expect(() => createIdAllocator(-5)).toThrow(RangeError);
    expect(() => createIdAllocator(1.5)).toThrow(RangeError);
    expect(() => createIdAllocator(NaN)).toThrow(RangeError);
  });

  it("independent allocators do not share state", () => {
    const a = createIdAllocator();
    const b = createIdAllocator();
    a.next();
    a.next();
    expect(a.peek()).toBe(3);
    expect(b.peek()).toBe(1);
    expect(b.next()).toBe(1);
  });

  it("throws rather than overflow past MAX_SAFE_INTEGER", () => {
    const a = createIdAllocator(Number.MAX_SAFE_INTEGER);
    expect(a.next()).toBe(Number.MAX_SAFE_INTEGER);
    expect(() => a.next()).toThrow(RangeError);
  });
});
