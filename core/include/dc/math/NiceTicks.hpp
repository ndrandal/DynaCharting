#pragma once
#include <vector>

namespace dc {

struct TickSet {
  float min, max, step;
  std::vector<float> values;
};

// Compute "nice" tick values for an axis range.
// Snaps step to {1, 2, 2.5, 5, 10} × 10^n, then generates values.
TickSet computeNiceTicks(float lo, float hi, int targetCount = 5);

} // namespace dc
