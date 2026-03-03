#include "dc/layout/Anchor.hpp"

namespace dc {

std::pair<float, float> computeAnchorClipPosition(
    AnchorPoint point, const PaneRegion& region,
    float offsetX, float offsetY,
    int viewW, int viewH) {
  float cx = 0, cy = 0;

  switch (point) {
    case AnchorPoint::TopLeft:
    case AnchorPoint::MiddleLeft:
    case AnchorPoint::BottomLeft:
      cx = region.clipXMin;
      break;
    case AnchorPoint::TopCenter:
    case AnchorPoint::Center:
    case AnchorPoint::BottomCenter:
      cx = (region.clipXMin + region.clipXMax) * 0.5f;
      break;
    case AnchorPoint::TopRight:
    case AnchorPoint::MiddleRight:
    case AnchorPoint::BottomRight:
      cx = region.clipXMax;
      break;
  }

  switch (point) {
    case AnchorPoint::TopLeft:
    case AnchorPoint::TopCenter:
    case AnchorPoint::TopRight:
      cy = region.clipYMax;
      break;
    case AnchorPoint::MiddleLeft:
    case AnchorPoint::Center:
    case AnchorPoint::MiddleRight:
      cy = (region.clipYMin + region.clipYMax) * 0.5f;
      break;
    case AnchorPoint::BottomLeft:
    case AnchorPoint::BottomCenter:
    case AnchorPoint::BottomRight:
      cy = region.clipYMin;
      break;
  }

  float pxToClipX = (viewW > 0) ? (2.0f / static_cast<float>(viewW)) : 0.0f;
  float pxToClipY = (viewH > 0) ? (2.0f / static_cast<float>(viewH)) : 0.0f;

  cx += offsetX * pxToClipX;
  cy -= offsetY * pxToClipY;

  return {cx, cy};
}

} // namespace dc
