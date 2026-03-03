// D38.1 — VirtualRange: range computation, overscan, edge clamping, changed detection
#include "dc/data/VirtualRange.hpp"

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
  std::printf("=== D38.1 VirtualRange Tests ===\n");

  dc::VirtualizationManager vm;
  dc::VirtualConfig cfg;
  cfg.recordStride = 8;
  cfg.overscan = 10;
  cfg.dataXMin = 0.0;
  cfg.dataXStep = 1.0;
  vm.setConfig(cfg);
  vm.setTotalCount(1000);

  // Test 1: Basic range computation
  {
    auto r = vm.update(100.0, 200.0);
    check(r.startIndex == 90, "startIndex = 100 - 10 overscan = 90");
    check(r.endIndex == 211, "endIndex = 200 + 1 + 10 overscan = 211");
    check(r.totalCount == 1000, "totalCount preserved");
    check(r.changed, "first update is always changed");
  }

  // Test 2: Same range -> not changed
  {
    auto r = vm.update(100.0, 200.0);
    check(!r.changed, "same range -> not changed");
  }

  // Test 3: Different range -> changed
  {
    auto r = vm.update(150.0, 250.0);
    check(r.changed, "different range -> changed");
    check(r.startIndex == 140, "new startIndex = 150 - 10");
    check(r.endIndex == 261, "new endIndex = 250 + 1 + 10");
  }

  // Test 4: Edge clamping at start
  {
    auto r = vm.update(0.0, 50.0);
    check(r.startIndex == 0, "startIndex clamped to 0");
    check(r.endIndex == 61, "endIndex = 50 + 1 + 10");
  }

  // Test 5: Edge clamping at end
  {
    auto r = vm.update(980.0, 1010.0);
    check(r.endIndex == 1000, "endIndex clamped to totalCount");
    check(r.startIndex == 970, "startIndex = 980 - 10");
  }

  // Test 6: Both edges clamped (small dataset)
  {
    dc::VirtualizationManager vm2;
    dc::VirtualConfig cfg2;
    cfg2.dataXStep = 1.0;
    cfg2.overscan = 100;
    vm2.setConfig(cfg2);
    vm2.setTotalCount(50);

    auto r = vm2.update(0.0, 49.0);
    check(r.startIndex == 0, "small dataset: startIndex = 0");
    check(r.endIndex == 50, "small dataset: endIndex = totalCount");
  }

  // Test 7: Zero overscan
  {
    dc::VirtualizationManager vm3;
    dc::VirtualConfig cfg3;
    cfg3.dataXStep = 1.0;
    cfg3.overscan = 0;
    vm3.setConfig(cfg3);
    vm3.setTotalCount(100);

    auto r = vm3.update(20.0, 30.0);
    check(r.startIndex == 20, "zero overscan: startIndex = 20");
    check(r.endIndex == 31, "zero overscan: endIndex = 31");
  }

  // Test 8: Non-unit step
  {
    dc::VirtualizationManager vm4;
    dc::VirtualConfig cfg4;
    cfg4.dataXStep = 0.5;
    cfg4.overscan = 5;
    cfg4.dataXMin = 0.0;
    vm4.setConfig(cfg4);
    vm4.setTotalCount(200);

    auto r = vm4.update(10.0, 20.0); // 10/0.5=20, 20/0.5=40
    check(r.startIndex == 15, "non-unit step: startIndex");
    check(r.endIndex == 46, "non-unit step: endIndex");
  }

  // Test 9: Empty dataset
  {
    dc::VirtualizationManager vm5;
    dc::VirtualConfig cfg5;
    cfg5.dataXStep = 1.0;
    vm5.setConfig(cfg5);
    vm5.setTotalCount(0);

    auto r = vm5.update(0.0, 100.0);
    check(r.startIndex == 0 && r.endIndex == 0, "empty dataset: no range");
  }

  // Test 10: dataXMin offset
  {
    dc::VirtualizationManager vm6;
    dc::VirtualConfig cfg6;
    cfg6.dataXStep = 1.0;
    cfg6.dataXMin = 100.0;
    cfg6.overscan = 0;
    vm6.setConfig(cfg6);
    vm6.setTotalCount(50);

    auto r = vm6.update(110.0, 120.0);
    check(r.startIndex == 10, "dataXMin offset: startIndex = 10");
    check(r.endIndex == 21, "dataXMin offset: endIndex = 21");
  }

  std::printf("=== D38.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
