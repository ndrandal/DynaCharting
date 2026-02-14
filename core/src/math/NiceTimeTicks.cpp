#include "dc/math/NiceTimeTicks.hpp"
#include <cmath>
#include <ctime>

namespace dc {

// 24-entry interval table (seconds)
static const float kTimeIntervals[] = {
  1, 2, 5, 10, 15, 30,                         // sub-minute
  60, 120, 300, 600, 900, 1800,                 // minutes
  3600, 7200, 14400, 21600, 43200,              // hours
  86400, 172800, 604800,                         // days/weeks
  2592000, 7776000, 15552000, 31536000           // months/quarters/years
};
static constexpr int kTimeIntervalCount = 24;

// Cross-platform timegm
static std::time_t portableTimegm(std::tm* tm) {
#ifdef _WIN32
  return _mkgmtime(tm);
#else
  return timegm(tm);
#endif
}

TimeTickSet computeNiceTimeTicks(float tMin, float tMax, int targetCount) {
  TimeTickSet result;
  if (targetCount < 1) targetCount = 1;
  if (tMax <= tMin) {
    result.stepSeconds = 1.0f;
    result.values.push_back(tMin);
    return result;
  }

  float range = tMax - tMin;
  float rawStep = range / static_cast<float>(targetCount);

  // Find smallest interval >= rawStep
  float step = kTimeIntervals[kTimeIntervalCount - 1];
  for (int i = 0; i < kTimeIntervalCount; i++) {
    if (kTimeIntervals[i] >= rawStep) {
      step = kTimeIntervals[i];
      break;
    }
  }
  result.stepSeconds = step;

  float firstTick;

  if (step < 86400.0f) {
    // Sub-day: modular alignment
    firstTick = std::ceil(tMin / step) * step;
  } else {
    // Day+ intervals: floor to day/month/year boundary via gmtime
    auto epoch = static_cast<std::time_t>(tMin);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &epoch);
#else
    gmtime_r(&epoch, &tm);
#endif
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;

    if (step >= 2592000.0f) {
      // Month+ interval: floor to month boundary
      tm.tm_mday = 1;
      if (step >= 31536000.0f) {
        // Year interval: floor to year boundary
        tm.tm_mon = 0;
      }
    }

    firstTick = static_cast<float>(portableTimegm(&tm));
    // Advance to be >= tMin
    while (firstTick < tMin) {
      if (step >= 31536000.0f) {
        tm.tm_year++;
        firstTick = static_cast<float>(portableTimegm(&tm));
      } else if (step >= 2592000.0f) {
        tm.tm_mon++;
        if (tm.tm_mon > 11) { tm.tm_mon = 0; tm.tm_year++; }
        firstTick = static_cast<float>(portableTimegm(&tm));
      } else {
        firstTick += step;
      }
    }
  }

  // Generate ticks from firstTick to tMax
  if (step >= 31536000.0f) {
    // Year stepping via calendar
    auto epoch = static_cast<std::time_t>(firstTick);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &epoch);
#else
    gmtime_r(&epoch, &tm);
#endif
    for (int i = 0; i < targetCount * 3; i++) {
      float t = static_cast<float>(portableTimegm(&tm));
      if (t > tMax + 0.5f) break;
      result.values.push_back(t);
      tm.tm_year++;
    }
  } else if (step >= 2592000.0f) {
    // Month stepping via calendar
    auto epoch = static_cast<std::time_t>(firstTick);
    std::tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &epoch);
#else
    gmtime_r(&epoch, &tm);
#endif
    int monthStep = static_cast<int>(step / 2592000.0f + 0.5f);
    if (monthStep < 1) monthStep = 1;
    for (int i = 0; i < targetCount * 3; i++) {
      float t = static_cast<float>(portableTimegm(&tm));
      if (t > tMax + 0.5f) break;
      result.values.push_back(t);
      tm.tm_mon += monthStep;
      while (tm.tm_mon > 11) { tm.tm_mon -= 12; tm.tm_year++; }
    }
  } else {
    // Sub-month: uniform step
    for (float t = firstTick; t <= tMax + step * 0.01f; t += step) {
      if (t > tMax + 0.5f) break;
      result.values.push_back(t);
    }
  }

  return result;
}

} // namespace dc
