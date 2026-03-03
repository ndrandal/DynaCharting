#include "dc/commands/ValidationConfig.hpp"

#include <algorithm>
#include <sstream>

namespace dc {

bool validateRange(float val, float minV, float maxV, const std::string& field,
                   bool strict, std::vector<ValidationWarning>& warnings, float& clamped) {
  if (val >= minV && val <= maxV) {
    clamped = val;
    return true;
  }

  if (strict) {
    return false;
  }

  // Clamp and record warning
  clamped = std::max(minV, std::min(maxV, val));

  std::ostringstream oss;
  oss << field << " value " << val << " clamped to [" << minV << ", " << maxV << "]";
  warnings.push_back({field, oss.str()});
  return true;
}

bool validateColor(float r, float g, float b, float a,
                   bool strict, std::vector<ValidationWarning>& warnings) {
  float channels[4] = {r, g, b, a};
  const char* names[4] = {"r", "g", "b", "a"};
  bool ok = true;

  for (int i = 0; i < 4; ++i) {
    if (channels[i] < 0.0f || channels[i] > 1.0f) {
      if (strict) {
        ok = false;
      } else {
        std::ostringstream oss;
        oss << "color." << names[i] << " value " << channels[i] << " outside [0, 1]";
        warnings.push_back({std::string("color.") + names[i], oss.str()});
      }
    }
  }

  return ok;
}

} // namespace dc
