#pragma once
#include <vector>

namespace dc {

struct PaneRegion {
  float clipYMin, clipYMax;
  float clipXMin, clipXMax;
};

// Compute clip-space regions for N panes stacked vertically.
// fractions: relative sizes (e.g. {0.7, 0.3} for 70%/30%).
// gap: spacing between panes in clip units.
// margin: outer margin in clip units.
inline std::vector<PaneRegion> computePaneLayout(
    const std::vector<float>& fractions,
    float gap = 0.05f, float margin = 0.05f) {
  std::vector<PaneRegion> result;
  if (fractions.empty()) return result;

  float totalFrac = 0.0f;
  for (float f : fractions) totalFrac += f;

  float totalGap = gap * static_cast<float>(fractions.size() - 1);
  float availableY = 2.0f - 2.0f * margin - totalGap; // clip space is [-1, 1]
  float topY = 1.0f - margin;

  for (std::size_t i = 0; i < fractions.size(); i++) {
    float height = availableY * (fractions[i] / totalFrac);
    PaneRegion r;
    r.clipYMax = topY;
    r.clipYMin = topY - height;
    r.clipXMin = -1.0f + margin;
    r.clipXMax = 1.0f - margin;
    result.push_back(r);
    topY = r.clipYMin - gap;
  }

  return result;
}

} // namespace dc
