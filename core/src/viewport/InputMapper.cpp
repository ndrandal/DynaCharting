#include "dc/viewport/InputMapper.hpp"

namespace dc {

void InputMapper::setConfig(const InputMapperConfig& cfg) {
  config_ = cfg;
}

void InputMapper::setViewports(std::vector<Viewport*> viewports) {
  viewports_ = std::move(viewports);
}

bool InputMapper::processInput(const ViewportInputState& input) {
  lastCursorX_ = input.cursorX;
  lastCursorY_ = input.cursorY;

  // Find which viewport contains the cursor
  active_ = nullptr;
  for (auto* vp : viewports_) {
    if (vp && vp->containsPixel(input.cursorX, input.cursorY)) {
      active_ = vp;
      break;
    }
  }

  if (!active_) return false;

  bool changed = false;

  // Pan
  if (input.dragging && (input.dragDx != 0 || input.dragDy != 0)) {
    active_->pan(input.dragDx, input.dragDy);
    changed = true;

    // Linked X-axis: apply same X-pan to other viewports
    if (config_.linkXAxis) {
      for (auto* vp : viewports_) {
        if (vp && vp != active_) {
          vp->pan(input.dragDx, 0); // only X component
        }
      }
    }
  }

  // Zoom
  if (input.scrollDelta != 0) {
    double factor = input.scrollDelta * config_.zoomSensitivity;
    active_->zoom(factor, input.cursorX, input.cursorY);
    changed = true;

    // Linked X-axis: apply same X-zoom to other viewports
    if (config_.linkXAxis) {
      for (auto* vp : viewports_) {
        if (vp && vp != active_) {
          // Zoom only X-axis with same factor at equivalent X pivot
          double pivotDx, pivotDy;
          active_->pixelToData(input.cursorX, input.cursorY, pivotDx, pivotDy);

          // Scale only X range around the same data X pivot
          double scale = 1.0 / (1.0 + factor);
          auto range = vp->dataRange();
          double newXMin = pivotDx + (range.xMin - pivotDx) * scale;
          double newXMax = pivotDx + (range.xMax - pivotDx) * scale;
          vp->setDataRange(newXMin, newXMax, range.yMin, range.yMax);
        }
      }
    }
  }

  return changed;
}

bool InputMapper::cursorClip(double& cx, double& cy) const {
  if (!active_) return false;
  active_->pixelToClip(lastCursorX_, lastCursorY_, cx, cy);
  return true;
}

bool InputMapper::cursorData(double& dx, double& dy) const {
  if (!active_) return false;
  active_->pixelToData(lastCursorX_, lastCursorY_, dx, dy);
  return true;
}

} // namespace dc
