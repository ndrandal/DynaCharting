/* packages/authoring-kit/src/math.ts — ENC-714
 *
 * Small, pure math utilities that make up part of the authoring vocabulary but
 * are NOT already provided by dc-wasm's scale.ts. (Scale/tick/transform/grid
 * math lives in @repo/dc-wasm/chart and is re-exported from this package's
 * index — it is deliberately NOT reimplemented here.)
 *
 * Ported from the authoring-corpus runner (specs/2026-06-21-dynacharting-
 * authoring-corpus/runner/authoring-kit.js). Framework-agnostic: no DOM, no
 * engine, no globals.
 */

/** Linear interpolation between `a` and `b` at parameter `t` (unclamped). */
export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

/** Clamp `v` into the closed interval [`lo`, `hi`]. */
export const clamp = (v: number, lo: number, hi: number): number =>
  Math.max(lo, Math.min(hi, v));

/**
 * Min/max of a collection under an accessor, skipping `null`/`undefined`/`NaN`
 * values (matches the corpus `extent`). Returns `[Infinity, -Infinity]` for an
 * empty (or all-skipped) input — the same degenerate sentinel the source emits,
 * which callers typically pad or guard before use.
 */
export function extent<T = number>(
  arr: Iterable<T>,
  acc: (d: T) => number = (d) => d as unknown as number,
): [number, number] {
  let lo = Infinity;
  let hi = -Infinity;
  for (const d of arr) {
    const v = acc(d);
    if (v == null || Number.isNaN(v)) continue;
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  return [lo, hi];
}

/**
 * A small, fast, seeded PRNG (mulberry32-style) for reproducible demo/synthetic
 * data. Returns a function producing floats in [0, 1). Deterministic for a given
 * integer `seed` — identical to the corpus `rng`.
 */
export function rng(seed: number): () => number {
  let a = seed | 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
