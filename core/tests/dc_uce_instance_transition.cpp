// ENC-637 (E3) — per-instance transition lanes: scale instance opacity/size by the
// AnimationController's per-row progress, keyed by row id.
#include "dc/anim/AnimationController.hpp"
#include "dc/anim/AnimationManager.hpp"
#include "dc/anim/InstanceTransition.hpp"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>

static int passed = 0, failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
using namespace dc;

// Point4Color: pos2(8) + RGBA8(@8) + size(@12) = 16B.
static constexpr std::size_t kStride = 16, kRgba = 8, kSize = 12;

static std::vector<std::uint8_t> makeInstances(std::size_t n, float size) {
  std::vector<std::uint8_t> b(n * kStride, 0);
  for (std::size_t i = 0; i < n; ++i) {
    // RGBA8 = opaque white (R,G,B,A all 255).
    for (int k = 0; k < 4; ++k) b[i * kStride + kRgba + k] = 255;
    std::memcpy(b.data() + i * kStride + kSize, &size, sizeof(float));
  }
  return b;
}
static std::uint8_t alpha(const std::vector<std::uint8_t>& b, std::size_t i) {
  return b[i * kStride + kRgba + 3];
}
static float sizeOf(const std::vector<std::uint8_t>& b, std::size_t i) {
  float s; std::memcpy(&s, b.data() + i * kStride + kSize, sizeof(float)); return s;
}

int main() {
  std::printf("=== ENC-637 (E3) per-instance transition lanes ===\n");

  AnimationManager mgr;
  AnimationController ctrl(mgr, 0.2f, 0.2f);

  // Rows 1,2,3 enter, complete to Stable.
  ctrl.syncRows({1, 2, 3});
  mgr.tick(0.3f);  // all stable, progress 1
  // Now: 1 exits, 4 enters, 2/3 stable. Advance halfway.
  ctrl.syncRows({2, 3, 4});
  mgr.tick(0.1f);  // ~halfway through the 0.2s tweens
  // Expected: row1 ~0.5 (exiting), row4 ~0.5 (entering, eased), rows 2/3 = 1.

  const std::vector<std::int32_t> rowIds = {1, 2, 3, 4};  // instance order

  // --- opacity ---
  {
    auto inst = makeInstances(4, 10.0f);
    applyOpacityTransition(inst.data(), 4, kStride, kRgba, rowIds, ctrl);
    check(alpha(inst, 1) == 255 && alpha(inst, 2) == 255, "stable rows keep full alpha");
    check(alpha(inst, 0) > 0 && alpha(inst, 0) < 255, "exiting row faded (alpha < 255)");
    check(alpha(inst, 3) > 0 && alpha(inst, 3) < 255, "entering row faded in (alpha < 255)");
  }

  // --- size ---
  {
    auto inst = makeInstances(4, 10.0f);
    applyScaleTransition(inst.data(), 4, kStride, kSize, rowIds, ctrl);
    check(sizeOf(inst, 1) == 10.0f, "stable row keeps full size");
    check(sizeOf(inst, 0) > 0.0f && sizeOf(inst, 0) < 10.0f, "exiting row shrinks");
    check(sizeOf(inst, 3) > 0.0f && sizeOf(inst, 3) < 10.0f, "entering row grows from 0");
  }

  // --- untracked rows untouched (no transition controller entry) ---
  {
    AnimationManager m2;
    AnimationController empty(m2);  // tracks nothing
    auto inst = makeInstances(2, 7.0f);
    applyOpacityTransition(inst.data(), 2, kStride, kRgba, {99, 100}, empty);
    applyScaleTransition(inst.data(), 2, kStride, kSize, {99, 100}, empty);
    check(alpha(inst, 0) == 255 && alpha(inst, 1) == 255, "untracked rows keep alpha");
    check(sizeOf(inst, 0) == 7.0f, "untracked rows keep size");
  }

  // --- fully entered (progress 1) is a no-op ---
  {
    AnimationManager m3;
    AnimationController c3(m3, 0.1f, 0.1f);
    c3.syncRows({5});
    m3.tick(0.2f);  // row 5 stable, progress 1
    auto inst = makeInstances(1, 3.0f);
    applyOpacityTransition(inst.data(), 1, kStride, kRgba, {5}, c3);
    check(alpha(inst, 0) == 255, "fully-entered row: full alpha (no-op)");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
