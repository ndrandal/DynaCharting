#pragma once
#include "dc/anim/AnimationManager.hpp"
#include "dc/viewport/Viewport.hpp"

namespace dc {

class ViewportAnimator {
public:
  explicit ViewportAnimator(AnimationManager& mgr);

  // Animate viewport to a new data range over duration seconds.
  // Cancels any in-flight viewport animation first.
  void animateTo(Viewport& vp,
                 double xMin, double xMax,
                 double yMin, double yMax,
                 float durationSec = 0.3f,
                 EasingType easing = EasingType::EaseOutCubic);

  // Animate only the X range (Y stays current).
  void animateToX(Viewport& vp,
                  double xMin, double xMax,
                  float durationSec = 0.3f,
                  EasingType easing = EasingType::EaseOutCubic);

  // Animate only the Y range (X stays current).
  void animateToY(Viewport& vp,
                  double yMin, double yMax,
                  float durationSec = 0.3f,
                  EasingType easing = EasingType::EaseOutCubic);

  // Cancel any in-flight animation.
  void cancel();

  // Is an animation currently active?
  bool isAnimating() const;

private:
  AnimationManager& mgr_;
  TweenId activeTween_{0};
};

} // namespace dc
