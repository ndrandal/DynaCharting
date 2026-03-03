// D35.2 — DragDrop: custom threshold, diagonal distance, sub-threshold = no drag
#include "dc/interaction/DragDropState.hpp"

#include <cstdio>
#include <cmath>

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

int main() {
  std::printf("=== D35.2 DragDrop Threshold Tests ===\n");

  dc::DragDropState dds;

  // Test 1: Default threshold is 5px
  check(dds.threshold() == 5.0, "default threshold is 5px");

  // Test 2: Custom threshold
  dds.setThreshold(10.0);
  check(dds.threshold() == 10.0, "custom threshold set");

  // Test 3: Diagonal distance check
  // Start at (100, 100), threshold = 10
  // Move diagonally: (107, 107) -> distance = sqrt(49+49) ≈ 9.9 < 10
  {
    dds.beginDetect(1, 0, 100.0, 100.0, 0.0, 0.0);
    bool started = dds.update(107.0, 107.0, 7.0, 7.0);
    check(!started, "diagonal 9.9px < 10px threshold: no drag");
    check(dds.phase() == dc::DragDropPhase::Detecting, "still Detecting at 9.9px");

    // Move to (108, 108) -> distance = sqrt(64+64) ≈ 11.3 > 10
    started = dds.update(108.0, 108.0, 8.0, 8.0);
    check(started, "diagonal 11.3px > 10px threshold: drag started");
    check(dds.phase() == dc::DragDropPhase::Dragging, "now Dragging");
    dds.cancel();
  }

  // Test 4: Very small threshold (1px)
  {
    dds.setThreshold(1.0);
    dds.beginDetect(1, 0, 50.0, 50.0, 0.0, 0.0);
    bool started = dds.update(50.5, 50.5, 0.0, 0.0);
    check(!started, "0.7px < 1px: no drag");

    started = dds.update(51.0, 50.0, 0.0, 0.0);
    check(started, "1px horizontal: drag started");
    dds.cancel();
  }

  // Test 5: Zero threshold (immediate drag)
  {
    dds.setThreshold(0.0);
    dds.beginDetect(1, 0, 50.0, 50.0, 0.0, 0.0);
    // Any movement should trigger
    bool started = dds.update(50.0, 50.0, 0.0, 0.0);
    check(started, "zero threshold: any update starts drag");
    dds.cancel();
  }

  // Test 6: Only X movement
  {
    dds.setThreshold(5.0);
    dds.beginDetect(1, 0, 0.0, 0.0, 0.0, 0.0);
    bool started = dds.update(4.0, 0.0, 0.0, 0.0);
    check(!started, "4px horizontal < 5px: no drag");
    started = dds.update(6.0, 0.0, 0.0, 0.0);
    check(started, "6px horizontal > 5px: drag");
    dds.cancel();
  }

  // Test 7: Only Y movement
  {
    dds.beginDetect(1, 0, 0.0, 0.0, 0.0, 0.0);
    bool started = dds.update(0.0, 4.0, 0.0, 0.0);
    check(!started, "4px vertical < 5px: no drag");
    started = dds.update(0.0, 6.0, 0.0, 0.0);
    check(started, "6px vertical > 5px: drag");
    dds.cancel();
  }

  // Test 8: Large threshold
  {
    dds.setThreshold(100.0);
    dds.beginDetect(1, 0, 0.0, 0.0, 0.0, 0.0);
    bool started = dds.update(70.0, 70.0, 0.0, 0.0);
    check(!started, "99px < 100px: no drag");
    started = dds.update(71.0, 71.0, 0.0, 0.0);
    double dist = std::sqrt(71.0 * 71.0 + 71.0 * 71.0);
    check(started == (dist >= 100.0), "100.4px >= 100px: drag");
    dds.cancel();
  }

  std::printf("=== D35.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
