// ENC-1391 / ENC-1403 — the C++ x-axis time labels, as DATA for the one real predicate.
//
// D1's tier-1 predicate is "every x tick label parses as a timestamp". ENC-1296 unified the
// two implementations of that sentence onto time.ts's `TIMESTAMP_GRAMMARS` /
// `parsesAsTimestamp` (packages/dc-wasm/src/chart/time.ts, ENC-1254). ENC-1391's adversarial
// pass then found a THIRD emitter, in C++: `chooseTimeFormat`
// (core/include/dc/math/TimeFormat.hpp) emitted `%b %d` / `%b %Y`, whose output ("Nov 15")
// that grammar rejects. ENC-1391 moved this side onto the ISO forms.
//
// ── WHY THIS FILE NO LONGER CONTAINS A PREDICATE (ENC-1403) ──────────────────────────────
//
// ENC-1391 kept it there by TRANSCRIBING `parsesAsTimestamp` into C++ — five regexes plus
// `validDate` / `validClock`, copied by hand — and calling the copy a "TEST-ONLY reader, not
// a fourth authority". The reader was test-only; the CLAIM that it mirrored the authority was
// not checkable, and it was false. Measured on the merged tree, over a 40-label probe derived
// from the five grammars' field edges, the transcription disagreed with the real predicate on
// **18 of 40**, in four distinct classes:
//
//   1. bare `2024`  — copy: true unconditionally.  authority: `ambiguous`  (hole H1, ENC-1390)
//   2. bare `14:32` — copy: true if `validClock`.  authority: `ambiguous`  (hole H2, ENC-1390)
//   3. `+010000`, `-000500-01-01` — copy: FALSE (its year field was `\d{4}` only).
//      authority: `instant` — ENC-1390 widened the grammar to accept exactly what
//      `yearField` emits. A divergence in the opposite direction, so "the copy is merely
//      laxer than the authority" was never true either.
//   4. `2026-02-30`, `2023-02-29`, `2026-04-31` — copy: true (`d <= 31`).  authority: false
//      (days-in-month + leap year). This one was stale ON ARRIVAL: the days-in-month table
//      has been in `validDate` since ENC-1254 (#133), the commit that first wrote the
//      predicate — so the copy never agreed with it, on any tree, for a moment.
//
// Classes 1-3 arrived when ENC-1390 (#149) merged two seconds after ENC-1391 (#148). Class 4
// never agreed at all. The copy still PASSED throughout, because a transcription-based test
// asserts that the transcription is self-consistent — never that the real predicate holds.
//
// So the predicate is gone from here and is EXECUTED, not copied, exactly once:
//
//   * THIS test is the EMITTER half. It makes no claim about what parses. It asserts the rung
//     ladder `chooseTimeFormat` selects, and that the real pipeline
//     (`computeNiceTimeTicks` -> `chooseTimeFormat` -> `formatTimestamp`) emits EXACTLY the
//     (rung, fmt, instant, label) rows of the golden corpus below — byte for byte.
//   * `packages/dc-wasm/src/chart/time.cpp-labels.test.ts` is the PREDICATE half. It parses
//     that corpus out of THIS FILE as text (the idiom `theme.test.ts` already uses for the
//     C++/TS theme mirror) and runs the real `parsesAsTimestamp` on every row.
//
// Neither half can go stale without going red: change the emitter and the golden mismatches
// here; change the predicate and the TS half re-judges these labels for real.
//
// ── THE ASYMMETRY THAT IS THE POINT, NOT A DETAIL ────────────────────────────────────────
//
// The corpus carries each label's INSTANT, which is why this split works at all. 18 of the 40
// corpus rows are `ambiguous` under the authority — every `%Y` row (a bare
// 4-digit string is also a record index) and every `%H:%M` row (a bare `HH:MM` is also an
// elapsed `m:ss`). Those are REJECTED by `parsesAsTimestamp(label)` and ACCEPTED by
// `parsesAsTimestamp(label, {instantMs})`, which is the round-trip proof. A test that demanded
// `instant`-from-the-string-alone for every rung would fail on rungs that are correct; a test
// that dropped the instant would have to accept bare `2024` blind, which is hole H1 again.
//
// ── REGENERATING THE GOLDEN ──────────────────────────────────────────────────────────────
//
//   DC_ENC1403_DUMP=1 ./build/core/dc_enc1391_time_grammar
//
// prints the corpus in the exact C++ literal form below. Paste it in, then let the TS half
// judge the new labels. If it rejects them, the emitter regressed — that is the gate working.

#include "dc/math/NiceTimeTicks.hpp"
#include "dc/math/TimeFormat.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int tests = 0;
static int passed = 0;

static void check(bool cond, const char* msg) {
  tests++;
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    std::exit(1);
  }
  passed++;
  std::printf("  OK: %s\n", msg);
}

namespace {

// The instant every direct row is formatted from: 2023-11-14T22:13:20Z.
const float kBase = 1700000000.0f;

// ---------------------------------------------------------------------------
// GOLDEN CORPUS — what the REAL emitter produces. Fields: rung|fmt|instantSeconds|label
// `instantSeconds` is `static_cast<std::time_t>(tickValue)`, i.e. the instant
// `formatTimestamp` actually rendered; the TS half uses it as `instantMs = x * 1000` and
// checks the label round-trips in UTC. Separator is `|` so neither language has to escape.
//
// --- ENC-1403 GOLDEN CORPUS BEGIN ---
const char* const kGolden[] = {
    "direct|%H:%M:%S|1700000000|22:13:20",
    "direct|%H:%M|1700000000|22:13",
    "direct|%Y-%m-%d|1700000000|2023-11-14",
    "direct|%Y-%m|1700000000|2023-11",
    "direct|%Y|1700000000|2023",
    "ladder>1d-4.25d|%Y-%m-%d|1700006400|2023-11-15",
    "ladder>1d-4.25d|%Y-%m-%d|1700092800|2023-11-16",
    "ladder>1d-4.25d|%Y-%m-%d|1700179200|2023-11-17",
    "ladder>1d-4.25d|%Y-%m-%d|1700265600|2023-11-18",
    "ladder>1d-4.25d|%Y-%m-%d|1700352000|2023-11-19",
    "ladder>1d-1wk|%Y-%m-%d|1700092800|2023-11-16",
    "ladder>1d-1wk|%Y-%m-%d|1700265600|2023-11-18",
    "ladder>1d-1wk|%Y-%m-%d|1700438400|2023-11-20",
    "ladder>1d-30d|%Y-%m-%d|1700524800|2023-11-21",
    "ladder>1d-30d|%Y-%m-%d|1701129600|2023-11-28",
    "ladder>1d-30d|%Y-%m-%d|1701734400|2023-12-05",
    "ladder>1d-30d|%Y-%m-%d|1702339200|2023-12-12",
    "ladder>1mo-6mo|%Y-%m|1701388800|2023-12",
    "ladder>1mo-6mo|%Y-%m|1709251200|2024-03",
    "ladder>1mo-1yr|%Y-%m|1701388800|2023-12",
    "ladder>1mo-1yr|%Y-%m|1709251200|2024-03",
    "ladder>1mo-1yr|%Y-%m|1717200000|2024-06",
    "ladder>1mo-1yr|%Y-%m|1725148800|2024-09",
    "ladder>1yr-5yr|%Y|1704067200|2024",
    "ladder>1yr-5yr|%Y|1735689600|2025",
    "ladder>1yr-5yr|%Y|1767225600|2026",
    "ladder>1yr-5yr|%Y|1798761600|2027",
    "ladder>1yr-5yr|%Y|1830297600|2028",
    "ladder-subday-1h|%H:%M|1700000128|22:15",
    "ladder-subday-1h|%H:%M|1700001024|22:30",
    "ladder-subday-1h|%H:%M|1700001920|22:45",
    "ladder-subday-1h|%H:%M|1700002816|23:00",
    "ladder-subday-6h|%H:%M|1700006400|00:00",
    "ladder-subday-6h|%H:%M|1700013568|01:59",
    "ladder-subday-6h|%H:%M|1700020736|03:58",
    "ladder-subday-24h|%H:%M|1700006400|00:00",
    "ladder-subday-24h|%H:%M|1700028032|06:00",
    "ladder-subday-24h|%H:%M|1700049664|12:01",
    "ladder-subday-24h|%H:%M|1700071296|18:01",
    "ladder-degenerate|%H:%M:%S|1700000000|22:13:20",
};
// --- ENC-1403 GOLDEN CORPUS END ---

std::string row(const char* rung, const char* fmt, float instant, const std::string& label) {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "%s|%s|%lld|%s", rung, fmt,
                static_cast<long long>(static_cast<std::time_t>(instant)), label.c_str());
  return buf;
}

// One ladder rung, driven through the REAL pipeline. Appends a row per emitted tick and
// asserts the step, so a change to the interval table cannot make the rung vacuous.
void ladderRung(std::vector<std::string>& out, const char* rung, float tMin, float tMax,
                float expectStep) {
  dc::TimeTickSet ts = dc::computeNiceTimeTicks(tMin, tMax, 5);
  const char* fmt = dc::chooseTimeFormat(ts.stepSeconds);

  char msg[256];
  std::snprintf(msg, sizeof(msg), "%s: step is %.0fs as expected (got %.0fs)", rung,
                static_cast<double>(expectStep), static_cast<double>(ts.stepSeconds));
  check(ts.stepSeconds == expectStep, msg);

  std::snprintf(msg, sizeof(msg), "%s: emitted at least one tick", rung);
  check(!ts.values.empty(), msg);

  for (float v : ts.values) out.push_back(row(rung, fmt, v, dc::formatTimestamp(v, fmt, true)));
}

// Everything the real emitter produces, in golden order.
std::vector<std::string> emitted() {
  std::vector<std::string> out;

  // The five rungs formatted directly from one known instant — independent of the tick
  // ladder, so a tick-generator change cannot silently remove a format from the corpus.
  const char* rungs[] = {"%H:%M:%S", "%H:%M", "%Y-%m-%d", "%Y-%m", "%Y"};
  for (const char* fmt : rungs)
    out.push_back(row("direct", fmt, kBase, dc::formatTimestamp(kBase, fmt, true)));

  // Domains chosen so computeNiceTimeTicks lands on the rung named, >1-day and >1-month
  // included — those two were ENC-1391's defect.
  ladderRung(out, "ladder>1d-4.25d", kBase - 3600.0f, kBase + 100.0f * 3600.0f + 3600.0f,
             86400.0f);
  ladderRung(out, "ladder>1d-1wk", kBase, kBase + 604800.0f, 172800.0f);
  ladderRung(out, "ladder>1d-30d", kBase, kBase + 2592000.0f, 604800.0f);
  ladderRung(out, "ladder>1mo-6mo", kBase, kBase + 15552000.0f, 7776000.0f);
  ladderRung(out, "ladder>1mo-1yr", kBase, kBase + 31536000.0f, 7776000.0f);
  ladderRung(out, "ladder>1yr-5yr", kBase, kBase + 157680000.0f, 31536000.0f);
  // …and the sub-day rungs the defect never touched, so the whole ladder is covered.
  ladderRung(out, "ladder-subday-1h", kBase, kBase + 3600.0f, 900.0f);
  ladderRung(out, "ladder-subday-6h", kBase, kBase + 21600.0f, 7200.0f);
  ladderRung(out, "ladder-subday-24h", kBase, kBase + 86400.0f, 21600.0f);
  // Degenerate domain (tMax <= tMin) takes computeNiceTimeTicks' early-out at step=1s.
  ladderRung(out, "ladder-degenerate", kBase, kBase, 1.0f);

  return out;
}

} // namespace

int main() {
  // ---- the rung ladder chooseTimeFormat actually selects --------------------------------
  check(std::string(dc::chooseTimeFormat(30.0f)) == "%H:%M:%S", "rung <60s -> %H:%M:%S");
  check(std::string(dc::chooseTimeFormat(300.0f)) == "%H:%M", "rung <1h -> %H:%M");
  check(std::string(dc::chooseTimeFormat(21600.0f)) == "%H:%M", "rung <1d -> %H:%M");
  check(std::string(dc::chooseTimeFormat(86400.0f)) == "%Y-%m-%d",
        "rung >=1d -> %Y-%m-%d (was %b %d before ENC-1391)");
  check(std::string(dc::chooseTimeFormat(2592000.0f)) == "%Y-%m",
        "rung >=30d -> %Y-%m (was %b %Y before ENC-1391)");
  check(std::string(dc::chooseTimeFormat(31536000.0f)) == "%Y", "rung >=365d -> %Y");

  // ---- the real emitter's output, against the golden the TS predicate judges ------------
  const std::vector<std::string> actual = emitted();

  if (std::getenv("DC_ENC1403_DUMP") != nullptr) {
    std::printf("\n// --- ENC-1403 GOLDEN CORPUS BEGIN ---\nconst char* const kGolden[] = {\n");
    for (const std::string& r : actual) std::printf("    \"%s\",\n", r.c_str());
    std::printf("};\n// --- ENC-1403 GOLDEN CORPUS END ---\n");
    return 0;
  }

  const std::size_t goldenCount = sizeof(kGolden) / sizeof(kGolden[0]);
  char msg[512];
  std::snprintf(msg, sizeof(msg),
                "emitter produced %zu rows, golden has %zu "
                "(regenerate: DC_ENC1403_DUMP=1 ./build/core/dc_enc1391_time_grammar)",
                actual.size(), goldenCount);
  check(actual.size() == goldenCount, msg);

  for (std::size_t i = 0; i < goldenCount; ++i) {
    std::snprintf(msg, sizeof(msg), "row %zu: emitter '%s' == golden '%s'", i,
                  actual[i].c_str(), kGolden[i]);
    check(actual[i] == kGolden[i], msg);
  }

  // The corpus must carry an instant for every row and cover all five rungs, or the TS half
  // silently judges less than the ladder emits. Derived from the rows, not hand-listed.
  const char* rungFmts[] = {"%H:%M:%S", "%H:%M", "%Y-%m-%d", "%Y-%m", "%Y"};
  for (const char* fmt : rungFmts) {
    const std::string needle = std::string("|") + fmt + "|";
    bool found = false;
    for (const std::string& r : actual)
      if (r.find(needle) != std::string::npos) found = true;
    std::snprintf(msg, sizeof(msg), "corpus covers rung '%s'", fmt);
    check(found, msg);
  }

  std::printf("ENC-1391/ENC-1403 time grammar (emitter half): %d/%d PASS\n", passed, tests);
  std::printf("  the PREDICATE half is packages/dc-wasm/src/chart/time.cpp-labels.test.ts\n");
  return 0;
}
