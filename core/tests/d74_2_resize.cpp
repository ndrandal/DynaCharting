// D74.2 — LayoutGrid: divider drag, custom sizes, cell spanning
#include "dc/layout/LayoutGrid.hpp"

#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static bool near(double a, double b, double eps = 1e-6) {
  return std::fabs(a - b) < eps;
}

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
  std::printf("=== D74.2 LayoutGrid Resize Tests ===\n");

  // Test 1: hitTest finds horizontal divider
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 1);
    // Equal rows: divider at position 0.5

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    check(idx >= 0, "hitTest finds horizontal divider at 0.5");

    auto& d = grid.dividers()[static_cast<std::size_t>(idx)];
    check(d.horizontal, "found divider is horizontal");
  }

  // Test 2: hitTest finds vertical divider
  {
    dc::LayoutGrid grid;
    grid.setLayout(1, 2);
    // Equal cols: divider at position 0.5

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    check(idx >= 0, "hitTest finds vertical divider at 0.5");

    auto& d = grid.dividers()[static_cast<std::size_t>(idx)];
    check(!d.horizontal, "found divider is vertical");
  }

  // Test 3: hitTest miss
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);

    int idx = grid.hitTestDivider(0.1, 0.1, 0.01);
    check(idx == -1, "hitTest miss returns -1");
  }

  // Test 4: drag horizontal divider changes row heights
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 1);
    // Initially equal: 0.5, 0.5
    // Divider at position 0.5

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    check(idx >= 0, "drag: find divider");

    grid.beginDividerDrag(idx);
    grid.updateDividerDrag(idx, 0.7);  // Move divider to 0.7
    grid.endDividerDrag(idx);

    // Recompute to see effects
    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* top = grid.getCell(0, 0);
    auto* bot = grid.getCell(1, 0);

    check(near(top->height, 0.7), "top row height after drag to 0.7");
    check(near(bot->height, 0.3), "bottom row height after drag to 0.7");
  }

  // Test 5: drag vertical divider changes column widths
  {
    dc::LayoutGrid grid;
    grid.setLayout(1, 2);

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    check(idx >= 0, "drag vertical: find divider");

    grid.beginDividerDrag(idx);
    grid.updateDividerDrag(idx, 0.3);
    grid.endDividerDrag(idx);

    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* left = grid.getCell(0, 0);
    auto* right = grid.getCell(0, 1);

    check(near(left->width, 0.3), "left col width after drag to 0.3");
    check(near(right->width, 0.7), "right col width after drag to 0.3");
  }

  // Test 6: divider drag is clamped — cannot collapse a row
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 1);

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    grid.beginDividerDrag(idx);
    grid.updateDividerDrag(idx, 0.99);  // Try to push near the bottom
    grid.endDividerDrag(idx);

    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* bot = grid.getCell(1, 0);
    check(bot->height >= 0.04, "bottom row not collapsed (min fraction enforced)");
  }

  // Test 7: beginDividerDrag / endDividerDrag sets dragging flag
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 1);

    int idx = grid.hitTestDivider(0.5, 0.5, 0.02);
    check(idx >= 0, "dragging flag: find divider");

    check(!grid.dividers()[static_cast<std::size_t>(idx)].dragging,
          "not dragging initially");

    grid.beginDividerDrag(idx);
    check(grid.dividers()[static_cast<std::size_t>(idx)].dragging,
          "dragging after begin");

    grid.endDividerDrag(idx);
    check(!grid.dividers()[static_cast<std::size_t>(idx)].dragging,
          "not dragging after end");
  }

  // Test 8: custom row heights
  {
    dc::LayoutGrid grid;
    grid.setLayout(3, 1);
    grid.setRowHeights({0.5, 0.3, 0.2});
    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* r0 = grid.getCell(0, 0);
    auto* r1 = grid.getCell(1, 0);
    auto* r2 = grid.getCell(2, 0);

    check(near(r0->height, 0.5), "custom row 0 height");
    check(near(r1->height, 0.3), "custom row 1 height");
    check(near(r2->height, 0.2), "custom row 2 height");

    check(near(r0->y, 0.0), "custom row 0 y");
    check(near(r1->y, 0.5), "custom row 1 y");
    check(near(r2->y, 0.8), "custom row 2 y");
  }

  // Test 9: custom column widths
  {
    dc::LayoutGrid grid;
    grid.setLayout(1, 3);
    grid.setColWidths({0.2, 0.5, 0.3});
    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* c0 = grid.getCell(0, 0);
    auto* c1 = grid.getCell(0, 1);
    auto* c2 = grid.getCell(0, 2);

    check(near(c0->width, 0.2), "custom col 0 width");
    check(near(c1->width, 0.5), "custom col 1 width");
    check(near(c2->width, 0.3), "custom col 2 width");

    check(near(c0->x, 0.0), "custom col 0 x");
    check(near(c1->x, 0.2), "custom col 1 x");
    check(near(c2->x, 0.7), "custom col 2 x");
  }

  // Test 10: setRowHeights with wrong count is ignored
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 1);
    grid.setRowHeights({0.5, 0.3, 0.2});  // 3 values for 2 rows
    grid.setGap(0.0);
    grid.recompute(100, 100);

    // Should still be equal distribution (0.5, 0.5) since setRowHeights was rejected
    auto* r0 = grid.getCell(0, 0);
    auto* r1 = grid.getCell(1, 0);
    check(near(r0->height, 0.5), "wrong count: row 0 unchanged");
    check(near(r1->height, 0.5), "wrong count: row 1 unchanged");
  }

  // Test 11: cell spanning — 2x2, span (0,0) across 1x2
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setCellSpan(0, 0, 1, 2);  // top row: merge both columns
    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* merged = grid.getCell(0, 0);
    check(merged != nullptr, "merged cell exists");
    check(merged->colSpan == 2, "colSpan is 2");

    // The merged cell should be full width, half height
    check(near(merged->width, 1.0), "merged cell full width");
    check(near(merged->height, 0.5), "merged cell half height");

    // Cell (0,1) should have been removed
    check(grid.getCell(0, 1) == nullptr, "spanned-over cell removed");

    // Bottom row cells still exist
    check(grid.getCell(1, 0) != nullptr, "bottom-left still exists");
    check(grid.getCell(1, 1) != nullptr, "bottom-right still exists");
  }

  // Test 12: cell spanning — 2x2, span (0,0) across 2x1
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setCellSpan(0, 0, 2, 1);  // left column: merge both rows
    grid.setGap(0.0);
    grid.recompute(100, 100);

    auto* merged = grid.getCell(0, 0);
    check(merged != nullptr, "row-span merged cell exists");
    check(merged->rowSpan == 2, "rowSpan is 2");
    check(near(merged->width, 0.5), "row-span cell half width");
    check(near(merged->height, 1.0), "row-span cell full height");

    check(grid.getCell(1, 0) == nullptr, "spanned-over row cell removed");
  }

  // Test 13: cell spanning — 2x2, span (0,0) across 2x2 (full merge)
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setCellSpan(0, 0, 2, 2);
    grid.setGap(0.0);
    grid.recompute(100, 100);

    check(grid.cells().size() == 1, "full merge: 1 cell remaining");
    auto* merged = grid.getCell(0, 0);
    check(near(merged->width, 1.0) && near(merged->height, 1.0),
          "full merge: cell covers entire area");
  }

  // Test 14: span clamp to grid bounds
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setCellSpan(1, 1, 5, 5);  // exceeds grid

    auto* cell = grid.getCell(1, 1);
    check(cell != nullptr, "out-of-bounds span cell exists");
    check(cell->rowSpan == 1, "rowSpan clamped to 1");
    check(cell->colSpan == 1, "colSpan clamped to 1");
  }

  // Test 15: dividers after drag maintain correct positions for 3-row grid
  {
    dc::LayoutGrid grid;
    grid.setLayout(3, 1);
    // Initially: 1/3, 1/3, 1/3
    // 2 horizontal dividers

    check(grid.dividers().size() == 2, "3x1 has 2 dividers");

    // First divider at ~0.333, second at ~0.667
    auto& d0 = grid.dividers()[0];
    auto& d1 = grid.dividers()[1];
    check(near(d0.position, 1.0 / 3.0), "first divider at 1/3");
    check(near(d1.position, 2.0 / 3.0), "second divider at 2/3");
  }

  // Test 16: invalid divider index — no crash
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);

    grid.beginDividerDrag(-1);     // should not crash
    grid.updateDividerDrag(-1, 0.5);
    grid.endDividerDrag(-1);

    grid.beginDividerDrag(100);    // should not crash
    grid.updateDividerDrag(100, 0.5);
    grid.endDividerDrag(100);

    check(true, "invalid divider index: no crash");
  }

  // Test 17: custom sizes with gap
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setRowHeights({0.6, 0.4});
    grid.setColWidths({0.3, 0.7});
    grid.setGap(20.0);
    grid.recompute(1000, 1000);

    // Gap normalized: 20/1000 = 0.02
    // Available W = 1.0 - 0.02 = 0.98, available H = 1.0 - 0.02 = 0.98
    // Row 0 height = 0.6 * 0.98 = 0.588
    // Row 1 height = 0.4 * 0.98 = 0.392
    // Col 0 width  = 0.3 * 0.98 = 0.294
    // Col 1 width  = 0.7 * 0.98 = 0.686

    auto* c00 = grid.getCell(0, 0);
    check(near(c00->width, 0.294), "custom+gap c00 width");
    check(near(c00->height, 0.588), "custom+gap c00 height");

    auto* c11 = grid.getCell(1, 1);
    check(near(c11->width, 0.686), "custom+gap c11 width");
    check(near(c11->height, 0.392), "custom+gap c11 height");

    // c11 position: x = 0.294 + 0.02, y = 0.588 + 0.02
    check(near(c11->x, 0.314), "custom+gap c11 x");
    check(near(c11->y, 0.608), "custom+gap c11 y");
  }

  // Test 18: recompute is idempotent (calling twice gives same result)
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 3);
    grid.setGap(4.0);
    grid.recompute(800, 600);

    auto cells1 = grid.cells();

    grid.recompute(800, 600);

    auto& cells2 = grid.cells();
    check(cells1.size() == cells2.size(), "idempotent: same cell count");

    bool allSame = true;
    for (std::size_t i = 0; i < cells1.size(); ++i) {
      if (!near(cells1[i].x, cells2[i].x) ||
          !near(cells1[i].y, cells2[i].y) ||
          !near(cells1[i].width, cells2[i].width) ||
          !near(cells1[i].height, cells2[i].height)) {
        allSame = false;
        break;
      }
    }
    check(allSame, "idempotent: cell positions unchanged on second recompute");
  }

  std::printf("=== D74.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
