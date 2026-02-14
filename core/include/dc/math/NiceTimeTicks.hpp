#pragma once
#include <vector>

namespace dc {

struct TimeTickSet {
  float stepSeconds;              // chosen interval in seconds
  std::vector<float> values;      // tick positions (epoch seconds)
};

// Compute time-aligned tick positions for a time range [tMin, tMax].
// Snaps to human-meaningful intervals (1s, 5s, 1min, 1hr, 1day, etc.).
TimeTickSet computeNiceTimeTicks(float tMin, float tMax, int targetCount = 6);

} // namespace dc
