#pragma once
#include "dc/anim/Easing.hpp"
#include <cstdint>
#include <functional>

namespace dc {

using TweenId = std::uint32_t;

struct Tween {
  TweenId id{0};
  float from{0.0f};
  float to{1.0f};
  float duration{0.3f};       // seconds
  float elapsed{0.0f};
  EasingType easing{EasingType::EaseOutQuad};
  std::function<void(float value)> onUpdate;  // called each tick with interpolated value
  std::function<void()> onComplete;           // called once when finished
  bool finished{false};
};

} // namespace dc
