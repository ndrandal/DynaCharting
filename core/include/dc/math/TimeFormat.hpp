#pragma once
#include <ctime>
#include <string>

namespace dc {

// Choose an appropriate strftime format string based on tick interval.
inline const char* chooseTimeFormat(float stepSeconds) {
  if (stepSeconds < 60)       return "%H:%M:%S";   // 14:30:15
  if (stepSeconds < 3600)     return "%H:%M";       // 14:30
  if (stepSeconds < 86400)    return "%H:%M";       // 14:00
  if (stepSeconds < 2592000)  return "%b %d";       // Jan 15
  if (stepSeconds < 31536000) return "%b %Y";       // Jan 2024
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
