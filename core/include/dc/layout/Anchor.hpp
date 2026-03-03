#pragma once
#include "dc/layout/PaneLayout.hpp"
#include <cstdint>
#include <utility>

namespace dc {

enum class AnchorPoint : std::uint8_t {
  TopLeft, TopCenter, TopRight,
  MiddleLeft, Center, MiddleRight,
  BottomLeft, BottomCenter, BottomRight
};

struct Anchor {
  AnchorPoint point{AnchorPoint::TopLeft};
  float offsetX{0};  // pixels
  float offsetY{0};  // pixels
};

std::pair<float, float> computeAnchorClipPosition(
    AnchorPoint point, const PaneRegion& region,
    float offsetX, float offsetY,
    int viewW, int viewH);

} // namespace dc
