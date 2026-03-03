// D35.1 — DragDrop: full lifecycle detect → drag → drop, cancel from each phase
#include "dc/interaction/DragDropState.hpp"

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

int main() {
  std::printf("=== D35.1 DragDrop Tests ===\n");

  dc::DragDropState dds;

  // Test 1: Initial state is Idle
  check(dds.phase() == dc::DragDropPhase::Idle, "initial phase is Idle");

  // Test 2: Full lifecycle: detect → drag → drop
  {
    dds.beginDetect(42, 7, 100.0, 200.0, 10.0, 20.0);
    check(dds.phase() == dc::DragDropPhase::Detecting, "phase is Detecting after beginDetect");
    check(dds.payload().sourceDrawItemId == 42, "payload sourceDrawItemId");
    check(dds.payload().recordIndex == 7, "payload recordIndex");
    check(dds.payload().startDataX == 10.0, "payload startDataX");
    check(dds.payload().startDataY == 20.0, "payload startDataY");

    // Sub-threshold update: still Detecting
    bool started = dds.update(102.0, 202.0, 10.2, 20.2);
    check(!started, "sub-threshold update doesn't start drag");
    check(dds.phase() == dc::DragDropPhase::Detecting, "still Detecting");

    // Beyond threshold: starts dragging
    started = dds.update(200.0, 300.0, 20.0, 30.0);
    check(started, "beyond-threshold update starts drag");
    check(dds.phase() == dc::DragDropPhase::Dragging, "phase is Dragging");

    // Drop
    auto result = dds.drop(99, 25.0, 35.0);
    check(dds.phase() == dc::DragDropPhase::Dropped, "phase is Dropped");
    check(result.accepted, "drop accepted (was dragging)");
    check(result.payload.sourceDrawItemId == 42, "drop payload preserved");
    check(result.targetDrawItemId == 99, "drop targetDrawItemId");
    check(result.dropDataX == 25.0, "drop data X");
    check(result.dropDataY == 35.0, "drop data Y");
  }

  // Test 3: Cancel from Detecting phase
  {
    dds.cancel();
    check(dds.phase() == dc::DragDropPhase::Idle, "cancel resets to Idle");

    dds.beginDetect(1, 0, 50.0, 50.0, 5.0, 5.0);
    check(dds.phase() == dc::DragDropPhase::Detecting, "Detecting again");
    dds.cancel();
    check(dds.phase() == dc::DragDropPhase::Idle, "cancel from Detecting -> Idle");
  }

  // Test 4: Cancel from Dragging phase
  {
    dds.beginDetect(1, 0, 0.0, 0.0, 0.0, 0.0);
    dds.update(100.0, 100.0, 50.0, 50.0); // beyond threshold
    check(dds.phase() == dc::DragDropPhase::Dragging, "Dragging");
    dds.cancel();
    check(dds.phase() == dc::DragDropPhase::Idle, "cancel from Dragging -> Idle");
  }

  // Test 5: Drop from Detecting (not yet dragging) -> not accepted
  {
    dds.beginDetect(1, 0, 0.0, 0.0, 0.0, 0.0);
    auto result = dds.drop(10, 1.0, 1.0);
    check(!result.accepted, "drop from Detecting is not accepted");
    check(dds.phase() == dc::DragDropPhase::Dropped, "phase is Dropped after failed drop");
  }

  std::printf("=== D35.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
