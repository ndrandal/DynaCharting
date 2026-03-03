// D48 — DrawItemAnimator implementation
#include "dc/anim/DrawItemAnimator.hpp"

namespace dc {

DrawItemAnimator::DrawItemAnimator(AnimationManager& mgr, Scene& scene)
  : mgr_(mgr), scene_(scene) {}

TweenId DrawItemAnimator::animateColor(Id drawItemId, float r, float g, float b, float a,
                                       float duration, EasingType easing) {
  // Capture start color at creation time
  DrawItem* di = scene_.getDrawItemMutable(drawItemId);
  if (!di) return 0;

  float startR = di->color[0];
  float startG = di->color[1];
  float startB = di->color[2];
  float startA = di->color[3];

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = duration;
  tw.easing = easing;
  tw.onUpdate = [this, drawItemId, startR, startG, startB, startA, r, g, b, a](float t) {
    DrawItem* item = scene_.getDrawItemMutable(drawItemId);
    if (!item) return;
    item->color[0] = startR + (r - startR) * t;
    item->color[1] = startG + (g - startG) * t;
    item->color[2] = startB + (b - startB) * t;
    item->color[3] = startA + (a - startA) * t;
  };

  return mgr_.addTween(std::move(tw));
}

TweenId DrawItemAnimator::animateOpacity(Id drawItemId, float targetAlpha,
                                         float duration, EasingType easing) {
  DrawItem* di = scene_.getDrawItemMutable(drawItemId);
  if (!di) return 0;

  float startAlpha = di->color[3];

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = duration;
  tw.easing = easing;
  tw.onUpdate = [this, drawItemId, startAlpha, targetAlpha](float t) {
    DrawItem* item = scene_.getDrawItemMutable(drawItemId);
    if (!item) return;
    item->color[3] = startAlpha + (targetAlpha - startAlpha) * t;
  };

  return mgr_.addTween(std::move(tw));
}

TweenId DrawItemAnimator::animateTransform(Id drawItemId, float targetTx, float targetTy,
                                           float targetSx, float targetSy,
                                           float duration, EasingType easing) {
  // Look up the transform via the DrawItem's transformId
  const DrawItem* di = scene_.getDrawItem(drawItemId);
  if (!di || di->transformId == 0) return 0;

  Id transformId = di->transformId;
  Transform* tf = scene_.getTransformMutable(transformId);
  if (!tf) return 0;

  float startTx = tf->params.tx;
  float startTy = tf->params.ty;
  float startSx = tf->params.sx;
  float startSy = tf->params.sy;

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = duration;
  tw.easing = easing;
  tw.onUpdate = [this, transformId, startTx, startTy, startSx, startSy,
                 targetTx, targetTy, targetSx, targetSy](float t) {
    Transform* xf = scene_.getTransformMutable(transformId);
    if (!xf) return;
    xf->params.tx = startTx + (targetTx - startTx) * t;
    xf->params.ty = startTy + (targetTy - startTy) * t;
    xf->params.sx = startSx + (targetSx - startSx) * t;
    xf->params.sy = startSy + (targetSy - startSy) * t;
    recomputeMat3(*xf);
  };

  return mgr_.addTween(std::move(tw));
}

TweenId DrawItemAnimator::animateLineWidth(Id drawItemId, float target,
                                           float duration, EasingType easing) {
  DrawItem* di = scene_.getDrawItemMutable(drawItemId);
  if (!di) return 0;

  float startLW = di->lineWidth;

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = duration;
  tw.easing = easing;
  tw.onUpdate = [this, drawItemId, startLW, target](float t) {
    DrawItem* item = scene_.getDrawItemMutable(drawItemId);
    if (!item) return;
    item->lineWidth = startLW + (target - startLW) * t;
  };

  return mgr_.addTween(std::move(tw));
}

TweenId DrawItemAnimator::animatePointSize(Id drawItemId, float target,
                                           float duration, EasingType easing) {
  DrawItem* di = scene_.getDrawItemMutable(drawItemId);
  if (!di) return 0;

  float startPS = di->pointSize;

  Tween tw;
  tw.from = 0.0f;
  tw.to = 1.0f;
  tw.duration = duration;
  tw.easing = easing;
  tw.onUpdate = [this, drawItemId, startPS, target](float t) {
    DrawItem* item = scene_.getDrawItemMutable(drawItemId);
    if (!item) return;
    item->pointSize = startPS + (target - startPS) * t;
  };

  return mgr_.addTween(std::move(tw));
}

} // namespace dc
