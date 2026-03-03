#include "dc/viewport/GestureRecognizer.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace dc {

void GestureRecognizer::setEnabled(GestureType type, bool enabled) {
  switch (type) {
    case GestureType::Pinch:        enablePinch_  = enabled; break;
    case GestureType::TwoFingerPan: enablePan_    = enabled; break;
    case GestureType::Rotate:       enableRotate_ = enabled; break;
    case GestureType::None: break;
  }
}

void GestureRecognizer::reset() {
  active_.clear();
  prevDist_ = 0;
  prevAngle_ = 0;
  prevCenterX_ = 0;
  prevCenterY_ = 0;
  hasPrev_ = false;
}

GestureResult GestureRecognizer::processTouches(const TouchPoint* points, std::size_t count) {
  GestureResult result;

  // Update tracked touches
  for (std::size_t i = 0; i < count; ++i) {
    const auto& tp = points[i];
    switch (tp.phase) {
      case TouchPhase::Began:
      case TouchPhase::Moved:
        active_[tp.id] = TrackedTouch{tp.x, tp.y};
        break;
      case TouchPhase::Ended:
      case TouchPhase::Cancelled:
        active_.erase(tp.id);
        break;
    }
  }

  // Need exactly 2 active touches for gestures
  if (active_.size() != 2) {
    hasPrev_ = false;
    return result;
  }

  // Get the two active touch positions
  auto it = active_.begin();
  double x0 = it->second.x, y0 = it->second.y;
  ++it;
  double x1 = it->second.x, y1 = it->second.y;

  double dx = x1 - x0;
  double dy = y1 - y0;
  double dist = std::sqrt(dx * dx + dy * dy);
  double angle = std::atan2(dy, dx);
  double centerX = (x0 + x1) * 0.5;
  double centerY = (y0 + y1) * 0.5;

  result.centerX = centerX;
  result.centerY = centerY;

  if (hasPrev_) {
    // Determine gesture type based on deltas
    double scaleDelta = (prevDist_ > 1e-9) ? (dist / prevDist_) : 1.0;
    double rotDelta = angle - prevAngle_;
    double panDx = centerX - prevCenterX_;
    double panDy = centerY - prevCenterY_;

    // Normalize rotation delta to [-pi, pi]
    while (rotDelta >  M_PI) rotDelta -= 2.0 * M_PI;
    while (rotDelta < -M_PI) rotDelta += 2.0 * M_PI;

    // Determine dominant gesture
    double scaleAmount = std::fabs(scaleDelta - 1.0);
    double panAmount = std::sqrt(panDx * panDx + panDy * panDy);
    double rotAmount = std::fabs(rotDelta);

    if (enablePinch_ && scaleAmount > panAmount * 0.01 && scaleAmount > rotAmount) {
      result.type = GestureType::Pinch;
      result.scale = scaleDelta;
    } else if (enablePan_ && panAmount > 0) {
      result.type = GestureType::TwoFingerPan;
      result.panDeltaX = panDx;
      result.panDeltaY = panDy;
    } else if (enableRotate_ && rotAmount > 1e-6) {
      result.type = GestureType::Rotate;
      result.rotation = rotDelta;
    }

    // Always provide all values regardless of dominant type
    result.scale = scaleDelta;
    result.panDeltaX = panDx;
    result.panDeltaY = panDy;
    result.rotation = rotDelta;
  }

  prevDist_ = dist;
  prevAngle_ = angle;
  prevCenterX_ = centerX;
  prevCenterY_ = centerY;
  hasPrev_ = true;

  return result;
}

} // namespace dc
