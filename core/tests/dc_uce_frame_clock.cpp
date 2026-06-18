// ENC-635 (E1) — FrameClock: drives the (previously unwired) AnimationManager(s)
// per frame and reports whether to keep rendering.
#include "dc/anim/AnimationManager.hpp"
#include "dc/anim/FrameClock.hpp"
#include "dc/anim/Tween.hpp"

#include <cstdio>

static int passed = 0, failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
using namespace dc;

int main() {
  std::printf("=== ENC-635 (E1) FrameClock ===\n");

  FrameClock clock;
  AnimationManager mgr;
  clock.addManager(&mgr);
  check(clock.managerCount() == 1, "manager registered");
  clock.addManager(&mgr);  // idempotent
  check(clock.managerCount() == 1, "idempotent: still 1 manager");

  // No animations -> ticking advances time but reports not-animating (host idles).
  check(!clock.tick(0.016f), "tick with no tweens -> not animating");
  check(clock.frame() == 1, "frame counter advanced");

  // Add a tween 0->1 over 0.3s; capture progress.
  float last = -1.0f;
  bool done = false;
  Tween t;
  t.from = 0.0f; t.to = 1.0f; t.duration = 0.3f;
  t.onUpdate = [&](float v) { last = v; };
  t.onComplete = [&]() { done = true; };
  mgr.addTween(t);

  check(clock.tick(0.1f), "tick advances tween -> animating");
  check(last > 0.0f && last < 1.0f, "onUpdate fired mid-flight (0<v<1)");
  check(clock.isAnimating(), "isAnimating true while tween runs");
  const float mid = last;

  clock.tick(0.1f);
  check(last > mid, "value progresses monotonically");

  // Finish it off (total >= 0.3s).
  clock.tick(0.2f);
  check(done, "onComplete fired");
  check(!clock.isAnimating(), "isAnimating false after completion (host can idle)");
  check(mgr.activeCount() == 0, "no active tweens left");
  check(clock.elapsedSeconds() > 0.39 && clock.elapsedSeconds() < 0.42, "elapsed ~0.4s");
  check(clock.frame() == 4, "4 ticks counted");

  // removeManager detaches.
  clock.removeManager(&mgr);
  check(clock.managerCount() == 0, "removeManager detaches");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
