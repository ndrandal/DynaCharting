// Shared, dependency-free helpers for the showcase dataset generators.
// Deterministic PRNG + small numeric utilities. No app coupling.

import { writeFileSync } from "node:fs";

/** mulberry32 — tiny deterministic PRNG. Seed -> () => [0,1). */
export function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** Standard-normal sample (Box-Muller) from a uniform rng. */
export function gaussian(rng) {
  let u = 0;
  let v = 0;
  while (u === 0) u = rng();
  while (v === 0) v = rng();
  return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
}

/** Round to a finite f32-representable number (the wire is f32). */
export function f32(x) {
  return Math.fround(x);
}

/** Round to `d` decimal places (and snap to f32). */
export function round(x, d = 4) {
  const p = 10 ** d;
  return f32(Math.round(x * p) / p);
}

/**
 * Build a dataset object from a list of updates.
 * Sorts updates by t (stable), derives meta.fields/streamKeys, fills counts.
 */
export function buildDataset(meta, updates) {
  // Stable sort by t so equal-t updates keep insertion order.
  const indexed = updates.map((u, i) => [u, i]);
  indexed.sort((a, b) => a[0].t - b[0].t || a[1] - b[1]);
  const sorted = indexed.map(([u]) => u);

  const fields = [...new Set(sorted.map((u) => u.field))].sort();
  const streamKeys = [...new Set(sorted.map((u) => u.streamKey))].sort();

  return {
    meta: {
      ...meta,
      streamKeys,
      fields,
      updateCount: sorted.length,
    },
    updates: sorted,
  };
}

/** Write a dataset as pretty-ish JSON (updates one-per-line for diff sanity). */
export function writeDataset(path, dataset) {
  const head = JSON.stringify({ meta: dataset.meta }, null, 2).replace(/\n}\s*$/, "");
  const lines = dataset.updates.map((u) => "    " + JSON.stringify(u));
  const body =
    head + ",\n  \"updates\": [\n" + lines.join(",\n") + "\n  ]\n}\n";
  writeFileSync(path, body);
  return dataset.updates.length;
}
