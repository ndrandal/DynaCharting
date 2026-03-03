// D27.3 — ViewportAnimator test (smooth viewport transitions)
#include "dc/anim/ViewportAnimator.hpp"
#include "dc/anim/AnimationManager.hpp"
#include "dc/viewport/Viewport.hpp"

#include <cmath>
#include <cstdio>

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

static bool nearD(double a, double b, double eps = 1e-3) {
  return std::fabs(a - b) < eps;
}

int main() {
  std::printf("=== D27.3 ViewportAnimator ===\n");

  // Test 1: Basic viewport animation
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, 0.0, 50.0);

    anim.animateTo(vp, 50.0, 150.0, 10.0, 60.0, 1.0f, dc::EasingType::Linear);
    check(anim.isAnimating(), "animation started");

    // Tick halfway (linear: t=0.5 → values halfway)
    mgr.tick(0.5f);
    auto dr = vp.dataRange();
    check(nearD(dr.xMin, 25.0), "xMin halfway: 25");
    check(nearD(dr.xMax, 125.0), "xMax halfway: 125");
    check(nearD(dr.yMin, 5.0), "yMin halfway: 5");
    check(nearD(dr.yMax, 55.0), "yMax halfway: 55");

    // Tick to completion
    mgr.tick(0.5f);
    dr = vp.dataRange();
    check(nearD(dr.xMin, 50.0), "xMin final: 50");
    check(nearD(dr.xMax, 150.0), "xMax final: 150");
    check(nearD(dr.yMin, 10.0), "yMin final: 10");
    check(nearD(dr.yMax, 60.0), "yMax final: 60");
    check(!anim.isAnimating(), "animation completed");
  }

  // Test 2: animateToX only changes X
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, -10.0, 10.0);

    anim.animateToX(vp, 200.0, 300.0, 1.0f, dc::EasingType::Linear);
    mgr.tick(1.0f);
    auto dr = vp.dataRange();
    check(nearD(dr.xMin, 200.0), "animateToX: xMin=200");
    check(nearD(dr.xMax, 300.0), "animateToX: xMax=300");
    check(nearD(dr.yMin, -10.0), "animateToX: yMin unchanged");
    check(nearD(dr.yMax, 10.0), "animateToX: yMax unchanged");
  }

  // Test 3: animateToY only changes Y
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, -10.0, 10.0);

    anim.animateToY(vp, -50.0, 50.0, 1.0f, dc::EasingType::Linear);
    mgr.tick(1.0f);
    auto dr = vp.dataRange();
    check(nearD(dr.xMin, 0.0), "animateToY: xMin unchanged");
    check(nearD(dr.xMax, 100.0), "animateToY: xMax unchanged");
    check(nearD(dr.yMin, -50.0), "animateToY: yMin=-50");
    check(nearD(dr.yMax, 50.0), "animateToY: yMax=50");
  }

  // Test 4: New animation cancels previous
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, 0.0, 100.0);

    // Start first animation
    anim.animateTo(vp, 50.0, 150.0, 50.0, 150.0, 2.0f, dc::EasingType::Linear);
    mgr.tick(0.5f); // 25% of first animation

    // Start second animation (should cancel first)
    anim.animateTo(vp, 200.0, 300.0, 200.0, 300.0, 1.0f, dc::EasingType::Linear);
    check(mgr.activeCount() == 1, "only 1 active tween after restart");

    mgr.tick(1.0f);
    auto dr = vp.dataRange();
    check(nearD(dr.xMin, 200.0), "second animation completes: xMin=200");
    check(nearD(dr.xMax, 300.0), "second animation completes: xMax=300");
  }

  // Test 5: Cancel stops animation
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, 0.0, 100.0);

    anim.animateTo(vp, 200.0, 300.0, 200.0, 300.0, 1.0f, dc::EasingType::Linear);
    mgr.tick(0.5f); // halfway
    auto drMid = vp.dataRange();

    anim.cancel();
    check(!anim.isAnimating(), "not animating after cancel");

    mgr.tick(0.5f); // nothing should change
    auto drAfter = vp.dataRange();
    check(nearD(drMid.xMin, drAfter.xMin), "xMin frozen after cancel");
    check(nearD(drMid.xMax, drAfter.xMax), "xMax frozen after cancel");
  }

  // Test 6: Easing is applied to viewport transition
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 100.0, 0.0, 100.0);

    anim.animateTo(vp, 100.0, 200.0, 100.0, 200.0, 1.0f, dc::EasingType::EaseInQuad);
    mgr.tick(0.5f);
    auto dr = vp.dataRange();
    // EaseInQuad at t=0.5: eased = 0.25, so xMin = 0 + (100-0)*0.25 = 25
    check(nearD(dr.xMin, 25.0), "EaseInQuad: xMin=25 at t=0.5");
    check(nearD(dr.xMax, 125.0), "EaseInQuad: xMax=125 at t=0.5");
  }

  // Test 7: Not animating when no animation started
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    check(!anim.isAnimating(), "not animating initially");
  }

  // Test 8: Multiple small ticks converge correctly
  {
    dc::AnimationManager mgr;
    dc::ViewportAnimator anim(mgr);
    dc::Viewport vp;
    vp.setDataRange(0.0, 10.0, 0.0, 10.0);

    anim.animateTo(vp, 10.0, 20.0, 10.0, 20.0, 1.0f, dc::EasingType::Linear);

    // 20 ticks of 50ms each = 1 second total
    for (int i = 0; i < 20; i++) {
      mgr.tick(0.05f);
    }

    auto dr = vp.dataRange();
    check(nearD(dr.xMin, 10.0), "20 small ticks: xMin converges to 10");
    check(nearD(dr.xMax, 20.0), "20 small ticks: xMax converges to 20");
    check(!anim.isAnimating(), "animation complete after 20 ticks");
  }

  std::printf("=== D27.3 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
