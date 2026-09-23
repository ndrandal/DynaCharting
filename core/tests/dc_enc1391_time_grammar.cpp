// ENC-1391 — the C++ x-axis time labels conform to THE timestamp grammar.
//
// D1's tier-1 predicate is "every x tick label parses as a timestamp". ENC-1296 unified the
// two implementations of that sentence onto time.ts's `TIMESTAMP_GRAMMARS` /
// `parsesAsTimestamp` (packages/dc-wasm/src/chart/time.ts, ENC-1254), carried as data in
// specs/2026-09-19-chart-quality-bar/harness/timestamp-grammar.json.
//
// Its adversarial pass then found a THIRD emitter, in C++, reconciled with neither:
// `chooseTimeFormat` (core/include/dc/math/TimeFormat.hpp) emitted `%b %d` / `%b %Y`, whose
// output ("Nov 15") that grammar rejects. ENC-1391 moved this side onto the ISO forms.
//
// This test is the assertion that keeps it there. `parsesTimestamp` below is a C++
// transcription of the grammar — deliberately a TEST-ONLY reader, not a fourth authority:
// the five patterns and the field ranges are copied verbatim from `TIMESTAMP_GRAMMARS` +
// `validDate` + `validClock`, and the point of the test is that the REAL emitter
// (`computeNiceTimeTicks` -> `chooseTimeFormat` -> `formatTimestamp`) satisfies it at every
// rung the tick ladder can actually produce — including the >1-day and >1-month steps that
// were the defect.
//
// The negative controls at the bottom assert this reader is not a rubber stamp: the old
// `%b %d` / `%b %Y` output must still FAIL it, or a regression in TimeFormat.hpp would
// leave every row above green.

#include "dc/math/NiceTimeTicks.hpp"
#include "dc/math/TimeFormat.hpp"

#include <cstdio>
#include <cstdlib>
#include <regex>
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

// ---------------------------------------------------------------------------
// The grammar, transcribed. Source of truth: time.ts TIMESTAMP_GRAMMARS, carried as
// specs/2026-09-19-chart-quality-bar/harness/timestamp-grammar.json "patterns".
// ---------------------------------------------------------------------------
namespace {

int num(const std::ssub_match& m) { return m.matched ? std::atoi(m.str().c_str()) : -1; }

bool validDate(int y, int mo, int d) {
  (void)y;
  return mo >= 1 && mo <= 12 && d >= 1 && d <= 31;
}

bool validClock(int h, int mi, int s, int ms) {
  if (h < 0 || h > 23) return false;
  if (mi < 0 || mi > 59) return false;
  if (s != -1 && (s < 0 || s > 59)) return false;
  if (ms != -1 && (ms < 0 || ms > 999)) return false;
  return true;
}

// Mirrors `parsesAsTimestamp`. Note the bare-year rung returns true unconditionally on the
// TS side too (pinned as hole H1, ENC-1390) — transcribing it faithfully is the point.
bool parsesTimestamp(const std::string& label) {
  static const std::regex reYear(R"(^(\d{4})$)");
  static const std::regex reMonth(R"(^(\d{4})-(\d{2})$)");
  static const std::regex reDate(R"(^(\d{4})-(\d{2})-(\d{2})$)");
  static const std::regex reDateTime(
      R"(^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{3}))?)?$)");
  static const std::regex reTime(R"(^(\d{2}):(\d{2})(?::(\d{2})(?:\.(\d{3}))?)?$)");

  std::smatch m;
  if (std::regex_match(label, m, reYear)) return true;
  if (std::regex_match(label, m, reMonth)) return validDate(num(m[1]), num(m[2]), 1);
  if (std::regex_match(label, m, reDate)) return validDate(num(m[1]), num(m[2]), num(m[3]));
  if (std::regex_match(label, m, reDateTime))
    return validDate(num(m[1]), num(m[2]), num(m[3])) &&
           validClock(num(m[4]), num(m[5]), num(m[6]), num(m[7]));
  if (std::regex_match(label, m, reTime))
    return validClock(num(m[1]), num(m[2]), num(m[3]), num(m[4]));
  return false;
}

// Run the REAL pipeline over one domain and assert every emitted label parses.
void ladderRung(const char* what, float tMin, float tMax, float expectStep) {
  dc::TimeTickSet ts = dc::computeNiceTimeTicks(tMin, tMax, 5);
  const char* fmt = dc::chooseTimeFormat(ts.stepSeconds);

  char msg[256];
  if (expectStep > 0.0f) {
    std::snprintf(msg, sizeof(msg), "%s: step is %.0fs as expected (got %.0fs)", what,
                  static_cast<double>(expectStep), static_cast<double>(ts.stepSeconds));
    check(ts.stepSeconds == expectStep, msg);
  }

  std::snprintf(msg, sizeof(msg), "%s: emitted at least one tick", what);
  check(!ts.values.empty(), msg);

  for (float v : ts.values) {
    std::string label = dc::formatTimestamp(v, fmt, true);
    std::snprintf(msg, sizeof(msg), "%s: step=%.0fs fmt=%s label='%s' parsesAsTimestamp",
                  what, static_cast<double>(ts.stepSeconds), fmt, label.c_str());
    check(parsesTimestamp(label), msg);
  }
}

} // namespace

int main() {
  const float kBase = 1700000000.0f; // 2023-11-14T22:13:20Z

  // ---- the reader is not a rubber stamp -----------------------------------------------
  // Positives copied from timestamp-grammar.json corpus.accept …
  check(parsesTimestamp("2026"), "grammar accepts '2026'");
  check(parsesTimestamp("2026-09"), "grammar accepts '2026-09'");
  check(parsesTimestamp("2026-09-19"), "grammar accepts '2026-09-19'");
  check(parsesTimestamp("2026-09-19 14:32"), "grammar accepts '2026-09-19 14:32'");
  check(parsesTimestamp("14:32:05.250"), "grammar accepts '14:32:05.250'");
  // … and negatives from corpus.reject, including the elapsed-duration control D1 exists to
  // fail and the two forms THIS TICKET removed.
  check(!parsesTimestamp("0:12"), "grammar rejects '0:12' (elapsed-duration control)");
  check(!parsesTimestamp("3pm"), "grammar rejects '3pm'");
  check(!parsesTimestamp("2026-13-01"), "grammar rejects '2026-13-01' (month range)");
  check(!parsesTimestamp("24:00"), "grammar rejects '24:00' (hour range)");
  check(!parsesTimestamp("Nov 15"), "grammar rejects 'Nov 15' — the OLD %b %d output");
  check(!parsesTimestamp("Nov 2023"), "grammar rejects 'Nov 2023' — the OLD %b %Y output");

  // ---- the ladder chooseTimeFormat actually selects ------------------------------------
  check(std::string(dc::chooseTimeFormat(30.0f)) == "%H:%M:%S", "rung <60s -> %H:%M:%S");
  check(std::string(dc::chooseTimeFormat(300.0f)) == "%H:%M", "rung <1h -> %H:%M");
  check(std::string(dc::chooseTimeFormat(21600.0f)) == "%H:%M", "rung <1d -> %H:%M");
  check(std::string(dc::chooseTimeFormat(86400.0f)) == "%Y-%m-%d",
        "rung >=1d -> %Y-%m-%d (was %b %d before ENC-1391)");
  check(std::string(dc::chooseTimeFormat(2592000.0f)) == "%Y-%m",
        "rung >=30d -> %Y-%m (was %b %Y before ENC-1391)");
  check(std::string(dc::chooseTimeFormat(31536000.0f)) == "%Y", "rung >=365d -> %Y");

  // Every rung formats an instant that parses — direct, independent of the tick ladder.
  const char* rungs[] = {"%H:%M:%S", "%H:%M", "%Y-%m-%d", "%Y-%m", "%Y"};
  for (const char* fmt : rungs) {
    std::string label = dc::formatTimestamp(kBase, fmt, true);
    char msg[160];
    std::snprintf(msg, sizeof(msg), "formatTimestamp(fmt='%s') -> '%s' parsesAsTimestamp",
                  fmt, label.c_str());
    check(parsesTimestamp(label), msg);
  }

  // ---- the acceptance criterion: a tick at a >1-day and a >1-month step ---------------
  // Domains chosen so computeNiceTimeTicks lands on the rung named; the expected step is
  // asserted, so a change to the interval table cannot make these rows vacuous.
  ladderRung(">1-day step (4.25d span, live_server's own data)", kBase - 3600.0f,
             kBase + 100.0f * 3600.0f + 3600.0f, 86400.0f);
  ladderRung(">1-day step (1 week span)", kBase, kBase + 604800.0f, 172800.0f);
  ladderRung(">1-day step (30d span, weekly ticks)", kBase, kBase + 2592000.0f, 604800.0f);
  ladderRung(">1-month step (6 month span)", kBase, kBase + 15552000.0f, 7776000.0f);
  ladderRung(">1-month step (1 year span)", kBase, kBase + 31536000.0f, 7776000.0f);
  ladderRung(">1-year step (5 year span)", kBase, kBase + 157680000.0f, 31536000.0f);

  // …and the sub-day rungs the defect never touched, so the whole ladder is covered.
  ladderRung("sub-day (1 hour span)", kBase, kBase + 3600.0f, 900.0f);
  ladderRung("sub-day (6 hour span)", kBase, kBase + 21600.0f, 7200.0f);
  ladderRung("sub-day (24 hour span)", kBase, kBase + 86400.0f, 21600.0f);

  // Degenerate domain (tMax <= tMin) takes computeNiceTimeTicks' early-out at step=1s.
  ladderRung("degenerate domain (zero span)", kBase, kBase, 1.0f);

  std::printf("ENC-1391 time grammar: %d/%d PASS\n", passed, tests);
  return 0;
}
