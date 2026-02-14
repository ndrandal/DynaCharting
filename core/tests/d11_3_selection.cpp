// D11.3 — SelectionState test
// Tests: Single mode (select A → select B → only B), Toggle mode,
// navigation (selectNext/Previous), boundary checks.

#include "dc/selection/SelectionState.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Single mode — select A → select B → only B ---
  {
    dc::SelectionState sel;
    sel.setMode(dc::SelectionMode::Single);

    dc::SelectionKey a{40, 0};
    dc::SelectionKey b{40, 1};

    sel.select(a);
    requireTrue(sel.isSelected(a), "A selected");
    requireTrue(sel.hasSelection(), "has selection");

    sel.select(b);
    requireTrue(!sel.isSelected(a), "A deselected after B selected (single mode)");
    requireTrue(sel.isSelected(b), "B selected");
    requireTrue(sel.selectedKeys().size() == 1, "only 1 selected");

    std::printf("  Test 1 (single mode) PASS\n");
  }

  // --- Test 2: Toggle mode — toggle A on, toggle A off ---
  {
    dc::SelectionState sel;
    sel.setMode(dc::SelectionMode::Toggle);

    dc::SelectionKey a{40, 0};
    dc::SelectionKey b{40, 1};

    sel.toggle(a);
    requireTrue(sel.isSelected(a), "toggle A → on");

    sel.toggle(b);
    requireTrue(sel.isSelected(a), "A still selected");
    requireTrue(sel.isSelected(b), "B selected");
    requireTrue(sel.selectedKeys().size() == 2, "2 selected");

    sel.toggle(a);
    requireTrue(!sel.isSelected(a), "toggle A again → off");
    requireTrue(sel.isSelected(b), "B still selected");
    requireTrue(sel.selectedKeys().size() == 1, "1 selected after toggle off");

    std::printf("  Test 2 (toggle mode) PASS\n");
  }

  // --- Test 3: Navigation — selectNext / selectPrevious ---
  {
    dc::SelectionState sel;
    sel.setMode(dc::SelectionMode::Single);
    sel.setRecordCount(40, 10);

    dc::SelectionKey start{40, 0};
    sel.select(start);

    bool ok = sel.selectNext();
    requireTrue(ok, "selectNext returns true");
    requireTrue(sel.current().recordIndex == 1, "moved to record 1");

    ok = sel.selectNext();
    requireTrue(ok, "selectNext returns true again");
    requireTrue(sel.current().recordIndex == 2, "moved to record 2");

    ok = sel.selectPrevious();
    requireTrue(ok, "selectPrevious returns true");
    requireTrue(sel.current().recordIndex == 1, "back to record 1");

    std::printf("  Test 3 (navigation next/previous) PASS\n");
  }

  // --- Test 4: Boundary — at last record, selectNext returns false ---
  {
    dc::SelectionState sel;
    sel.setMode(dc::SelectionMode::Single);
    sel.setRecordCount(40, 3);

    dc::SelectionKey last{40, 2};
    sel.select(last);

    bool ok = sel.selectNext();
    requireTrue(!ok, "selectNext at last record → false");
    requireTrue(sel.current().recordIndex == 2, "still at record 2");

    // At first record, selectPrevious returns false
    dc::SelectionKey first{40, 0};
    sel.select(first);
    ok = sel.selectPrevious();
    requireTrue(!ok, "selectPrevious at record 0 → false");
    requireTrue(sel.current().recordIndex == 0, "still at record 0");

    std::printf("  Test 4 (boundary navigation) PASS\n");
  }

  // --- Test 5: clear() ---
  {
    dc::SelectionState sel;
    sel.setMode(dc::SelectionMode::Toggle);
    sel.select({40, 0});
    sel.select({40, 1});
    requireTrue(sel.hasSelection(), "has selection before clear");
    sel.clear();
    requireTrue(!sel.hasSelection(), "no selection after clear");

    std::printf("  Test 5 (clear) PASS\n");
  }

  std::printf("D11.3 selection: ALL PASS\n");
  return 0;
}
