#pragma once
#include "dc/anim/Tween.hpp"
#include <vector>

namespace dc {

class AnimationManager {
public:
  // Add a tween. Returns its ID for later cancellation.
  TweenId addTween(Tween tween);

  // Cancel an active tween by ID. Returns true if found and removed.
  bool cancel(TweenId id);

  // Cancel all active tweens.
  void cancelAll();

  // Advance all tweens by dt seconds. Fires callbacks, removes finished tweens.
  void tick(float dt);

  // Number of currently active (non-finished) tweens.
  std::size_t activeCount() const;

  // Check if a specific tween is still active.
  bool isActive(TweenId id) const;

private:
  TweenId nextId_{1};
  std::vector<Tween> tweens_;
};

} // namespace dc
