// D49.1 — GestureRecognizer: pinch gesture (two touches moving apart, scale > 1)
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
  std::printf("=== D49.1 GestureRecognizer Pinch Tests ===\n");

  // Test 1: No touches -> None gesture
  {
    dc::GestureRecognizer gr;
    dc::GestureResult r = gr.processTouches(nullptr, 0);
    check(r.type == dc::GestureType::None, "no touches -> None");
  }

  // Test 2: Single touch -> None gesture
  {
    dc::GestureRecognizer gr;
    dc::TouchPoint tp{1, 100.0, 200.0, dc::TouchPhase::Began};
    dc::GestureResult r = gr.processTouches(&tp, 1);
    check(r.type == dc::GestureType::None, "single touch -> None");
  }

  // Test 3: Two touches began then moved apart -> pinch scale > 1
  {
    dc::GestureRecognizer gr;

    // Frame 1: two fingers touch down
    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    dc::GestureResult r1 = gr.processTouches(began, 2);
    // First frame with 2 touches establishes baseline, no gesture yet
    check(r1.type == dc::GestureType::None, "first two-touch frame -> None (baseline)");

    // Frame 2: fingers move apart (pinch out)
    dc::TouchPoint moved[2] = {
      {1,  50.0, 200.0, dc::TouchPhase::Moved},
      {2, 250.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r2 = gr.processTouches(moved, 2);
    check(r2.scale > 1.0, "pinch out: scale > 1.0");
    check(r2.type == dc::GestureType::Pinch, "pinch out: gesture type is Pinch");
  }

  // Test 4: Two touches moving closer -> pinch scale < 1
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1,  50.0, 200.0, dc::TouchPhase::Began},
      {2, 250.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    // Move closer (pinch in)
    dc::TouchPoint moved[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Moved},
      {2, 200.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    check(r.scale < 1.0, "pinch in: scale < 1.0");
  }

  // Test 5: Center position is midpoint of two touches
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 100.0, dc::TouchPhase::Began},
      {2, 300.0, 300.0, dc::TouchPhase::Began}
    };
    dc::GestureResult r = gr.processTouches(began, 2);
    check(std::fabs(r.centerX - 200.0) < 1e-6, "center X = midpoint");
    check(std::fabs(r.centerY - 200.0) < 1e-6, "center Y = midpoint");
  }

  // Test 6: Reset clears state
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    gr.reset();

    // After reset, should behave like fresh start
    dc::GestureResult r = gr.processTouches(began, 2);
    check(r.type == dc::GestureType::None, "after reset, first frame -> None");
  }

  // Test 7: Touch ended reduces to < 2 active -> None
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    // End one touch
    dc::TouchPoint ended{1, 100.0, 200.0, dc::TouchPhase::Ended};
    dc::GestureResult r = gr.processTouches(&ended, 1);
    check(r.type == dc::GestureType::None, "one touch ended -> None");
  }

  // Test 8: Disable pinch -> even spreading fingers gives TwoFingerPan
  {
    dc::GestureRecognizer gr;
    gr.setEnabled(dc::GestureType::Pinch, false);

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint moved[2] = {
      {1,  50.0, 200.0, dc::TouchPhase::Moved},
      {2, 250.0, 200.0, dc::TouchPhase::Moved}
    };
    dc::GestureResult r = gr.processTouches(moved, 2);
    // With pinch disabled, center doesn't move so pan delta is ~0; gesture classification may vary
    // But it should NOT be Pinch
    check(r.type != dc::GestureType::Pinch, "pinch disabled: type != Pinch");
  }

  // Test 9: Cancelled touch is same as ended
  {
    dc::GestureRecognizer gr;

    dc::TouchPoint began[2] = {
      {1, 100.0, 200.0, dc::TouchPhase::Began},
      {2, 200.0, 200.0, dc::TouchPhase::Began}
    };
    gr.processTouches(began, 2);

    dc::TouchPoint cancelled{2, 200.0, 200.0, dc::TouchPhase::Cancelled};
    dc::GestureResult r = gr.processTouches(&cancelled, 1);
    check(r.type == dc::GestureType::None, "cancelled touch -> None (< 2 active)");
  }

  std::printf("=== D49.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
