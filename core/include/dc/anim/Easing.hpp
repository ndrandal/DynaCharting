#pragma once
#include <cmath>
#include <cstdint>

namespace dc {

enum class EasingType : std::uint8_t {
  Linear = 0,
  EaseInQuad,
  EaseOutQuad,
  EaseInOutQuad,
  EaseInCubic,
  EaseOutCubic,
  EaseInOutCubic,
  EaseOutBack,
  EaseOutElastic
};

// Evaluate easing function at t ∈ [0,1]. Returns value in [0,1] (may overshoot for Back/Elastic).
inline float ease(EasingType type, float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;

  switch (type) {
    case EasingType::Linear:
      return t;

    case EasingType::EaseInQuad:
      return t * t;

    case EasingType::EaseOutQuad:
      return t * (2.0f - t);

    case EasingType::EaseInOutQuad:
      return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);

    case EasingType::EaseInCubic:
      return t * t * t;

    case EasingType::EaseOutCubic: {
      float u = t - 1.0f;
      return u * u * u + 1.0f;
    }

    case EasingType::EaseInOutCubic:
      return (t < 0.5f) ? (4.0f * t * t * t)
                         : ((t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f);

    case EasingType::EaseOutBack: {
      constexpr float c1 = 1.70158f;
      constexpr float c3 = c1 + 1.0f;
      float u = t - 1.0f;
      return 1.0f + c3 * u * u * u + c1 * u * u;
    }

    case EasingType::EaseOutElastic: {
      constexpr float c4 = (2.0f * 3.14159265f) / 3.0f;
      return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
    }

    default:
      return t;
  }
}

} // namespace dc
