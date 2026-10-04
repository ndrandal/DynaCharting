/* ENC-1403 — the C++ x-axis labels are judged by THE predicate, not by a copy of it.
 *
 * ENC-1391 made the C++ emitter (`chooseTimeFormat` / `formatTimestamp`) agree with D1's
 * tier-1 rule, and asserted it from C++ by TRANSCRIBING `parsesAsTimestamp` — five regexes
 * plus `validDate`/`validClock`, copied by hand into
 * `core/tests/dc_enc1391_time_grammar.cpp`. A transcription-based test asserts that the
 * transcription is self-consistent; it never asserts that the real predicate holds, and it
 * goes stale in silence. That copy disagreed with this module on 18 of a 40-label probe, in
 * four classes — bare `2024` (H1), bare `14:32` (H2), the expanded year form `+010000`
 * (rejected by the copy, accepted here), and days-in-month/leap (`2026-02-30` accepted by
 * the copy). Three arrived when ENC-1390 merged two seconds after ENC-1391; the fourth never
 * agreed at all. The C++ test stayed green through every one of them.
 *
 * So the predicate now lives in exactly one place and is EXECUTED here. The split:
 *
 *   * the C++ test is the EMITTER half — it claims only that the real pipeline emits exactly
 *     the (rung, fmt, instant, label) rows of a golden corpus, and knows nothing about what
 *     parses.
 *   * THIS test is the PREDICATE half — it reads that corpus out of the C++ source as text
 *     (the idiom `theme.test.ts` already uses for the C++/TS theme mirror, since neither the
 *     emitter nor the tick generator is bound into the WASM module) and runs the real
 *     `parsesAsTimestamp` on every row.
 *
 * Change the emitter and the C++ half goes red on the golden. Change the predicate and THIS
 * half re-judges the real labels. Neither can drift quietly, which is the whole fix.
 *
 * ── THE ASYMMETRY IS THE SUBSTANCE ──────────────────────────────────────────────────────
 *
 * The emitter formats a VALUE; it does not carry the tick, so a label alone cannot settle the
 * two ambiguous shapes. 18 of the 40 corpus rows are `ambiguous`: every `%Y` row (a bare
 * 4-digit string is also `formatTick(v,'index')` past 999 records) and every `%H:%M` row (a
 * bare `HH:MM` is also an elapsed `m:ss`). Those are REJECTED without evidence and ACCEPTED
 * with the instant they were rendered from — the round-trip proof. Demanding
 * `instant`-from-the-string-alone at every rung would fail rungs that are correct; dropping
 * the instant would mean accepting bare `2024` blind, which is hole H1 restored. Both
 * directions are asserted below, per row, derived from the row's own format string.
 */

import { describe, it, expect } from "vitest";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";
import { classifyTimestampLabel, parsesAsTimestamp, type TimestampLabelKind } from "./time";

const HERE = dirname(fileURLToPath(import.meta.url));
// src/chart → repo root is four levels up (same derivation as theme.test.ts / axis.test.ts).
const REPO_ROOT = resolve(HERE, "../../../..");
const CPP_TEST = resolve(REPO_ROOT, "core/tests/dc_enc1391_time_grammar.cpp");

/**
 * What the authority must say about a label from each rung, FROM THE STRING ALONE.
 *
 * Keyed by the rung's strftime format, which is a column of the corpus — so the expectation
 * is derived from the data rather than from a parallel hand-list that could drift the same
 * way the transcription did. An unmapped format is a hard failure, not a skip: a new rung
 * must be ruled on here before its labels count as judged.
 */
const EXPECTED_KIND: Record<string, TimestampLabelKind> = {
  "%Y": "ambiguous", //      bare 4 digits ↔ a record index (ENC-1390 H1)
  "%H:%M": "ambiguous", //   bare HH:MM    ↔ elapsed m:ss   (ENC-1390 H2)
  "%H:%M:%S": "instant", //  three clock fields; the duration formatter emits two
  "%Y-%m": "instant",
  "%Y-%m-%d": "instant",
};

interface CorpusRow {
  rung: string;
  fmt: string;
  instantMs: number;
  label: string;
}

/** Parse the golden corpus out of the C++ test source. */
function readCorpus(): CorpusRow[] {
  const src = readFileSync(CPP_TEST, "utf8");
  const block = /ENC-1403 GOLDEN CORPUS BEGIN ---([\s\S]*?)--- ENC-1403 GOLDEN CORPUS END/.exec(src);
  if (!block) {
    throw new Error(
      `${CPP_TEST}: no "ENC-1403 GOLDEN CORPUS" block. The C++ emitter half is the source of ` +
        `these labels; if it moved, this test is judging nothing and must fail rather than pass.`,
    );
  }
  const rows: CorpusRow[] = [];
  for (const m of block[1].matchAll(/"([^"]*)"/g)) {
    const parts = m[1].split("|");
    if (parts.length !== 4) throw new Error(`malformed corpus row: ${m[1]}`);
    const seconds = Number(parts[2]);
    if (!Number.isInteger(seconds)) throw new Error(`corpus row has no integer instant: ${m[1]}`);
    rows.push({ rung: parts[0], fmt: parts[1], instantMs: seconds * 1000, label: parts[3] });
  }
  return rows;
}

const CORPUS = readCorpus();

describe("ENC-1403 — the C++ emitter's labels, judged by the real parsesAsTimestamp", () => {
  it("the corpus was actually found and is not empty", () => {
    // A corpus that silently came back empty would make every row-wise expectation below
    // vacuously true — the failure mode of an instrument that reports instead of gating.
    expect(CORPUS.length).toBeGreaterThan(0);
    expect(CORPUS.length).toBe(40);
  });

  it("every rung in the corpus has a ruling here", () => {
    const unmapped = [...new Set(CORPUS.map((r) => r.fmt))].filter((f) => !(f in EXPECTED_KIND));
    expect(unmapped, `rungs with no EXPECTED_KIND entry: ${unmapped.join(", ")}`).toEqual([]);
  });

  it("covers all five rungs the emitter can select", () => {
    expect([...new Set(CORPUS.map((r) => r.fmt))].sort()).toEqual(
      Object.keys(EXPECTED_KIND).sort(),
    );
  });

  it("classifies every emitted label as its rung requires", () => {
    const wrong = CORPUS.filter((r) => classifyTimestampLabel(r.label) !== EXPECTED_KIND[r.fmt]).map(
      (r) => `${r.rung} ${r.fmt} '${r.label}': ${classifyTimestampLabel(r.label)} != ${EXPECTED_KIND[r.fmt]}`,
    );
    expect(wrong).toEqual([]);
  });

  it("accepts EVERY emitted label when the tick's instant is supplied — D1 tier 1", () => {
    const rejected = CORPUS.filter(
      (r) => !parsesAsTimestamp(r.label, { instantMs: r.instantMs, zone: "utc" }),
    ).map((r) => `${r.rung} ${r.fmt} '${r.label}' @ ${r.instantMs}`);
    expect(rejected).toEqual([]);
  });

  it("REJECTS exactly the ambiguous rungs when no instant is supplied — hole H1/H2, closed", () => {
    // This is the row the deleted transcription got wrong: it returned true for all of these.
    const bare = CORPUS.map((r) => ({ r, ok: parsesAsTimestamp(r.label) }));
    for (const { r, ok } of bare) {
      expect(ok, `'${r.label}' (${r.fmt}) with no evidence`).toBe(EXPECTED_KIND[r.fmt] === "instant");
    }
    // …and the split is real in both directions, so neither branch is vacuous.
    const ambiguous = bare.filter(({ r }) => EXPECTED_KIND[r.fmt] === "ambiguous");
    const instant = bare.filter(({ r }) => EXPECTED_KIND[r.fmt] === "instant");
    expect(ambiguous.length).toBe(18);
    expect(instant.length).toBe(22);
    expect(ambiguous.length + instant.length).toBe(CORPUS.length);
  });

  it("the shapes the transcription got wrong are actually in the corpus", () => {
    // A floor-not-ceiling guard: if a future ladder change stopped emitting bare years or
    // bare HH:MM, the two assertions above would pass while testing nothing about H1/H2.
    expect(CORPUS.filter((r) => /^\d{4}$/.test(r.label)).length).toBeGreaterThan(0);
    expect(CORPUS.filter((r) => /^\d{2}:\d{2}$/.test(r.label)).length).toBeGreaterThan(0);
  });

  it("the pre-ENC-1391 output still fails, even WITH the right instant", () => {
    // `%b %d` / `%b %Y` named no year and no instant; evidence cannot rescue them, because
    // they are not a rendering of their own tick in any style. Stronger than the old
    // transcription's negative control, which only showed the COPY rejected them.
    const novFifteen = 1700006400000; // the instant the >1-day rung's first tick sits at
    expect(parsesAsTimestamp("Nov 15", { instantMs: novFifteen, zone: "utc" })).toBe(false);
    expect(parsesAsTimestamp("Nov 2023", { instantMs: novFifteen, zone: "utc" })).toBe(false);
    expect(classifyTimestampLabel("Nov 15")).toBe("not-a-timestamp");
  });
});

describe("ENC-1403 — the four classes the C++ transcription disagreed on", () => {
  // Each row is a label the deleted copy judged differently from this module. Pinning the
  // AUTHORITY's verdict here is what makes "the copy was wrong" a measurement rather than a
  // claim in a comment, and it is where a future regression of any of the four shows up.
  const cases: Array<[string, boolean, TimestampLabelKind, string]> = [
    ["2024", false, "ambiguous", "H1 — copy said true unconditionally"],
    ["9999", false, "ambiguous", "H1 — the whole 4-digit shape, not a sub-range"],
    ["14:32", false, "ambiguous", "H2 — copy said true via validClock"],
    ["10:00", false, "ambiguous", "H2 — also an elapsed 10m00s"],
    ["+010000", true, "instant", "expanded year — copy's year field was \\d{4} only"],
    ["-000500-01-01", true, "instant", "expanded year in a date — copy rejected"],
    ["2026-02-30", false, "not-a-timestamp", "days-in-month — copy allowed d<=31"],
    ["2023-02-29", false, "not-a-timestamp", "leap year — copy allowed it"],
    ["2026-04-31", false, "not-a-timestamp", "days-in-month — copy allowed it"],
    ["2026-02-30 14:32", false, "not-a-timestamp", "same hole inside the date-time rung"],
  ];
  for (const [label, expected, kind, why] of cases) {
    it(`'${label}' → ${expected} (${why})`, () => {
      expect(classifyTimestampLabel(label)).toBe(kind);
      expect(parsesAsTimestamp(label)).toBe(expected);
    });
  }
});
