// ENC-636 (E2) — AnimationController: data-bound transitions with object constancy.
//
// The research's "keyed object constancy + transition clock": when the live row set
// changes (data append/eviction, or a selection filtering rows in/out), diff the
// DURABLE row ids (RowIdentity / EncodeResult::instanceRowIds, ENC-594) old vs new
// and classify each row:
//   * ENTER  — newly present: tween a per-row progress 0 -> 1 (fade/scale in).
//   * EXIT   — gone: tween progress 1 -> 0, then drop the row (fade/scale out).
//   * STABLE — present in both: progress held at 1.
// progressOf(rowId) is the per-row [0,1] the encode/render side (E3) multiplies
// into a per-instance lane (opacity/size) so enters/exits animate, keyed by id so a
// row keeps its identity across frames (no fl! popping).
//
// Tweens run on a borrowed AnimationManager driven by the FrameClock (E1). Pure
// `dc` (no GPU): feed it the current row-id set each refresh; it owns only the
// per-row animation state.
#pragma once

#include "dc/anim/AnimationManager.hpp"
#include "dc/anim/Easing.hpp"
#include "dc/anim/Tween.hpp"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dc {

class AnimationController {
 public:
  enum class Phase : std::uint8_t { Enter, Stable, Exit };

  // `mgr` is borrowed (driven by a FrameClock). Durations in seconds.
  explicit AnimationController(AnimationManager& mgr, float enterSeconds = 0.3f,
                               float exitSeconds = 0.3f)
      : mgr_(mgr), enterDur_(enterSeconds), exitDur_(exitSeconds) {}

  // Reconcile the live row-id set: ENTER newly-present rows (0->1), EXIT departed
  // rows (1->0, dropped on completion), hold survivors STABLE. Idempotent if the
  // set is unchanged. `< 0` ids are ignored (no row-id threading).
  void syncRows(const std::vector<std::int32_t>& currentRowIds);

  // Per-row transition progress in [0,1]; 0 for an unknown row.
  float progressOf(std::int32_t rowId) const {
    auto it = rows_.find(rowId);
    return it == rows_.end() ? 0.0f : it->second.progress;
  }
  Phase phaseOf(std::int32_t rowId) const {
    auto it = rows_.find(rowId);
    return it == rows_.end() ? Phase::Exit : it->second.phase;
  }
  bool isTracked(std::int32_t rowId) const { return rows_.count(rowId) > 0; }
  // Rows tracked, INCLUDING ones still exiting (rendered while they fade out).
  std::size_t trackedCount() const { return rows_.size(); }

 private:
  struct Row {
    Phase phase{Phase::Enter};
    float progress{0.0f};
    TweenId tween{0};
  };

  void startTween(std::int32_t rowId, float from, float to, float dur,
                  bool exiting);

  AnimationManager& mgr_;
  float enterDur_;
  float exitDur_;
  std::unordered_map<std::int32_t, Row> rows_;
};

}  // namespace dc
