#include "dc/math/NiceTicks.hpp"
#include <cmath>

namespace dc {

TickSet computeNiceTicks(float lo, float hi, int targetCount) {
  TickSet result;
  if (targetCount < 1) targetCount = 1;
  if (hi <= lo) {
    result.min = lo;
    result.max = hi;
    result.step = 1.0f;
    result.values.push_back(lo);
    return result;
  }

  float range = hi - lo;
  float rawStep = range / static_cast<float>(targetCount);

  // Snap to nice step: {1, 2, 2.5, 5, 10} × 10^n
  float mag = std::pow(10.0f, std::floor(std::log10(rawStep)));
  float residual = rawStep / mag;

  float niceStep;
  if (residual <= 1.0f)       niceStep = 1.0f * mag;
  else if (residual <= 2.0f)  niceStep = 2.0f * mag;
  else if (residual <= 2.5f)  niceStep = 2.5f * mag;
  else if (residual <= 5.0f)  niceStep = 5.0f * mag;
  else                        niceStep = 10.0f * mag;

  result.step = niceStep;
  result.min = std::floor(lo / niceStep) * niceStep;
  result.max = std::ceil(hi / niceStep) * niceStep;

  for (float v = result.min; v <= result.max + niceStep * 0.01f; v += niceStep) {
    result.values.push_back(v);
  }

  return result;
}

} // namespace dc
