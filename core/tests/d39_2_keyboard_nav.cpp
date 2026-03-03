// D39.2 — Keyboard navigation: grid group, processKey dispatch, Escape clears
#include "dc/interaction/FocusManager.hpp"
#include "dc/viewport/InputState.hpp"

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
  std::printf("=== D39.2 Keyboard Navigation Tests ===\n");

  dc::FocusManager fm;

  // Setup a 2x3 grid:
  // Group 1: items 10, 20, 30 (tabOrder 0, 1, 2)
  // Group 2: items 40, 50, 60 (tabOrder 3, 4, 5)
  fm.addFocusable({10, 0, 1});
  fm.addFocusable({20, 1, 1});
  fm.addFocusable({30, 2, 1});
  fm.addFocusable({40, 3, 2});
  fm.addFocusable({50, 4, 2});
  fm.addFocusable({60, 5, 2});

  // Test 1: Left/Right navigation within group
  fm.setFocus(20);
  fm.navigateLeft();
  check(fm.focusedId() == 10, "navigateLeft in group 1: 20->10");

  fm.navigateRight();
  check(fm.focusedId() == 20, "navigateRight in group 1: 10->20");

  fm.navigateRight();
  check(fm.focusedId() == 30, "navigateRight in group 1: 20->30");

  // Test 2: Left wraps within group
  fm.setFocus(10);
  fm.navigateLeft();
  check(fm.focusedId() == 30, "navigateLeft wraps: 10->30");

  // Test 3: Right wraps within group
  fm.setFocus(30);
  fm.navigateRight();
  check(fm.focusedId() == 10, "navigateRight wraps: 30->10");

  // Test 4: Down navigates to different group
  fm.setFocus(20);
  fm.navigateDown();
  check(fm.focusedId() == 40, "navigateDown: group 1 -> group 2 (first in group 2)");

  // Test 5: Up navigates back
  fm.navigateUp();
  check(fm.focusedId() == 30, "navigateUp: group 2 -> group 1 (last in group 1)");

  // Test 6: Up at top group -> no change (no group above)
  fm.setFocus(10);
  fm.navigateUp();
  check(fm.focusedId() == 10, "navigateUp at top group: no change");

  // Test 7: Down at bottom group -> no change
  fm.setFocus(60);
  fm.navigateDown();
  check(fm.focusedId() == 60, "navigateDown at bottom group: no change");

  // Test 8: processKey(Tab) cycles focus
  fm.setFocus(10);
  bool handled = fm.processKey(dc::KeyCode::Tab);
  check(handled, "Tab is handled");
  check(fm.focusedId() == 20, "Tab cycles to next");

  // Test 9: processKey(Escape) clears focus
  handled = fm.processKey(dc::KeyCode::Escape);
  check(handled, "Escape is handled");
  check(fm.focusedId() == 0, "Escape clears focus");

  // Test 10: processKey(Left/Right/Up/Down)
  fm.setFocus(50);
  fm.processKey(dc::KeyCode::Left);
  check(fm.focusedId() == 40, "KeyCode::Left navigates left");

  fm.processKey(dc::KeyCode::Right);
  check(fm.focusedId() == 50, "KeyCode::Right navigates right");

  fm.processKey(dc::KeyCode::Up);
  check(fm.focusedId() == 30, "KeyCode::Up navigates up");

  fm.processKey(dc::KeyCode::Down);
  check(fm.focusedId() == 40, "KeyCode::Down navigates down");

  // Test 11: processKey with unhandled key returns false
  handled = fm.processKey(dc::KeyCode::None);
  check(!handled, "None key not handled");

  // Test 12: Navigation with no focus does nothing
  fm.clearFocus();
  fm.navigateLeft();
  check(fm.focusedId() == 0, "navigateLeft with no focus: stays 0");
  fm.navigateDown();
  check(fm.focusedId() == 0, "navigateDown with no focus: stays 0");

  std::printf("=== D39.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
