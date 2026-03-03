// D27.2 — Tween & AnimationManager test
#include "dc/anim/AnimationManager.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

static bool near(float a, float b, float eps = 1e-4f) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D27.2 Tween & AnimationManager ===\n");

  // Test 1: Basic tween lifecycle
  {
    dc::AnimationManager mgr;
    float lastValue = -1.0f;
    bool completed = false;

    dc::Tween tw;
    tw.from = 0.0f;
    tw.to = 100.0f;
    tw.duration = 1.0f;
    tw.easing = dc::EasingType::Linear;
    tw.onUpdate = [&](float v) { lastValue = v; };
    tw.onComplete = [&]() { completed = true; };

    auto id = mgr.addTween(std::move(tw));
    check(mgr.activeCount() == 1, "1 active tween after add");
    check(mgr.isActive(id), "tween is active");

    // Tick halfway
    mgr.tick(0.5f);
    check(near(lastValue, 50.0f), "halfway value ~50");
    check(!completed, "not completed at 0.5s");

    // Tick to completion
    mgr.tick(0.5f);
    check(near(lastValue, 100.0f), "final value ~100");
    check(completed, "onComplete fired");
    check(mgr.activeCount() == 0, "tween removed after completion");
    check(!mgr.isActive(id), "tween no longer active");
  }

  // Test 2: Multiple tweens
  {
    dc::AnimationManager mgr;
    float v1 = 0.0f, v2 = 0.0f;

    dc::Tween tw1;
    tw1.from = 0.0f; tw1.to = 10.0f; tw1.duration = 1.0f;
    tw1.easing = dc::EasingType::Linear;
    tw1.onUpdate = [&](float v) { v1 = v; };

    dc::Tween tw2;
    tw2.from = 100.0f; tw2.to = 200.0f; tw2.duration = 2.0f;
    tw2.easing = dc::EasingType::Linear;
    tw2.onUpdate = [&](float v) { v2 = v; };

    mgr.addTween(std::move(tw1));
    mgr.addTween(std::move(tw2));
    check(mgr.activeCount() == 2, "2 active tweens");

    mgr.tick(1.0f);
    check(near(v1, 10.0f), "tw1 completed at 1s");
    check(near(v2, 150.0f), "tw2 halfway at 1s");
    check(mgr.activeCount() == 1, "1 active after tw1 completes");

    mgr.tick(1.0f);
    check(near(v2, 200.0f), "tw2 completed at 2s");
    check(mgr.activeCount() == 0, "0 active after both complete");
  }

  // Test 3: Cancel tween
  {
    dc::AnimationManager mgr;
    float v = 0.0f;

    dc::Tween tw;
    tw.from = 0.0f; tw.to = 100.0f; tw.duration = 1.0f;
    tw.easing = dc::EasingType::Linear;
    tw.onUpdate = [&](float val) { v = val; };

    auto id = mgr.addTween(std::move(tw));
    mgr.tick(0.25f);
    check(near(v, 25.0f), "value at 0.25s before cancel");

    bool cancelled = mgr.cancel(id);
    check(cancelled, "cancel returns true");
    check(mgr.activeCount() == 0, "0 active after cancel");

    // Value stays at last update
    float frozenV = v;
    mgr.tick(0.5f);
    check(near(v, frozenV), "value frozen after cancel");
  }

  // Test 4: Cancel nonexistent ID
  {
    dc::AnimationManager mgr;
    check(!mgr.cancel(999), "cancel nonexistent returns false");
  }

  // Test 5: cancelAll
  {
    dc::AnimationManager mgr;

    dc::Tween tw1; tw1.duration = 1.0f;
    dc::Tween tw2; tw2.duration = 1.0f;
    mgr.addTween(std::move(tw1));
    mgr.addTween(std::move(tw2));
    check(mgr.activeCount() == 2, "2 tweens before cancelAll");

    mgr.cancelAll();
    check(mgr.activeCount() == 0, "0 tweens after cancelAll");
  }

  // Test 6: Zero-duration tween completes immediately
  {
    dc::AnimationManager mgr;
    float v = -1.0f;
    bool completed = false;

    dc::Tween tw;
    tw.from = 0.0f; tw.to = 42.0f; tw.duration = 0.0f;
    tw.easing = dc::EasingType::Linear;
    tw.onUpdate = [&](float val) { v = val; };
    tw.onComplete = [&]() { completed = true; };

    mgr.addTween(std::move(tw));
    mgr.tick(0.001f);
    check(near(v, 42.0f), "zero-duration: final value immediately");
    check(completed, "zero-duration: onComplete fired");
    check(mgr.activeCount() == 0, "zero-duration: removed immediately");
  }

  // Test 7: Easing is applied (not just linear)
  {
    dc::AnimationManager mgr;
    float v = 0.0f;

    dc::Tween tw;
    tw.from = 0.0f; tw.to = 1.0f; tw.duration = 1.0f;
    tw.easing = dc::EasingType::EaseInQuad;
    tw.onUpdate = [&](float val) { v = val; };

    mgr.addTween(std::move(tw));
    mgr.tick(0.5f);
    // EaseInQuad at t=0.5: 0.5^2 = 0.25
    check(near(v, 0.25f), "EaseInQuad applied: 0.25 at t=0.5");
  }

  // Test 8: Callback receives clamped final value
  {
    dc::AnimationManager mgr;
    std::vector<float> values;

    dc::Tween tw;
    tw.from = 0.0f; tw.to = 1.0f; tw.duration = 0.5f;
    tw.easing = dc::EasingType::Linear;
    tw.onUpdate = [&](float val) { values.push_back(val); };

    mgr.addTween(std::move(tw));
    // Overshoot the duration in one tick
    mgr.tick(2.0f);
    check(!values.empty(), "callback was called");
    check(near(values.back(), 1.0f), "final callback value is exactly 1.0");
  }

  std::printf("=== D27.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
