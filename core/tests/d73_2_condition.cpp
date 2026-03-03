// D73.2 — AlertManager: range alerts, GreaterThan/LessThan transitions, resetAll, remove/clear
#include "dc/data/AlertManager.hpp"

#include <cstdio>
#include <vector>

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
  std::printf("=== D73.2 AlertManager Condition Tests ===\n");

  // Test 1: EntersRange — value moves from outside to inside [40, 60]
  {
    dc::AlertManager mgr;
    int fireCount = 0;
    double firedVal = 0;

    mgr.setCallback([&](const dc::Alert&, double v) { ++fireCount; firedVal = v; });

    auto id = mgr.addRangeAlert("EnterRange", dc::AlertCondition::EntersRange, 40.0, 60.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(30.0);   // init, outside range
    mgr.evaluate(35.0);   // still outside
    check(fireCount == 0, "EntersRange: no fire while outside");

    mgr.evaluate(50.0);   // enters range
    check(fireCount == 1, "EntersRange: fires on entering range");
    check(firedVal == 50.0, "EntersRange: correct value");

    mgr.evaluate(55.0);   // still inside, should not fire again
    check(fireCount == 1, "EntersRange: no fire while staying inside");

    mgr.evaluate(70.0);   // exits range
    check(fireCount == 1, "EntersRange: no fire on exiting (wrong condition)");

    mgr.evaluate(45.0);   // re-enters range
    check(fireCount == 2, "EntersRange: fires on re-entering");
  }

  // Test 2: ExitsRange — value moves from inside to outside [40, 60]
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addRangeAlert("ExitRange", dc::AlertCondition::ExitsRange, 40.0, 60.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(50.0);   // init, inside range
    mgr.evaluate(55.0);   // still inside
    check(fireCount == 0, "ExitsRange: no fire while inside");

    mgr.evaluate(65.0);   // exits range (above)
    check(fireCount == 1, "ExitsRange: fires on exiting above");

    mgr.evaluate(50.0);   // back inside
    mgr.evaluate(35.0);   // exits range (below)
    check(fireCount == 2, "ExitsRange: fires on exiting below");
  }

  // Test 3: EntersRange with boundary values
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    mgr.addRangeAlert("BoundaryEnter", dc::AlertCondition::EntersRange, 40.0, 60.0);

    mgr.evaluate(39.0);   // init, outside (just below)
    mgr.evaluate(40.0);   // exactly at low boundary — inside [40, 60]
    check(fireCount == 1, "EntersRange: fires at exact low boundary");
  }

  // Test 4: ExitsRange with boundary values
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    mgr.addRangeAlert("BoundaryExit", dc::AlertCondition::ExitsRange, 40.0, 60.0);

    mgr.evaluate(60.0);   // init, inside (at high boundary)
    mgr.evaluate(60.1);   // just above high boundary — outside
    check(fireCount == 1, "ExitsRange: fires just above high boundary");
  }

  // Test 5: GreaterThan fires on transition from <= to >
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("GT75", dc::AlertCondition::GreaterThan, 75.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(70.0);   // init, below
    mgr.evaluate(72.0);   // still below
    check(fireCount == 0, "GreaterThan: no fire while below");

    mgr.evaluate(80.0);   // transitions to above
    check(fireCount == 1, "GreaterThan: fires on transition to above");

    mgr.evaluate(85.0);   // still above — should not fire again (already true)
    check(fireCount == 1, "GreaterThan: no re-fire while staying above");

    mgr.evaluate(70.0);   // drops below
    mgr.evaluate(80.0);   // transitions again
    check(fireCount == 2, "GreaterThan: fires again after dropping and re-crossing");
  }

  // Test 6: LessThan fires on transition from >= to <
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("LT25", dc::AlertCondition::LessThan, 25.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(30.0);   // init, above
    mgr.evaluate(28.0);   // still above
    check(fireCount == 0, "LessThan: no fire while above");

    mgr.evaluate(20.0);   // transitions to below
    check(fireCount == 1, "LessThan: fires on transition to below");

    mgr.evaluate(15.0);   // still below — should not re-fire
    check(fireCount == 1, "LessThan: no re-fire while staying below");

    mgr.evaluate(30.0);   // back above
    mgr.evaluate(20.0);   // transitions again
    check(fireCount == 2, "LessThan: fires again after going above and re-crossing");
  }

  // Test 7: resetAll re-enables all alerts and clears triggered state
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id1 = mgr.addCrossingAlert("A", dc::AlertCondition::CrossingUp, 50.0);
    auto id2 = mgr.addCrossingAlert("B", dc::AlertCondition::CrossingUp, 60.0);
    // both are oneShot by default

    mgr.evaluate(40.0);
    mgr.evaluate(55.0);   // A fires
    mgr.evaluate(65.0);   // B fires
    check(fireCount == 2, "resetAll: both alerts fired initially");
    check(mgr.triggeredCount() == 2, "resetAll: triggered count is 2");

    // Both are now disabled (oneShot)
    check(!mgr.get(id1)->enabled, "resetAll: A disabled after oneShot");
    check(!mgr.get(id2)->enabled, "resetAll: B disabled after oneShot");

    // Reset all
    mgr.resetAll();
    check(mgr.triggeredCount() == 0, "resetAll: triggered count is 0 after reset");
    check(mgr.get(id1)->enabled, "resetAll: A re-enabled");
    check(mgr.get(id2)->enabled, "resetAll: B re-enabled");
    check(!mgr.get(id1)->initialized, "resetAll: A initialized cleared");

    // Should be able to fire again after reset
    fireCount = 0;
    mgr.evaluate(40.0);   // re-init
    mgr.evaluate(55.0);   // A fires again
    check(fireCount == 1, "resetAll: A fires again after reset");
  }

  // Test 8: remove() removes specific alert
  {
    dc::AlertManager mgr;
    auto id1 = mgr.addCrossingAlert("X", dc::AlertCondition::CrossingUp, 10.0);
    auto id2 = mgr.addCrossingAlert("Y", dc::AlertCondition::CrossingUp, 20.0);
    check(mgr.count() == 2, "remove: starts with 2 alerts");

    mgr.remove(id1);
    check(mgr.count() == 1, "remove: count is 1 after removing");
    check(mgr.get(id1) == nullptr, "remove: removed alert is gone");
    check(mgr.get(id2) != nullptr, "remove: other alert still exists");
  }

  // Test 9: clear() removes all alerts
  {
    dc::AlertManager mgr;
    mgr.addCrossingAlert("A", dc::AlertCondition::CrossingUp, 10.0);
    mgr.addCrossingAlert("B", dc::AlertCondition::CrossingDown, 20.0);
    mgr.addRangeAlert("C", dc::AlertCondition::EntersRange, 30.0, 50.0);
    check(mgr.count() == 3, "clear: starts with 3 alerts");

    mgr.clear();
    check(mgr.count() == 0, "clear: count is 0 after clear");
    check(mgr.alerts().empty(), "clear: alerts vector is empty");
  }

  // Test 10: alerts() accessor returns all alerts
  {
    dc::AlertManager mgr;
    mgr.addCrossingAlert("First", dc::AlertCondition::CrossingUp, 10.0);
    mgr.addRangeAlert("Second", dc::AlertCondition::EntersRange, 20.0, 30.0);

    const auto& all = mgr.alerts();
    check(all.size() == 2, "alerts(): returns all alerts");
    check(all[0].name == "First", "alerts(): first alert name");
    check(all[1].name == "Second", "alerts(): second alert name");
  }

  // Test 11: Range alert with oneShot default (fires once then disables)
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    mgr.addRangeAlert("OneShotRange", dc::AlertCondition::EntersRange, 40.0, 60.0);

    mgr.evaluate(30.0);   // init, outside
    mgr.evaluate(50.0);   // enters → fires, oneShot disables
    check(fireCount == 1, "range oneShot: fires once");

    mgr.evaluate(30.0);   // exit
    mgr.evaluate(50.0);   // re-enter — disabled, no fire
    check(fireCount == 1, "range oneShot: does not fire again");
  }

  // Test 12: remove non-existent id does not crash
  {
    dc::AlertManager mgr;
    mgr.addCrossingAlert("Solo", dc::AlertCondition::CrossingUp, 10.0);
    mgr.remove(999);  // non-existent
    check(mgr.count() == 1, "remove non-existent: count unchanged");
  }

  // Test 13: IDs are unique and incrementing
  {
    dc::AlertManager mgr;
    auto a = mgr.addCrossingAlert("A", dc::AlertCondition::CrossingUp, 1.0);
    auto b = mgr.addCrossingAlert("B", dc::AlertCondition::CrossingUp, 2.0);
    auto c = mgr.addRangeAlert("C", dc::AlertCondition::EntersRange, 3.0, 4.0);
    check(a < b && b < c, "IDs are strictly increasing");
  }

  std::printf("=== D73.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
