#pragma once
#include <cstdint>

namespace dc {

enum class TouchPhase : std::uint8_t { Began, Moved, Ended, Cancelled };

struct TouchPoint {
  std::uint32_t id{0};
  double x{0}, y{0};
  TouchPhase phase{TouchPhase::Began};
};

enum class GestureType : std::uint8_t { None, Pinch, TwoFingerPan, Rotate };

struct GestureResult {
  GestureType type{GestureType::None};
  double centerX{0}, centerY{0};
  double scale{1.0};
  double rotation{0.0};
  double panDeltaX{0}, panDeltaY{0};
};

} // namespace dc
