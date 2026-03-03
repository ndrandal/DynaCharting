// D71.1 — HoverManager: hover state tracking, enter/exit callbacks, clear
#include "dc/interaction/HoverManager.hpp"

#include <cstdio>
#include <cstdint>

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
  std::printf("=== D71.1 HoverManager Tests ===\n");

  // Test 1: Initial state — not hovering
  {
    dc::HoverManager hm;
    check(!hm.isHovering(), "initial: not hovering");
    check(hm.hoveredDrawItemId() == 0, "initial: hoveredDrawItemId is 0");
    check(hm.hoverDataX() == 0.0, "initial: hoverDataX is 0");
    check(hm.hoverDataY() == 0.0, "initial: hoverDataY is 0");
  }

  // Test 2: Update with non-zero drawItemId starts hovering
  {
    dc::HoverManager hm;
    hm.update(42, 10.0, 20.0, 100.0, 200.0);
    check(hm.isHovering(), "update: is hovering");
    check(hm.hoveredDrawItemId() == 42, "update: hoveredDrawItemId is 42");
    check(hm.hoverDataX() == 10.0, "update: hoverDataX is 10");
    check(hm.hoverDataY() == 20.0, "update: hoverDataY is 20");
  }

  // Test 3: Update with drawItemId 0 means not hovering
  {
    dc::HoverManager hm;
    hm.update(0, 5.0, 5.0, 50.0, 50.0);
    check(!hm.isHovering(), "update 0: not hovering");
    check(hm.hoveredDrawItemId() == 0, "update 0: hoveredDrawItemId is 0");
  }

  // Test 4: Clear resets hover state
  {
    dc::HoverManager hm;
    hm.update(42, 10.0, 20.0, 100.0, 200.0);
    check(hm.isHovering(), "before clear: hovering");
    hm.clear();
    check(!hm.isHovering(), "after clear: not hovering");
    check(hm.hoveredDrawItemId() == 0, "after clear: hoveredDrawItemId is 0");
    check(hm.hoverDataX() == 0.0, "after clear: hoverDataX is 0");
    check(hm.hoverDataY() == 0.0, "after clear: hoverDataY is 0");
  }

  // Test 5: Enter callback fires on hover start
  {
    dc::HoverManager hm;
    std::uint32_t enteredId = 0;
    double enteredX = 0, enteredY = 0;
    hm.setOnHoverEnter([&](std::uint32_t id, double dx, double dy) {
      enteredId = id;
      enteredX = dx;
      enteredY = dy;
    });

    hm.update(7, 15.0, 25.0, 150.0, 250.0);
    check(enteredId == 7, "enter callback: id is 7");
    check(enteredX == 15.0, "enter callback: dataX is 15");
    check(enteredY == 25.0, "enter callback: dataY is 25");
  }

  // Test 6: Exit callback fires when hover target changes
  {
    dc::HoverManager hm;
    std::uint32_t exitedId = 0;
    hm.setOnHoverExit([&](std::uint32_t id, double, double) {
      exitedId = id;
    });

    hm.update(10, 1.0, 2.0, 10.0, 20.0);
    check(exitedId == 0, "no exit yet");

    hm.update(20, 3.0, 4.0, 30.0, 40.0);
    check(exitedId == 10, "exit callback: exited id 10");
  }

  // Test 7: Exit callback fires on clear
  {
    dc::HoverManager hm;
    std::uint32_t exitedId = 0;
    hm.setOnHoverExit([&](std::uint32_t id, double, double) {
      exitedId = id;
    });

    hm.update(55, 5.0, 6.0, 50.0, 60.0);
    hm.clear();
    check(exitedId == 55, "exit on clear: exited id 55");
  }

  // Test 8: No exit callback if not hovering when clear is called
  {
    dc::HoverManager hm;
    bool exitCalled = false;
    hm.setOnHoverExit([&](std::uint32_t, double, double) {
      exitCalled = true;
    });

    hm.clear();
    check(!exitCalled, "no exit callback if not hovering on clear");
  }

  // Test 9: Enter/exit sequence when switching between items
  {
    dc::HoverManager hm;
    int enterCount = 0;
    int exitCount = 0;
    hm.setOnHoverEnter([&](std::uint32_t, double, double) { ++enterCount; });
    hm.setOnHoverExit([&](std::uint32_t, double, double) { ++exitCount; });

    hm.update(1, 0, 0, 0, 0);
    check(enterCount == 1, "sequence: first enter");
    check(exitCount == 0, "sequence: no exit yet");

    hm.update(2, 0, 0, 0, 0);
    check(enterCount == 2, "sequence: second enter");
    check(exitCount == 1, "sequence: first exit");

    hm.update(0, 0, 0, 0, 0);
    check(enterCount == 2, "sequence: no enter for id 0");
    check(exitCount == 2, "sequence: second exit (to nothing)");
  }

  // Test 10: Same drawItemId does not re-trigger callbacks
  {
    dc::HoverManager hm;
    int enterCount = 0;
    hm.setOnHoverEnter([&](std::uint32_t, double, double) { ++enterCount; });

    hm.update(5, 1.0, 2.0, 10.0, 20.0);
    hm.update(5, 3.0, 4.0, 30.0, 40.0);
    check(enterCount == 1, "same id: enter called only once");
    check(hm.hoverDataX() == 3.0, "same id: dataX updated");
    check(hm.hoverDataY() == 4.0, "same id: dataY updated");
  }

  // Test 11: Tooltip data reflects hover state after update with no provider
  {
    dc::HoverManager hm;
    hm.update(42, 10.0, 20.0, 100.0, 200.0);
    const auto& tt = hm.tooltip();
    check(tt.visible, "tooltip: visible after hover");
    check(tt.drawItemId == 42, "tooltip: drawItemId is 42");
    check(tt.dataX == 10.0, "tooltip: dataX is 10");
    check(tt.screenX == 100.0, "tooltip: screenX is 100");
  }

  // Test 12: Tooltip cleared when hovering nothing
  {
    dc::HoverManager hm;
    hm.update(42, 10.0, 20.0, 100.0, 200.0);
    hm.update(0, 0, 0, 0, 0);
    const auto& tt = hm.tooltip();
    check(!tt.visible, "tooltip: not visible when hovering nothing");
  }

  std::printf("=== D71.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
