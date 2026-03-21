#include "dc/anim/AnimationManager.hpp"
#include <algorithm>

namespace dc {

TweenId AnimationManager::addTween(Tween tween) {
  tween.id = nextId_++;
  tween.finished = false;
  tween.elapsed = 0.0f;
  tweens_.push_back(std::move(tween));
  return tweens_.back().id;
}

bool AnimationManager::cancel(TweenId id) {
  for (auto it = tweens_.begin(); it != tweens_.end(); ++it) {
    if (it->id == id) {
      tweens_.erase(it);
      return true;
    }
  }
  return false;
}

void AnimationManager::cancelAll() {
  tweens_.clear();
}

void AnimationManager::tick(float dt) {
  // Swap tweens_ into a local to guard against iterator invalidation:
  // callbacks (onUpdate, onComplete) may call addTween(), which would
  // reallocate tweens_ while we're iterating over it.
  auto active = std::move(tweens_);
  tweens_.clear();  // any addTween() during callbacks goes into fresh tweens_

  for (auto& tw : active) {
    if (tw.finished) continue;

    tw.elapsed += dt;
    float t = (tw.duration > 0.0f) ? (tw.elapsed / tw.duration) : 1.0f;
    if (t >= 1.0f) t = 1.0f;

    float eased = ease(tw.easing, t);
    float value = tw.from + (tw.to - tw.from) * eased;

    if (tw.onUpdate) {
      tw.onUpdate(value);
    }

    if (t >= 1.0f) {
      tw.finished = true;
      if (tw.onComplete) {
        tw.onComplete();
      }
    }
  }

  // Keep unfinished tweens, prepend before any newly-added ones
  for (auto& tw : active) {
    if (!tw.finished) tweens_.insert(tweens_.begin(), std::move(tw));
  }
}

std::size_t AnimationManager::activeCount() const {
  return tweens_.size();
}

bool AnimationManager::isActive(TweenId id) const {
  for (const auto& tw : tweens_) {
    if (tw.id == id) return true;
  }
  return false;
}

} // namespace dc
