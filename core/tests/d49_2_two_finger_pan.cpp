// D49.2 — GestureRecognizer: two-finger pan (both touches move same direction)
#include "dc/viewport/GestureRecognizer.hpp"

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

int main() {
  std::printf("=== D49.2 Two-Finger Pan Tests ===\n");

  // Test 1: Two fingers move right by same amount -> pan right
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    // Both fingers move right by 50px (same direction, same distance -> pure pan)
    dc::TouchPoint moved[2] = {
      {1, 150.0, 200.0, dc::TouchPhase::Moved},
      {2, 250.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(r.type == dc::GestureType::TwoFingerPan, "same-direction move -> TwoFingerPan");
    check(std::fabs(r.panDeltaX - 50.0) < 1e-6, "panDeltaX = 50");
    check(std::fabs(r.panDeltaY) < 1e-6, "panDeltaY = 0");
  }

  // Test 2: Two fingers move down -> pan down
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 100.0, dc::TouchPhase::Began},
      {2, 200.0, 100.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1, 100.0, 170.0, dc::TouchPhase::Moved},
      {2, 200.0, 170.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(r.type == dc::GestureType::TwoFingerPan, "vertical move -> TwoFingerPan");
    check(std::fabs(r.panDeltaX) < 1e-6, "panDeltaX = 0 for vertical pan");
    check(std::fabs(r.panDeltaY - 70.0) < 1e-6, "panDeltaY = 70");
  }

  // Test 3: Diagonal pan
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 100.0, dc::TouchPhase::Began},
      {2, 200.0, 100.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1, 130.0, 140.0, dc::TouchPhase::Moved},
      {2, 230.0, 140.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(r.type == dc::GestureType::TwoFingerPan, "diagonal move -> TwoFingerPan");
    check(std::fabs(r.panDeltaX - 30.0) < 1e-6, "panDeltaX = 30");
    check(std::fabs(r.panDeltaY - 40.0) < 1e-6, "panDeltaY = 40");
  }

  // Test 4: Scale should be ~1.0 for pure pan (same distance maintained)
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1, 150.0, 200.0, dc::TouchPhase::Moved},
      {2, 250.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(std::fabs(r.scale - 1.0) < 1e-6, "pure pan: scale ~= 1.0");
  }

  // Test 5: Multi-frame pan accumulation
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 100.0, dc::TouchPhase::Began},
      {2, 200.0, 100.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    // Frame 1: move right 20
    dc::TouchPoint m1[2] = {
      {1, 120.0, 100.0, dc::TouchPhase::Moved},
      {2, 220.0, 100.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r1 = gr.processTouches(m1, 2);
    check(std::fabs(r1.panDeltaX - 20.0) < 1e-6, "frame1 panDeltaX = 20");

    // Frame 2: move right another 30
    dc::TouchPoint m2[2] = {
      {1, 150.0, 100.0, dc::TouchPhase::Moved},
      {2, 250.0, 100.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r2 = gr.processTouches(m2, 2);
    check(std::fabs(r2.panDeltaX - 30.0) < 1e-6, "frame2 panDeltaX = 30 (delta, not cumulative)");
  }

  // Test 6: Pan with TwoFingerPan disabled -> None or other type
  {
    dc::GestureRecognizer gr;
    gr.setEnabled(dc::GestureType::TwoFingerPan, false);

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1, 150.0, 200.0, dc::TouchPhase::Moved},
      {2, 250.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(r.type != dc::GestureType::TwoFingerPan, "pan disabled: type != TwoFingerPan");
    // pan deltas are still reported as values even if type is not Pan
    check(std::fabs(r.panDeltaX - 50.0) < 1e-6, "pan disabled: panDeltaX still computed");
  }

  // Test 7: Center tracks correctly during pan
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 100.0, dc::TouchPhase::Began},
      {2, 200.0, 100.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1, 200.0, 200.0, dc::TouchPhase::Moved},
      {2, 300.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(std::fabs(r.centerX - 250.0) < 1e-6, "center X after pan = 250");
    check(std::fabs(r.centerY - 200.0) < 1e-6, "center Y after pan = 200");
  }

  std::printf("=== D49.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
