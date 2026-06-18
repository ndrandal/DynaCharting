// ENC-636 (E2) — AnimationController: row-id diff -> enter/exit/update tweens with
// object constancy (durable RowIdentity keys).
#include "dc/anim/AnimationController.hpp"
#include "dc/anim/AnimationManager.hpp"

#include <cstdio>

static int passed = 0, failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}
using namespace dc;
using Phase = AnimationController::Phase;

int main() {
  std::printf("=== ENC-636 (E2) AnimationController ===\n");

  AnimationManager mgr;
  AnimationController ctrl(mgr, /*enter=*/0.2f, /*exit=*/0.2f);

  // Initial rows all ENTER from 0.
  ctrl.syncRows({1, 2, 3});
  check(ctrl.trackedCount() == 3, "3 rows tracked");
  check(ctrl.phaseOf(1) == Phase::Enter, "row 1 entering");
  check(ctrl.progressOf(1) == 0.0f, "row 1 progress starts at 0");

  // Advance: progress rises mid-flight.
  mgr.tick(0.1f);
  check(ctrl.progressOf(1) > 0.0f && ctrl.progressOf(1) < 1.0f, "row 1 progress rising");

  // Complete the enter.
  mgr.tick(0.2f);
  check(ctrl.progressOf(2) == 1.0f && ctrl.phaseOf(2) == Phase::Stable,
        "enter completes -> progress 1, Stable");

  // New set: 1 leaves, 4 arrives, 2/3 stay.
  ctrl.syncRows({2, 3, 4});
  check(ctrl.phaseOf(1) == Phase::Exit, "row 1 now Exit");
  check(ctrl.phaseOf(4) == Phase::Enter, "row 4 Enter");
  check(ctrl.phaseOf(2) == Phase::Stable && ctrl.progressOf(2) == 1.0f, "row 2 stays Stable");
  check(ctrl.trackedCount() == 4, "4 tracked (row 1 still fading out)");

  // Mid exit/enter.
  mgr.tick(0.1f);
  check(ctrl.progressOf(1) < 1.0f && ctrl.progressOf(1) > 0.0f, "row 1 fading out");
  check(ctrl.progressOf(4) > 0.0f && ctrl.progressOf(4) < 1.0f, "row 4 fading in");

  // Finish: row 1 fully exits (dropped); row 4 stable.
  mgr.tick(0.2f);
  check(!ctrl.isTracked(1), "row 1 dropped after exit completes");
  check(ctrl.trackedCount() == 3, "3 tracked again");
  check(ctrl.progressOf(4) == 1.0f && ctrl.phaseOf(4) == Phase::Stable, "row 4 Stable");

  // Re-enter: row 3 starts exiting, then comes back before it finishes.
  ctrl.syncRows({2, 4});
  check(ctrl.phaseOf(3) == Phase::Exit, "row 3 exiting");
  mgr.tick(0.1f);
  const float fading = ctrl.progressOf(3);
  check(fading < 1.0f, "row 3 partway out");
  ctrl.syncRows({2, 3, 4});  // row 3 returns
  check(ctrl.phaseOf(3) == Phase::Enter, "row 3 re-enters (reversed)");
  mgr.tick(0.3f);
  check(ctrl.isTracked(3) && ctrl.progressOf(3) == 1.0f, "row 3 recovered to Stable (object constancy)");

  // Idempotent: same set, no churn.
  const std::size_t before = ctrl.trackedCount();
  ctrl.syncRows({2, 3, 4});
  check(ctrl.trackedCount() == before, "unchanged set is idempotent");

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
