// ENC-1265 — the frame-timing instrument, and the gate that keeps it honest.
//
// THE DEFECT THIS EXISTS TO PREVENT RECURRING. `dc::Stats::frameMs` was
// declared in 2026-05, marshalled across the WASM boundary, re-exposed on the TS
// EngineHost, p95'd over a rolling window, rendered into a HUD badge and burned
// into 22 committed screenshots — and was assigned by NOTHING in core/. It read
// 0.0 on every frame for four months. The showcase HUD, finding a zero, silently
// substituted `1000 / fps`, so the badge printed ONE measurement in TWO places
// under two different unit labels: `1 fps · 1000.7 ms` is `1000/0.9993`, and the
// `.7` is rounding, not information. PERF-CLAIMS C1/C3 struck the whole set.
//
// Nothing caught it because nothing could: a field that is never written has no
// wrong value to assert against, and the only code that would have exercised it
// lives in dc_gpu — inside the 47 render tests LIMITATIONS.md DC-L01 excludes at
// *configure* time. So this file is in the DEFAULT build on purpose, the same
// reason dc_enc1251_body_floor and dc_enc1257_bar_sizing are.
//
// [1]-[4] check the timing primitive (dc/debug/WallClockMs.hpp) against an
// independent clock. [5] is the gate proper: it reads the stats structs out of
// the tree and fails if any field whose name ends in `Ms` is assigned nowhere
// under core/src or core/wasm. That is the shape of the original bug, stated as
// a rule, checked by the build people actually run.
//
// NEGATIVE CONTROLS (ctest WILL_FAIL, the ENC-1249 pattern):
//   --negctl-frozen-clock       feeds [1]-[4] a clock that never advances.
//   --negctl-unassigned-field   feeds [5] a field name that is assigned nowhere.
// A check never seen to fail is not a check. Both are registered in
// core/CMakeLists.txt, so every run of the suite re-demonstrates falsifiability
// rather than trusting a paragraph in a PR description.

#include "dc/debug/WallClockMs.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifndef DC_CORE_DIR
#error "DC_CORE_DIR must be defined (see core/CMakeLists.txt)"
#endif

namespace {

int g_failed = 0;

void check(bool ok, const char* what, const std::string& detail) {
  std::printf("  %s  %-58s %s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
  if (!ok) ++g_failed;
}

// ---------------------------------------------------------------- source gate

// Strip a trailing `//` comment so prose that merely NAMES a field (this file is
// full of it, and so is Stats.hpp) can never be mistaken for an assignment.
std::string stripLineComment(const std::string& line) {
  const std::size_t i = line.find("//");
  return i == std::string::npos ? line : line.substr(0, i);
}

// Pull every `double <something>Ms` member out of `struct <name> {` ... `};`.
std::vector<std::string> timingFieldsOf(const std::filesystem::path& file,
                                        const std::string& structName) {
  std::vector<std::string> fields;
  std::ifstream in(file);
  if (!in) return fields;
  const std::regex openRe("^\\s*struct\\s+" + structName + "\\s*\\{");
  const std::regex fieldRe("^\\s*double\\s+([A-Za-z_][A-Za-z0-9_]*Ms)\\b");
  bool inside = false;
  std::string line;
  while (std::getline(in, line)) {
    const std::string code = stripLineComment(line);
    if (!inside) {
      if (std::regex_search(code, openRe)) inside = true;
      continue;
    }
    if (code.rfind("};", 0) == 0) break;  // closing brace at column 0
    std::smatch m;
    if (std::regex_search(code, m, fieldRe)) fields.push_back(m[1].str());
  }
  return fields;
}

// True if any .cpp/.hpp under `root` assigns to `field` — i.e. contains a
// `field =` (or `.field =`, `->field =`) that is not itself the declaration.
bool assignedUnder(const std::filesystem::path& root, const std::string& field) {
  if (!std::filesystem::exists(root)) return false;
  const std::regex assignRe("(^|[^A-Za-z0-9_])" + field +
                            "\\s*=[^=]");            // not `==`
  const std::regex declRe("^\\s*double\\s+" + field + "\\b");
  for (const auto& e : std::filesystem::recursive_directory_iterator(root)) {
    if (!e.is_regular_file()) continue;
    const std::string ext = e.path().extension().string();
    if (ext != ".cpp" && ext != ".hpp" && ext != ".h" && ext != ".cc") continue;
    std::ifstream in(e.path());
    std::string line;
    while (std::getline(in, line)) {
      const std::string code = stripLineComment(line);
      if (std::regex_search(code, declRe)) continue;  // the declaration itself
      if (std::regex_search(code, assignRe)) return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  bool frozenClock = false;
  bool unassignedField = false;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--negctl-frozen-clock") == 0) frozenClock = true;
    if (std::strcmp(argv[i], "--negctl-unassigned-field") == 0)
      unassignedField = true;
  }

  std::printf("=== ENC-1265: render timing is a measurement, not arithmetic ===\n");
  if (frozenClock)
    std::printf("  [negative control] clock frozen at 0.0 — [1]-[4] MUST fail\n");
  if (unassignedField)
    std::printf("  [negative control] injecting an unassigned field — [5] MUST fail\n");

  // ---------------------------------------------------------------- the clock
  //
  // Measured against an INDEPENDENT clock over the same sleep. Asserting only
  // "it is > 0" would pass for a field that returns any plausible constant,
  // which is a cousin of the bug above.
  const dc::WallClockMs clock;
  const auto refStart = std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  const double measured = frozenClock ? 0.0 : clock.elapsedMs();
  const double reference =
      std::chrono::duration<double, std::milli>(
          std::chrono::high_resolution_clock::now() - refStart)
          .count();

  check(measured >= 25.0 && measured < 2000.0,
        "[1] a 25 ms sleep reads as 25 ms or a little more",
        "measured = " + std::to_string(measured) + " ms");

  check(std::abs(measured - reference) < 5.0,
        "[2] it agrees with an independent clock over the same span",
        "measured " + std::to_string(measured) + " vs reference " +
            std::to_string(reference) + " ms");

  // A second read must not go backwards, and must not be frozen either: the
  // clock keeps running while it is read.
  const double again = frozenClock ? 0.0 : clock.elapsedMs();
  check(again >= measured, "[3] elapsedMs is monotonic non-decreasing",
        std::to_string(measured) + " -> " + std::to_string(again) + " ms");

  // restart() rewinds, and an immediate read is small — so the number tracks
  // real elapsed time rather than being accumulated or constant.
  dc::WallClockMs fresh;
  fresh.restart();
  const double idle = frozenClock ? 25.0 : fresh.elapsedMs();
  check(idle >= 0.0 && idle < 5.0,
        "[4] restart() rewinds; an immediate read is near zero",
        "idle = " + std::to_string(idle) + " ms");

  // ------------------------------------------------------------ the source gate
  //
  // Every `*Ms` field of the stats structs must be assigned somewhere in the
  // engine. This is `frameMs`'s whole failure mode, stated as a rule.
  const std::filesystem::path core{DC_CORE_DIR};
  struct Target {
    std::filesystem::path file;
    const char* structName;
  };
  const Target targets[] = {
      {core / "include" / "dc" / "debug" / "Stats.hpp", "Stats"},
      {core / "wasm" / "dc_engine_host.cpp", "DcEngineStats"},
  };

  std::vector<std::string> fields;
  for (const auto& t : targets) {
    const auto found = timingFieldsOf(t.file, t.structName);
    // A struct that suddenly has no timing fields would make this gate vacuous;
    // that is itself a finding, not a pass.
    if (found.empty()) {
      check(false, "[5a] the stats struct still declares a timing field",
            std::string(t.structName) + " in " + t.file.filename().string());
    }
    for (const auto& f : found) fields.push_back(f);
  }
  if (unassignedField) fields.push_back("enc1265NeverAssignedMs");

  std::set<std::string> uniq(fields.begin(), fields.end());
  bool allAssigned = true;
  std::string missing;
  for (const auto& f : uniq) {
    const bool ok = assignedUnder(core / "src", f) ||
                    assignedUnder(core / "wasm", f);
    if (!ok) {
      allAssigned = false;
      missing += (missing.empty() ? "" : ", ") + f;
    }
  }
  check(allAssigned,
        "[5] every *Ms stats field is assigned in core/src or core/wasm",
        allAssigned ? std::to_string(uniq.size()) + " field(s) checked"
                    : "NEVER ASSIGNED: " + missing);

  std::printf("=== %s (%d failure%s) ===\n", g_failed ? "FAILED" : "OK",
              g_failed, g_failed == 1 ? "" : "s");
  return g_failed == 0 ? 0 : 1;
}
