#include "dc/layout/LayoutInteraction.hpp"
#include "dc/layout/LayoutManager.hpp"

#include <cmath>

namespace dc {

void LayoutInteraction::setConfig(const LayoutInteractionConfig& cfg) {
  config_ = cfg;
}

void LayoutInteraction::setLayoutManager(LayoutManager* lm) {
  lm_ = lm;
}

void LayoutInteraction::setPixelViewport(int fbWidth, int fbHeight) {
  fbW_ = fbWidth;
  fbH_ = fbHeight;
}

bool LayoutInteraction::processInput(const ViewportInputState& input) {
  if (!lm_ || fbH_ <= 0) return false;

  // Handle active drag first (don't re-hover during drag)
  if (input.dragging && dragging_ && dragDivider_ >= 0) {
    float totalFrac = 0.0f;
    for (const auto& e : lm_->entries()) totalFrac += e.fraction;

    float delta = static_cast<float>(input.dragDy / static_cast<double>(fbH_)) * totalFrac;
    if (std::fabs(delta) > 1e-7f) {
      lm_->resizeDivider(static_cast<std::size_t>(dragDivider_), delta);
      return true;
    }
    return false;
  }

  // Convert cursor pixel Y to clip Y (Y flipped: pixel 0 = top = clip +1)
  double clipY = 1.0 - input.cursorY / static_cast<double>(fbH_) * 2.0;
  double tolClip = config_.hitTolerancePx / static_cast<double>(fbH_) * 2.0;

  // Hit test all dividers
  hoveredDivider_ = -1;
  for (std::size_t i = 0; i < lm_->dividerCount(); i++) {
    double divY = static_cast<double>(lm_->dividerClipY(i));
    if (std::fabs(clipY - divY) < tolClip) {
      hoveredDivider_ = static_cast<int>(i);
      break;
    }
  }

  if (input.dragging && !dragging_ && hoveredDivider_ >= 0) {
    // Start drag from hovered divider
    dragging_ = true;
    dragDivider_ = hoveredDivider_;

    float totalFrac = 0.0f;
    for (const auto& e : lm_->entries()) totalFrac += e.fraction;

    float delta = static_cast<float>(input.dragDy / static_cast<double>(fbH_)) * totalFrac;
    if (std::fabs(delta) > 1e-7f) {
      lm_->resizeDivider(static_cast<std::size_t>(dragDivider_), delta);
      return true;
    }
  }

  if (!input.dragging && dragging_) {
    dragging_ = false;
    dragDivider_ = -1;
  }

  return false;
}

} // namespace dc
