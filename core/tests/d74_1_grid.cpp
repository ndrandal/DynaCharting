// D74.1 — LayoutGrid: grid creation, presets, cell access, recompute
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
  std::printf("=== D74.1 LayoutGrid Tests ===\n");

  // Test 1: setLayout(2,2) creates 4 cells
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    check(grid.rows() == 2, "2x2 rows");
    check(grid.cols() == 2, "2x2 cols");
    check(grid.cells().size() == 4, "2x2 has 4 cells");
  }

  // Test 2: Each cell has unique ID and correct row/col
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);

    auto* c00 = grid.getCell(0, 0);
    auto* c01 = grid.getCell(0, 1);
    auto* c10 = grid.getCell(1, 0);
    auto* c11 = grid.getCell(1, 1);

    check(c00 != nullptr, "cell(0,0) exists");
    check(c01 != nullptr, "cell(0,1) exists");
    check(c10 != nullptr, "cell(1,0) exists");
    check(c11 != nullptr, "cell(1,1) exists");

    check(c00->id != c01->id && c00->id != c10->id && c00->id != c11->id,
          "all cell IDs unique");
  }

  // Test 3: getCellById
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 3);

    auto& cells = grid.cells();
    for (auto& c : cells) {
      auto* found = grid.getCellById(c.id);
      check(found != nullptr && found->id == c.id, "getCellById round-trip");
    }
    check(grid.getCellById(9999) == nullptr, "getCellById miss returns null");
  }

  // Test 4: recompute — 2x2 grid, no gap, 1000x800 canvas
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setGap(0.0);
    grid.recompute(1000, 800);

    auto* c00 = grid.getCell(0, 0);
    auto* c01 = grid.getCell(0, 1);
    auto* c10 = grid.getCell(1, 0);
    auto* c11 = grid.getCell(1, 1);

    // With equal weights and no gap, each cell is 0.5 x 0.5
    check(near(c00->x, 0.0) && near(c00->y, 0.0), "c00 position");
    check(near(c00->width, 0.5) && near(c00->height, 0.5), "c00 size");

    check(near(c01->x, 0.5) && near(c01->y, 0.0), "c01 position");
    check(near(c01->width, 0.5) && near(c01->height, 0.5), "c01 size");

    check(near(c10->x, 0.0) && near(c10->y, 0.5), "c10 position");
    check(near(c10->width, 0.5) && near(c10->height, 0.5), "c10 size");

    check(near(c11->x, 0.5) && near(c11->y, 0.5), "c11 position");
    check(near(c11->width, 0.5) && near(c11->height, 0.5), "c11 size");
  }

  // Test 5: recompute with gap
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setGap(10.0);
    grid.recompute(1000, 1000);

    // gap in normalized: 10/1000 = 0.01
    // available = 1.0 - 0.01 = 0.99 per axis (1 gap)
    // Each cell: 0.99 * 0.5 = 0.495
    auto* c00 = grid.getCell(0, 0);
    auto* c11 = grid.getCell(1, 1);

    check(near(c00->width, 0.495), "c00 width with gap");
    check(near(c00->height, 0.495), "c00 height with gap");

    // c11 starts after first col width + gap
    check(near(c11->x, 0.495 + 0.01), "c11 x with gap");
    check(near(c11->y, 0.495 + 0.01), "c11 y with gap");
  }

  // Test 6: Preset — single
  {
    dc::LayoutGrid grid;
    grid.setSingle();
    check(grid.rows() == 1 && grid.cols() == 1, "single is 1x1");
    check(grid.cells().size() == 1, "single has 1 cell");
  }

  // Test 7: Preset — dual
  {
    dc::LayoutGrid grid;
    grid.setDual();
    check(grid.rows() == 1 && grid.cols() == 2, "dual is 1x2");
    check(grid.cells().size() == 2, "dual has 2 cells");
  }

  // Test 8: Preset — triple
  {
    dc::LayoutGrid grid;
    grid.setTriple();
    check(grid.rows() == 1 && grid.cols() == 3, "triple is 1x3");
    check(grid.cells().size() == 3, "triple has 3 cells");
  }

  // Test 9: Preset — quad
  {
    dc::LayoutGrid grid;
    grid.setQuad();
    check(grid.rows() == 2 && grid.cols() == 2, "quad is 2x2");
    check(grid.cells().size() == 4, "quad has 4 cells");
  }

  // Test 10: Preset — 2x3
  {
    dc::LayoutGrid grid;
    grid.setLayout2x3();
    check(grid.rows() == 2 && grid.cols() == 3, "layout2x3 is 2x3");
    check(grid.cells().size() == 6, "layout2x3 has 6 cells");
  }

  // Test 11: Preset — 3x3
  {
    dc::LayoutGrid grid;
    grid.setLayout3x3();
    check(grid.rows() == 3 && grid.cols() == 3, "layout3x3 is 3x3");
    check(grid.cells().size() == 9, "layout3x3 has 9 cells");
  }

  // Test 12: setChartId
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    grid.setChartId(0, 0, 42);
    grid.setChartId(1, 1, 99);

    auto* c00 = grid.getCell(0, 0);
    auto* c11 = grid.getCell(1, 1);
    check(c00->chartId == 42, "chartId set on (0,0)");
    check(c11->chartId == 99, "chartId set on (1,1)");
  }

  // Test 13: gap getter/setter
  {
    dc::LayoutGrid grid;
    grid.setGap(5.0);
    check(near(grid.gap(), 5.0), "gap getter returns set value");
  }

  // Test 14: dividers count — 2x2 has 1 horizontal + 1 vertical = 2
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    check(grid.dividers().size() == 2, "2x2 has 2 dividers");

    bool hasH = false, hasV = false;
    for (auto& d : grid.dividers()) {
      if (d.horizontal) hasH = true;
      else hasV = true;
    }
    check(hasH, "2x2 has horizontal divider");
    check(hasV, "2x2 has vertical divider");
  }

  // Test 15: dividers count — 3x3 has 2 horizontal + 2 vertical = 4
  {
    dc::LayoutGrid grid;
    grid.setLayout(3, 3);
    check(grid.dividers().size() == 4, "3x3 has 4 dividers");
  }

  // Test 16: 1x1 has no dividers
  {
    dc::LayoutGrid grid;
    grid.setSingle();
    check(grid.dividers().empty(), "1x1 has no dividers");
  }

  // Test 17: recompute — 1x3 with no gap, cells tile horizontally
  {
    dc::LayoutGrid grid;
    grid.setTriple();
    grid.setGap(0.0);
    grid.recompute(900, 300);

    auto* c0 = grid.getCell(0, 0);
    auto* c1 = grid.getCell(0, 1);
    auto* c2 = grid.getCell(0, 2);

    // Each cell: 1/3 width, full height
    double third = 1.0 / 3.0;
    check(near(c0->width, third), "triple c0 width");
    check(near(c1->width, third), "triple c1 width");
    check(near(c2->width, third), "triple c2 width");
    check(near(c0->height, 1.0), "triple c0 full height");

    check(near(c0->x, 0.0), "triple c0 x");
    check(near(c1->x, third), "triple c1 x");
    check(near(c2->x, 2.0 * third), "triple c2 x");
  }

  // Test 18: getCell returns null for out-of-bounds
  {
    dc::LayoutGrid grid;
    grid.setLayout(2, 2);
    check(grid.getCell(5, 5) == nullptr, "out-of-bounds cell returns null");
    check(grid.getCell(-1, 0) == nullptr, "negative row returns null");
  }

  std::printf("=== D74.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
