// D48 — DrawItemAnimator: animate DrawItem properties (color, opacity, transform, etc.)
#pragma once
#include "dc/anim/AnimationManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/ids/Id.hpp"

namespace dc {

class DrawItemAnimator {
public:
  DrawItemAnimator(AnimationManager& mgr, Scene& scene);

  // Animate all 4 color channels from current to target over duration seconds.
  TweenId animateColor(Id drawItemId, float r, float g, float b, float a,
                       float duration, EasingType easing = EasingType::EaseOutQuad);

  // Animate only the alpha channel from current to targetAlpha.
  TweenId animateOpacity(Id drawItemId, float targetAlpha,
                         float duration, EasingType easing = EasingType::EaseOutQuad);

  // Animate the transform params (tx, ty, sx, sy) and recompute mat3 each tick.
  TweenId animateTransform(Id drawItemId, float targetTx, float targetTy,
                           float targetSx, float targetSy,
                           float duration, EasingType easing = EasingType::EaseOutQuad);

  // Animate lineWidth from current to target.
  TweenId animateLineWidth(Id drawItemId, float target,
                           float duration, EasingType easing = EasingType::EaseOutQuad);

  // Animate pointSize from current to target.
  TweenId animatePointSize(Id drawItemId, float target,
                           float duration, EasingType easing = EasingType::EaseOutQuad);

private:
  AnimationManager& mgr_;
  Scene& scene_;
};

} // namespace dc
