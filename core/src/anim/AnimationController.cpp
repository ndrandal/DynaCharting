// ENC-636 (E2) — AnimationController implementation. See header.
#include "dc/anim/AnimationController.hpp"

namespace dc {

void AnimationController::startTween(std::int32_t rowId, float from, float to,
                                    float dur, bool exiting) {
  Tween t;
  t.from = from;
  t.to = to;
  t.duration = dur;
  t.easing = EasingType::EaseOutQuad;
  t.onUpdate = [this, rowId](float v) {
    auto it = rows_.find(rowId);
    if (it != rows_.end()) it->second.progress = v;
  };
  if (exiting) {
    t.onComplete = [this, rowId]() { rows_.erase(rowId); };  // fully gone
  } else {
    t.onComplete = [this, rowId]() {
      auto it = rows_.find(rowId);
      if (it != rows_.end()) {
        it->second.phase = Phase::Stable;
        it->second.progress = 1.0f;
      }
    };
  }
  rows_[rowId].tween = mgr_.addTween(std::move(t));
}

void AnimationController::syncRows(const std::vector<std::int32_t>& currentRowIds) {
  std::unordered_set<std::int32_t> live;
  live.reserve(currentRowIds.size());
  for (std::int32_t id : currentRowIds)
    if (id >= 0) live.insert(id);

  // ENTER (or re-enter an exiting row): present now, not currently entering/stable.
  for (std::int32_t id : live) {
    auto it = rows_.find(id);
    if (it == rows_.end()) {
      // Brand new row.
      rows_[id] = Row{Phase::Enter, 0.0f, 0};
      startTween(id, 0.0f, 1.0f, enterDur_, /*exiting=*/false);
    } else if (it->second.phase == Phase::Exit) {
      // Was fading out and came back — reverse from its current progress.
      mgr_.cancel(it->second.tween);
      it->second.phase = Phase::Enter;
      startTween(id, it->second.progress, 1.0f, enterDur_, /*exiting=*/false);
    }
    // Enter (still running) / Stable: leave as-is.
  }

  // EXIT: tracked, not exiting, no longer live.
  for (auto& [id, row] : rows_) {
    if (row.phase != Phase::Exit && live.find(id) == live.end()) {
      mgr_.cancel(row.tween);
      row.phase = Phase::Exit;
      startTween(id, row.progress, 0.0f, exitDur_, /*exiting=*/true);
    }
  }
}

}  // namespace dc
