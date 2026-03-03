#include "dc/interaction/HoverManager.hpp"

namespace dc {

void HoverManager::update(std::uint32_t drawItemId, double dataX, double dataY,
                           double screenX, double screenY) {
  dataX_ = dataX;
  dataY_ = dataY;
  screenX_ = screenX;
  screenY_ = screenY;

  bool targetChanged = (drawItemId != currentDrawItemId_);

  if (targetChanged) {
    // Fire exit callback for previous target (if was hovering something)
    if (hovering_ && onExit_) {
      onExit_(currentDrawItemId_, dataX_, dataY_);
    }

    currentDrawItemId_ = drawItemId;

    if (drawItemId != 0) {
      hovering_ = true;
      // Fire enter callback for new target
      if (onEnter_) {
        onEnter_(drawItemId, dataX, dataY);
      }
      // Populate tooltip via provider
      if (provider_) {
        tooltip_ = provider_(drawItemId, dataX, dataY);
      } else {
        tooltip_ = {};
        tooltip_.visible = true;
        tooltip_.drawItemId = drawItemId;
        tooltip_.dataX = dataX;
        tooltip_.dataY = dataY;
        tooltip_.screenX = screenX;
        tooltip_.screenY = screenY;
      }
    } else {
      hovering_ = false;
      tooltip_ = {};
    }
  } else if (hovering_) {
    // Same target, update positions in tooltip
    tooltip_.dataX = dataX;
    tooltip_.dataY = dataY;
    tooltip_.screenX = screenX;
    tooltip_.screenY = screenY;
  }
}

void HoverManager::clear() {
  if (hovering_ && onExit_) {
    onExit_(currentDrawItemId_, dataX_, dataY_);
  }
  hovering_ = false;
  currentDrawItemId_ = 0;
  dataX_ = 0;
  dataY_ = 0;
  screenX_ = 0;
  screenY_ = 0;
  tooltip_ = {};
}

bool HoverManager::isHovering() const {
  return hovering_;
}

std::uint32_t HoverManager::hoveredDrawItemId() const {
  return currentDrawItemId_;
}

double HoverManager::hoverDataX() const {
  return dataX_;
}

double HoverManager::hoverDataY() const {
  return dataY_;
}

const TooltipData& HoverManager::tooltip() const {
  return tooltip_;
}

void HoverManager::setOnHoverEnter(HoverCallback cb) {
  onEnter_ = std::move(cb);
}

void HoverManager::setOnHoverExit(HoverCallback cb) {
  onExit_ = std::move(cb);
}

void HoverManager::setTooltipProvider(TooltipProvider provider) {
  provider_ = std::move(provider);
}

void HoverManager::setHoverDelay(double milliseconds) {
  hoverDelay_ = milliseconds;
}

double HoverManager::hoverDelay() const {
  return hoverDelay_;
}

void HoverManager::setTooltipAnchor(TooltipAnchor anchor) {
  anchor_ = anchor;
}

HoverManager::TooltipAnchor HoverManager::tooltipAnchor() const {
  return anchor_;
}

} // namespace dc
