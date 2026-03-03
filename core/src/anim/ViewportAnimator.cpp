#include "dc/anim/ViewportAnimator.hpp"

namespace dc {

ViewportAnimator::ViewportAnimator(AnimationManager& mgr)
  : mgr_(mgr) {}

void ViewportAnimator::animateTo(Viewport& vp,
                                  double xMin, double xMax,
                                  double yMin, double yMax,
                                  float durationSec,
                                  EasingType easing) {
  cancel();

  const auto startRange = vp.dataRange();
  double sx0 = startRange.xMin, sx1 = startRange.xMax;
  double sy0 = startRange.yMin, sy1 = startRange.yMax;

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = durationSec;
  tw.easing = easing;
  tw.onUpdate = [&vp, sx0, sx1, sy0, sy1, xMin, xMax, yMin, yMax](float t) {
    double dt = static_cast<double>(t);
    vp.setDataRange(
      sx0 + (xMin - sx0) * dt,
      sx1 + (xMax - sx1) * dt,
      sy0 + (yMin - sy0) * dt,
      sy1 + (yMax - sy1) * dt
    );
  };

  activeTween_ = mgr_.addTween(std::move(tw));
}

void ViewportAnimator::animateToX(Viewport& vp,
                                   double xMin, double xMax,
                                   float durationSec,
                                   EasingType easing) {
  const auto& dr = vp.dataRange();
  animateTo(vp, xMin, xMax, dr.yMin, dr.yMax, durationSec, easing);
}

void ViewportAnimator::animateToY(Viewport& vp,
                                   double yMin, double yMax,
                                   float durationSec,
                                   EasingType easing) {
  const auto& dr = vp.dataRange();
  animateTo(vp, dr.xMin, dr.xMax, yMin, yMax, durationSec, easing);
}

void ViewportAnimator::cancel() {
  if (activeTween_ != 0) {
    mgr_.cancel(activeTween_);
    activeTween_ = 0;
  }
}

bool ViewportAnimator::isAnimating() const {
  return activeTween_ != 0 && mgr_.isActive(activeTween_);
}

} // namespace dc
