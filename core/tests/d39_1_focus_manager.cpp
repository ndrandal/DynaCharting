// D39.1 — FocusManager: tab cycling (wraps), set/clear focus, remove while focused
#include "dc/interaction/FocusManager.hpp"

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
  std::printf("=== D39.1 FocusManager Tests ===\n");

  dc::FocusManager fm;

  // Test 1: Initially no focus
  check(fm.focusedId() == 0, "initially no focus");
  check(fm.count() == 0, "initially empty");

  // Test 2: Add focusables
  fm.addFocusable({10, 0, 1});
  fm.addFocusable({20, 1, 1});
  fm.addFocusable({30, 2, 2});
  check(fm.count() == 3, "3 focusables added");

  // Test 3: setFocus
  fm.setFocus(20);
  check(fm.focusedId() == 20, "setFocus(20) works");

  // Test 4: setFocus to non-existent does nothing
  fm.setFocus(999);
  check(fm.focusedId() == 20, "setFocus non-existent -> unchanged");

  // Test 5: clearFocus
  fm.clearFocus();
  check(fm.focusedId() == 0, "clearFocus resets to 0");

  // Test 6: focusNext from no focus -> first item
  fm.focusNext();
  check(fm.focusedId() == 10, "focusNext from none -> first (tabOrder=0)");

  // Test 7: focusNext cycles
  fm.focusNext(); // -> 20
  check(fm.focusedId() == 20, "focusNext -> 20");
  fm.focusNext(); // -> 30
  check(fm.focusedId() == 30, "focusNext -> 30");
  fm.focusNext(); // -> wraps to 10
  check(fm.focusedId() == 10, "focusNext wraps -> 10");

  // Test 8: focusPrevious
  fm.focusPrevious(); // wraps to 30
  check(fm.focusedId() == 30, "focusPrevious wraps -> 30");
  fm.focusPrevious(); // -> 20
  check(fm.focusedId() == 20, "focusPrevious -> 20");

  // Test 9: focusPrevious from no focus -> last item
  fm.clearFocus();
  fm.focusPrevious();
  check(fm.focusedId() == 30, "focusPrevious from none -> last");

  // Test 10: Remove focused item clears focus
  fm.setFocus(20);
  check(fm.focusedId() == 20, "focused on 20");
  fm.removeFocusable(20);
  check(fm.focusedId() == 0, "removing focused item clears focus");
  check(fm.count() == 2, "count is 2 after remove");

  // Test 11: Tab cycling after remove
  fm.focusNext();
  check(fm.focusedId() == 10, "focusNext after remove -> 10");
  fm.focusNext();
  check(fm.focusedId() == 30, "focusNext -> 30 (20 removed)");

  // Test 12: Remove non-existent is safe
  fm.removeFocusable(999);
  check(fm.count() == 2, "remove non-existent is safe");

  // Test 13: Add duplicate replaces
  fm.addFocusable({10, 5, 3}); // update tabOrder
  check(fm.count() == 2, "add duplicate doesn't increase count");
  // Item 10 should now have tabOrder=5, so it sorts after 30 (tabOrder=2)
  fm.clearFocus();
  fm.focusNext();
  check(fm.focusedId() == 30, "after re-sort, 30 (tabOrder=2) is first");
  fm.focusNext();
  check(fm.focusedId() == 10, "10 (tabOrder=5) is second");

  std::printf("=== D39.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
