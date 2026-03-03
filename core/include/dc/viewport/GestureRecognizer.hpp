#pragma once
#include "dc/viewport/TouchState.hpp"
#include <unordered_map>

namespace dc {

class GestureRecognizer {
public:
  GestureResult processTouches(const TouchPoint* points, std::size_t count);
  void setEnabled(GestureType type, bool enabled);
  void reset();

private:
  struct TrackedTouch { double x, y; };
  std::unordered_map<std::uint32_t, TrackedTouch> active_;
  double prevDist_{0}, prevAngle_{0};
  double prevCenterX_{0}, prevCenterY_{0};
  bool hasPrev_{false};
  bool enablePinch_{true}, enablePan_{true}, enableRotate_{false};
};

} // namespace dc
