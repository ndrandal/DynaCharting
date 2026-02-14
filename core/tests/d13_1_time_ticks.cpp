// D13.1 — NiceTimeTicks unit test (pure C++)

#include "dc/math/NiceTimeTicks.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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

int main() {
  // 1-hour range → step should be <= 600s (10 minutes)
  {
    float tMin = 1700000000.0f;
    float tMax = tMin + 3600.0f;
    auto ticks = dc::computeNiceTimeTicks(tMin, tMax, 6);
    check(ticks.stepSeconds <= 600.0f,
          "1-hour range: step <= 600s");
    check(ticks.stepSeconds >= 60.0f,
          "1-hour range: step >= 60s");
    check(!ticks.values.empty(), "1-hour range: ticks not empty");
  }

  // 24-hour range → step in [3600, 14400] range
  {
    float tMin = 1700000000.0f;
    float tMax = tMin + 86400.0f;
    auto ticks = dc::computeNiceTimeTicks(tMin, tMax, 6);
    check(ticks.stepSeconds >= 3600.0f,
          "24-hour range: step >= 3600s");
    check(ticks.stepSeconds <= 14400.0f,
          "24-hour range: step <= 14400s");
    check(!ticks.values.empty(), "24-hour range: ticks not empty");
  }

  // 30-day range → step in [86400, 604800] range
  {
    float tMin = 1700000000.0f;
    float tMax = tMin + 30.0f * 86400.0f;
    auto ticks = dc::computeNiceTimeTicks(tMin, tMax, 6);
    check(ticks.stepSeconds >= 86400.0f,
          "30-day range: step >= 86400s");
    check(ticks.stepSeconds <= 604800.0f,
          "30-day range: step <= 604800s");
    check(!ticks.values.empty(), "30-day range: ticks not empty");
  }

  // All tick values within [tMin, tMax]
  {
    float tMin = 1700000000.0f;
    float tMax = tMin + 3600.0f;
    auto ticks = dc::computeNiceTimeTicks(tMin, tMax, 6);
    bool allInRange = true;
    for (float v : ticks.values) {
      if (v < tMin - 1.0f || v > tMax + 1.0f) {
        allInRange = false;
        break;
      }
    }
    check(allInRange, "all ticks within [tMin, tMax]");
  }

  // Sub-day tick alignment: value % step ≈ 0
  // Use small epoch values to avoid float precision issues at 1.7e9 magnitude
  {
    float tMin = 0.0f;
    float tMax = 3600.0f;
    auto ticks = dc::computeNiceTimeTicks(tMin, tMax, 6);
    bool aligned = true;
    for (float v : ticks.values) {
      float rem = std::fmod(v, ticks.stepSeconds);
      if (rem > 1.0f && (ticks.stepSeconds - rem) > 1.0f) {
        aligned = false;
        break;
      }
    }
    check(aligned, "sub-day ticks are modular-aligned");
  }

  // Degenerate: tMax == tMin
  {
    auto ticks = dc::computeNiceTimeTicks(100.0f, 100.0f);
    check(ticks.values.size() == 1, "degenerate range: 1 tick");
  }

  std::printf("D13.1 time_ticks: %d/%d PASS\n", passed, tests);
  return 0;
}
