#!/usr/bin/env node
// ENC-521/ENC-522 — dataset validator.
//
// Loads every dataset under market/ and synthetic/ and checks:
//   - meta well-formed (durationMs, fields[], streamKeys[], updateCount)
//   - t monotonic non-decreasing and within [0, durationMs]
//   - every value finite (no NaN/Infinity/null) and f32-representable
//   - per-field value ranges sane (rsi 0..100, prices > 0, sizes >= 0, etc.)
//   - every declared meta.field appears in updates, and vice versa
//   - updateCount matches updates.length
//
//   node apps/showcase/data/validate.mjs    (exit 0 = all pass, 1 = failures)

import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { readFileSync, readdirSync, existsSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url));

function listDatasets() {
  const files = [];
  for (const sub of ["market", "synthetic"]) {
    const dir = join(HERE, sub);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) {
      if (f.endsWith(".json") && f !== "index.json") files.push(join(sub, f));
    }
  }
  return files.sort();
}

const F32_MAX = 3.4028234663852886e38;

// Per-field sanity bounds. Patterns matched in order; first match wins.
const FIELD_RULES = [
  { re: /^rsi$/, min: 0, max: 100 },
  { re: /^(open|high|low|close|lastPrice|vwap|sma_20|ema_12|ema_26|bb_upper|bb_middle|bb_lower)$/, min: 0.001, max: 1e6 },
  { re: /^volume$/, min: 0, max: 1e9 },
  { re: /^(macd|macdSignal|macdHistogram)$/, min: -1e5, max: 1e5 },
  { re: /^depth\.(bid|ask)\.\d+\.size$/, min: 0, max: 1e7 },
  { re: /^field\.r\d+\.c\d+$/, min: 0, max: 1 },
  { re: /^amp$/, min: -5, max: 5 },
];

function ruleFor(field) {
  return FIELD_RULES.find((r) => r.re.test(field));
}

function isF32(x) {
  return typeof x === "number" && Number.isFinite(x) && Math.abs(x) <= F32_MAX;
}

function validate(rel) {
  const errors = [];
  let ds;
  try {
    ds = JSON.parse(readFileSync(join(HERE, rel), "utf8"));
  } catch (e) {
    return { rel, errors: [`parse error: ${e.message}`], updates: 0 };
  }

  const meta = ds.meta;
  const updates = ds.updates;
  if (!meta || typeof meta !== "object") errors.push("missing meta");
  if (!Array.isArray(updates)) {
    errors.push("missing/invalid updates[]");
    return { rel, errors, updates: 0 };
  }

  const durationMs = meta?.durationMs;
  if (!Number.isFinite(durationMs) || durationMs <= 0) errors.push("meta.durationMs invalid");
  if (meta && meta.updateCount !== updates.length) {
    errors.push(`meta.updateCount=${meta.updateCount} != updates.length=${updates.length}`);
  }

  const seenFields = new Set();
  const seenKeys = new Set();
  let lastT = -1;

  for (let i = 0; i < updates.length; i++) {
    const u = updates[i];
    const where = `updates[${i}]`;
    if (typeof u !== "object" || u === null) {
      errors.push(`${where}: not an object`);
      continue;
    }
    // timestamps
    if (!Number.isInteger(u.t)) errors.push(`${where}: t not an integer (${u.t})`);
    else {
      if (u.t < lastT) errors.push(`${where}: t=${u.t} < previous ${lastT} (not monotonic)`);
      if (u.t < 0 || u.t > durationMs) errors.push(`${where}: t=${u.t} out of [0,${durationMs}]`);
      lastT = u.t;
    }
    // identity
    if (typeof u.streamKey !== "string" || !u.streamKey) errors.push(`${where}: bad streamKey`);
    else seenKeys.add(u.streamKey);
    if (typeof u.field !== "string" || !u.field) errors.push(`${where}: bad field`);
    else seenFields.add(u.field);
    // value finiteness + f32
    if (!isF32(u.value)) {
      errors.push(`${where}: value not finite/f32 (${u.value}) field=${u.field}`);
    } else {
      const rule = ruleFor(u.field);
      if (!rule) {
        errors.push(`${where}: no range rule for field "${u.field}"`);
      } else if (u.value < rule.min || u.value > rule.max) {
        errors.push(`${where}: ${u.field}=${u.value} out of [${rule.min},${rule.max}]`);
      }
    }
    if (errors.length > 30) {
      errors.push("... (truncated)");
      break;
    }
  }

  // declared-vs-present field coverage
  if (Array.isArray(meta?.fields)) {
    for (const f of meta.fields) if (!seenFields.has(f)) errors.push(`meta.fields has "${f}" but it never appears in updates`);
    for (const f of seenFields) if (!meta.fields.includes(f)) errors.push(`update field "${f}" missing from meta.fields`);
  } else {
    errors.push("meta.fields not an array");
  }
  if (Array.isArray(meta?.streamKeys)) {
    for (const k of seenKeys) if (!meta.streamKeys.includes(k)) errors.push(`update streamKey "${k}" missing from meta.streamKeys`);
  }

  return { rel, errors, updates: updates.length, fields: seenFields.size };
}

const datasets = listDatasets();
if (datasets.length === 0) {
  console.error("No datasets found. Run the generators first.");
  process.exit(1);
}

let failed = 0;
for (const rel of datasets) {
  const r = validate(rel);
  if (r.errors.length === 0) {
    console.log(`PASS  ${rel}  (${r.updates} updates, ${r.fields} fields)`);
  } else {
    failed++;
    console.log(`FAIL  ${rel}`);
    for (const e of r.errors) console.log(`        - ${e}`);
  }
}

console.log("");
if (failed === 0) {
  console.log(`OK: ${datasets.length} datasets validated.`);
  process.exit(0);
} else {
  console.log(`${failed}/${datasets.length} datasets FAILED.`);
  process.exit(1);
}
