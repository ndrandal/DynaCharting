// ENC-637 (E3) — per-instance transition lanes. See header.
#include "dc/anim/InstanceTransition.hpp"

#include <cmath>
#include <cstring>

namespace dc {

// Progress for instance i: the controller's per-row value, or 1 if untracked
// (untracked == not transitioning == full strength). Clamped to [0,1].
static float instanceProgress(const std::vector<std::int32_t>& rowIds,
                              std::size_t i, const AnimationController& ctrl) {
  if (i >= rowIds.size()) return 1.0f;
  const std::int32_t id = rowIds[i];
  if (id < 0 || !ctrl.isTracked(id)) return 1.0f;
  float p = ctrl.progressOf(id);
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return p;
}

void applyOpacityTransition(std::uint8_t* bytes, std::size_t count,
                            std::size_t stride, std::size_t rgbaOffset,
                            const std::vector<std::int32_t>& rowIds,
                            const AnimationController& ctrl) {
  if (!bytes || stride == 0) return;
  for (std::size_t i = 0; i < count; ++i) {
    const float p = instanceProgress(rowIds, i, ctrl);
    if (p >= 1.0f) continue;  // full alpha: nothing to do
    std::uint8_t* a = bytes + i * stride + rgbaOffset + 3;  // A byte (R,G,B,A)
    *a = static_cast<std::uint8_t>(std::lround(static_cast<float>(*a) * p));
  }
}

void applyScaleTransition(std::uint8_t* bytes, std::size_t count,
                          std::size_t stride, std::size_t sizeOffset,
                          const std::vector<std::int32_t>& rowIds,
                          const AnimationController& ctrl) {
  if (!bytes || stride == 0) return;
  for (std::size_t i = 0; i < count; ++i) {
    const float p = instanceProgress(rowIds, i, ctrl);
    if (p >= 1.0f) continue;
    std::uint8_t* slot = bytes + i * stride + sizeOffset;
    float size;
    std::memcpy(&size, slot, sizeof(float));
    size *= p;
    std::memcpy(slot, &size, sizeof(float));
  }
}

}  // namespace dc
