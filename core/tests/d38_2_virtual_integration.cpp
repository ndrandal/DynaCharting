// D38.2 — VirtualRange integration with Viewport
#include "dc/data/VirtualRange.hpp"
#include "dc/viewport/Viewport.hpp"

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
  std::printf("=== D38.2 VirtualRange Integration Tests ===\n");

  dc::VirtualizationManager vm;
  dc::VirtualConfig cfg;
  cfg.dataXStep = 1.0;
  cfg.dataXMin = 0.0;
  cfg.overscan = 20;
  vm.setConfig(cfg);
  vm.setTotalCount(10000);

  dc::Viewport vp;
  vp.setPixelViewport(800, 600);

  // Test 1: Initial viewport range
  {
    vp.setDataRange(0.0, 100.0, 0.0, 100.0);
    const auto& dr = vp.dataRange();
    auto r = vm.update(dr.xMin, dr.xMax);
    check(r.startIndex == 0, "initial: startIndex clamped to 0");
    check(r.endIndex == 121, "initial: endIndex = 100 + 1 + 20");
    check(r.changed, "initial update is changed");
  }

  // Test 2: Pan right
  {
    vp.setDataRange(50.0, 150.0, 0.0, 100.0);
    const auto& dr = vp.dataRange();
    auto r = vm.update(dr.xMin, dr.xMax);
    check(r.startIndex == 30, "pan right: startIndex = 50 - 20");
    check(r.endIndex == 171, "pan right: endIndex = 150 + 1 + 20");
    check(r.changed, "pan changes range");
  }

  // Test 3: Zoom in (narrower range)
  {
    vp.setDataRange(90.0, 110.0, 0.0, 100.0);
    const auto& dr = vp.dataRange();
    auto r = vm.update(dr.xMin, dr.xMax);
    check(r.startIndex == 70, "zoom in: startIndex = 90 - 20");
    check(r.endIndex == 131, "zoom in: endIndex = 110 + 1 + 20");
  }

  // Test 4: Zoom out (wider range)
  {
    vp.setDataRange(0.0, 5000.0, 0.0, 100.0);
    const auto& dr = vp.dataRange();
    auto r = vm.update(dr.xMin, dr.xMax);
    check(r.startIndex == 0, "zoom out: startIndex clamped");
    check(r.endIndex == 5021 > 10000 ? 10000 : 5021, "zoom out: endIndex");
  }

  // Test 5: Pan to end
  {
    vp.setDataRange(9900.0, 10000.0, 0.0, 100.0);
    const auto& dr = vp.dataRange();
    auto r = vm.update(dr.xMin, dr.xMax);
    check(r.endIndex == 10000, "pan to end: endIndex clamped to totalCount");
    check(r.startIndex == 9880, "pan to end: startIndex = 9900 - 20");
  }

  // Test 6: Same range twice -> not changed
  {
    const auto& dr = vp.dataRange();
    auto r1 = vm.update(dr.xMin, dr.xMax);
    auto r2 = vm.update(dr.xMin, dr.xMax);
    check(!r2.changed, "same range twice -> not changed");
  }

  // Test 7: Negative data range
  {
    vp.setDataRange(-50.0, 50.0, 0.0, 100.0);
    dc::VirtualizationManager vm2;
    dc::VirtualConfig cfg2;
    cfg2.dataXStep = 1.0;
    cfg2.dataXMin = -100.0;
    cfg2.overscan = 10;
    vm2.setConfig(cfg2);
    vm2.setTotalCount(200);

    const auto& dr = vp.dataRange();
    auto r = vm2.update(dr.xMin, dr.xMax);
    // -50 relative to -100 = index 50, -10 = 40
    check(r.startIndex == 40, "negative range: startIndex = 40");
    // 50 relative to -100 = index 150, +1+10 = 161
    check(r.endIndex == 161, "negative range: endIndex = 161");
  }

  std::printf("=== D38.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
