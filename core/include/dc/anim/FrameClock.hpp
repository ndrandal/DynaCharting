// ENC-635 (E1) — FrameClock: the per-frame heartbeat that drives animations.
//
// The anim subsystem (AnimationManager/Tween, D27/D48) is functional but UNWIRED:
// nothing calls AnimationManager::tick(), and no frame loop exists (the research's
// "transition clock that hasn't been built"). FrameClock is that hook: the host's
// render loop calls tick(dt) once per frame; FrameClock advances every registered
// AnimationManager and reports whether anything is still animating, so the host
// knows to schedule another frame (and can idle when nothing moves).
//
// It is the foundation for data-bound transitions: the AnimationController (E2)
// registers its manager here, and enter/exit/update tweens (E3) advance off this
// single clock. Pure `dc` (no GPU): dt is supplied by the caller (a real frame
// delta in the host, a fixed step in tests).
#pragma once

#include "dc/anim/AnimationManager.hpp"

#include <cstdint>
#include <vector>

namespace dc {

class FrameClock {
 public:
  // Register an AnimationManager to drive each tick (idempotent; nullptr ignored).
  void addManager(AnimationManager* mgr) {
    if (!mgr) return;
    for (auto* m : managers_)
      if (m == mgr) return;
    managers_.push_back(mgr);
  }
  void removeManager(AnimationManager* mgr) {
    for (std::size_t i = 0; i < managers_.size(); ++i) {
      if (managers_[i] == mgr) {
        managers_.erase(managers_.begin() + static_cast<std::ptrdiff_t>(i));
        return;
      }
    }
  }

  // Advance the clock by `dtSeconds`: tick every registered manager (firing tween
  // onUpdate/onComplete callbacks) and bump the frame counter. Returns true if any
  // animation is still active afterwards — i.e. the host should render again.
  bool tick(float dtSeconds) {
    elapsed_ += static_cast<double>(dtSeconds);
    ++frame_;
    for (auto* m : managers_)
      if (m) m->tick(dtSeconds);
    return isAnimating();
  }

  // True iff any registered manager has an active tween (the host can idle when
  // this is false — no wasted frames between interactions).
  bool isAnimating() const {
    for (auto* m : managers_)
      if (m && m->activeCount() > 0) return true;
    return false;
  }

  double elapsedSeconds() const { return elapsed_; }
  std::uint64_t frame() const { return frame_; }
  std::size_t managerCount() const { return managers_.size(); }

 private:
  std::vector<AnimationManager*> managers_;
  double elapsed_{0.0};
  std::uint64_t frame_{0};
};

}  // namespace dc
