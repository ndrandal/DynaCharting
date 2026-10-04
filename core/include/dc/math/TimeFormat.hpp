#pragma once
#include <ctime>
#include <string>

namespace dc {

// Choose an appropriate strftime format string based on tick interval.
//
// ENC-1391 — THE LABEL GRAMMAR. This is one of DynaCharting's two x-axis time-label
// emitters; the other is `formatTimeTick` in packages/dc-wasm/src/chart/time.ts, whose
// grammar (`TIMESTAMP_GRAMMARS` / `parsesAsTimestamp`, ENC-1254) was adopted as THE rule
// by ENC-1296 and is carried as data in
// specs/2026-09-19-chart-quality-bar/harness/timestamp-grammar.json. The rungs below are
// the ISO forms `formatTimeTick` already emits, so the two emitters now agree label for
// label, and `core/tests/dc_enc1391_time_grammar.cpp` asserts it over the whole ladder:
//
//   stepSeconds          this ladder    time.ts TimeLabelStyle    example
//   ------------------   ------------   ----------------------    ------------
//   < 60                 %H:%M:%S       "time-second"             14:30:15
//   < 3600               %H:%M          "time-minute"             14:30
//   < 86400              %H:%M          "time-minute"             14:00
//   < 2592000   (day)    %Y-%m-%d       "date"                    2023-11-15
//   < 31536000  (month)  %Y-%m          "month"                   2023-11
//   >=          (year)   %Y             "year"                    2024
//
// Until ENC-1391 the day and month rungs emitted the month-name forms `%b %d` ("Nov 15")
// and `%b %Y` ("Nov 2023"), which that grammar REJECTS. This side moved rather than the
// grammar widening to admit them, because "Nov 15" names no year and so is not an instant:
// no grammar can parse it back to a timestamp, and `parsesAsTimestamp` is exactly the
// predicate the chart-quality bar's tier-1 check asserts. Widening would also have
// re-admitted the `Mon 09` shape ENC-1296 removed from the superseded `_TIME_PATTERNS`,
// which is strictly weaker as a gate.
//
// KNOWN DIVERGENCE (not closed here): `timeLabelStyle` promotes a sub-day step to
// "date-time" (`2026-09-19 14:32`) when the DOMAIN crosses a calendar day, so two ticks a
// day apart cannot both read `09:30`. This function sees only the step, never the domain,
// so it cannot make that call. Both forms parse, so it is an ambiguity rather than a
// grammar break — see LIMITATIONS.md DC-L-1391.
inline const char* chooseTimeFormat(float stepSeconds) {
  if (stepSeconds < 60)       return "%H:%M:%S";   // 14:30:15
  if (stepSeconds < 3600)     return "%H:%M";       // 14:30
  if (stepSeconds < 86400)    return "%H:%M";       // 14:00
  if (stepSeconds < 2592000)  return "%Y-%m-%d";    // 2023-11-15
  if (stepSeconds < 31536000) return "%Y-%m";       // 2023-11
  return "%Y";                                       // 2024
}

// Cross-platform timegm (struct tm → epoch seconds as UTC).
inline std::time_t portableTimegm(std::tm* tm) {
#ifdef _WIN32
  return _mkgmtime(tm);
#else
  return timegm(tm);
#endif
}

// Format an epoch-seconds timestamp using strftime.
inline std::string formatTimestamp(float epochSeconds, const char* fmt, bool utc = true) {
  auto epoch = static_cast<std::time_t>(epochSeconds);
  std::tm tm;
  if (utc) {
#ifdef _WIN32
    gmtime_s(&tm, &epoch);
#else
    gmtime_r(&epoch, &tm);
#endif
  } else {
#ifdef _WIN32
    localtime_s(&tm, &epoch);
#else
    localtime_r(&epoch, &tm);
#endif
  }
  char buf[64];
  std::strftime(buf, sizeof(buf), fmt, &tm);
  return buf;
}

} // namespace dc
