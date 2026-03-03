// D73.1 — AlertManager: crossing alerts, oneShot disable, callback firing
#include "dc/data/AlertManager.hpp"

#include <cstdio>
#include <string>
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
  std::printf("=== D73.1 AlertManager Crossing Tests ===\n");

  // Test 1: CrossingUp fires when value crosses above threshold
  {
    dc::AlertManager mgr;
    std::vector<std::uint32_t> firedIds;
    std::vector<double> firedValues;

    mgr.setCallback([&](const dc::Alert& alert, double val) {
      firedIds.push_back(alert.id);
      firedValues.push_back(val);
    });

    auto id = mgr.addCrossingAlert("CrossUp50", dc::AlertCondition::CrossingUp, 50.0);
    check(id == 1, "first alert gets id 1");
    check(mgr.count() == 1, "count is 1 after adding");

    // First evaluation: initializes lastValue, should not fire
    mgr.evaluate(40.0);
    check(firedIds.empty(), "no fire on first evaluation (initialization)");
    check(mgr.triggeredCount() == 0, "triggered count 0 after init");

    // Second evaluation: still below threshold
    mgr.evaluate(45.0);
    check(firedIds.empty(), "no fire when still below threshold");

    // Third evaluation: crosses above 50
    mgr.evaluate(55.0);
    check(firedIds.size() == 1, "fires once on crossing up");
    check(firedIds[0] == id, "correct alert id fired");
    check(firedValues[0] == 55.0, "callback receives current value");
    check(mgr.triggeredCount() == 1, "triggered count is 1");
  }

  // Test 2: OneShot disables after firing
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("OneShot", dc::AlertCondition::CrossingUp, 100.0);
    // oneShot is true by default

    mgr.evaluate(90.0);   // init
    mgr.evaluate(110.0);  // cross up -> fires
    check(fireCount == 1, "oneShot: fires once");

    const dc::Alert* a = mgr.get(id);
    check(a != nullptr, "oneShot: alert exists");
    check(a->triggered, "oneShot: triggered flag set");
    check(!a->enabled, "oneShot: enabled is false after firing");

    // Even if value crosses back and up again, should not fire
    mgr.evaluate(90.0);
    mgr.evaluate(110.0);
    check(fireCount == 1, "oneShot: does not fire again after disabled");
  }

  // Test 3: Non-oneShot fires repeatedly
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("Repeating", dc::AlertCondition::CrossingUp, 50.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(40.0);   // init
    mgr.evaluate(55.0);   // cross up -> fires
    check(fireCount == 1, "repeating: first crossing fires");

    mgr.evaluate(40.0);   // back below
    mgr.evaluate(55.0);   // cross up again -> fires
    check(fireCount == 2, "repeating: second crossing fires");

    mgr.evaluate(40.0);
    mgr.evaluate(60.0);
    check(fireCount == 3, "repeating: third crossing fires");
  }

  // Test 4: CrossingDown
  {
    dc::AlertManager mgr;
    int fireCount = 0;
    double lastVal = 0;

    mgr.setCallback([&](const dc::Alert&, double v) { ++fireCount; lastVal = v; });

    mgr.addCrossingAlert("CrossDown30", dc::AlertCondition::CrossingDown, 30.0);

    mgr.evaluate(50.0);   // init
    mgr.evaluate(40.0);   // above threshold, no cross
    check(fireCount == 0, "CrossDown: no fire while above threshold");

    mgr.evaluate(25.0);   // crosses below 30
    check(fireCount == 1, "CrossDown: fires on crossing down");
    check(lastVal == 25.0, "CrossDown: correct value");
  }

  // Test 5: CrossingAny fires in both directions
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("CrossAny", dc::AlertCondition::CrossingAny, 50.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(40.0);   // init (below)
    mgr.evaluate(60.0);   // cross up
    check(fireCount == 1, "CrossAny: fires on crossing up");

    mgr.evaluate(40.0);   // cross down
    check(fireCount == 2, "CrossAny: fires on crossing down");

    mgr.evaluate(60.0);   // cross up again
    check(fireCount == 3, "CrossAny: fires on crossing up again");
  }

  // Test 6: get() returns nullptr for unknown id
  {
    dc::AlertManager mgr;
    check(mgr.get(999) == nullptr, "get unknown id returns nullptr");
  }

  // Test 7: No callback set — evaluate still works without crash
  {
    dc::AlertManager mgr;
    mgr.addCrossingAlert("NoCb", dc::AlertCondition::CrossingUp, 10.0);
    mgr.evaluate(5.0);
    mgr.evaluate(15.0);
    check(mgr.triggeredCount() == 1, "fires without callback set (no crash)");
  }

  // Test 8: Multiple alerts, only matching ones fire
  {
    dc::AlertManager mgr;
    std::vector<std::uint32_t> firedIds;

    mgr.setCallback([&](const dc::Alert& a, double) { firedIds.push_back(a.id); });

    auto id1 = mgr.addCrossingAlert("Up50", dc::AlertCondition::CrossingUp, 50.0);
    auto id2 = mgr.addCrossingAlert("Up100", dc::AlertCondition::CrossingUp, 100.0);
    auto id3 = mgr.addCrossingAlert("Down20", dc::AlertCondition::CrossingDown, 20.0);

    check(mgr.count() == 3, "three alerts added");

    mgr.evaluate(30.0);   // init all
    mgr.evaluate(60.0);   // crosses 50, not 100, not down 20
    check(firedIds.size() == 1, "only one alert fires");
    check(firedIds[0] == id1, "correct alert (Up50) fires");

    // id1 is now disabled (oneShot), only id2 and id3 are active
    firedIds.clear();
    mgr.evaluate(110.0);  // crosses 100
    check(firedIds.size() == 1, "second alert fires");
    check(firedIds[0] == id2, "correct alert (Up100) fires");

    firedIds.clear();
    mgr.evaluate(15.0);   // crosses down 20
    check(firedIds.size() == 1, "third alert fires");
    check(firedIds[0] == id3, "correct alert (Down20) fires");
  }

  // Test 9: setEnabled can manually disable/enable
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("Toggle", dc::AlertCondition::CrossingUp, 50.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(40.0);   // init
    mgr.setEnabled(id, false);
    mgr.evaluate(60.0);   // would cross, but disabled
    check(fireCount == 0, "disabled alert does not fire");

    mgr.setEnabled(id, true);
    // After re-enabling, lastValue was updated to 40.0 on init, but since
    // the alert was disabled during evaluate(60), lastValue wasn't updated.
    // Actually, disabled alerts skip evaluate entirely, so lastValue stays at 40.
    mgr.evaluate(60.0);   // crosses from stored lastValue (40) to 60
    check(fireCount == 1, "re-enabled alert fires");
  }

  // Test 10: Exact threshold value (value == threshold is not a crossing)
  {
    dc::AlertManager mgr;
    int fireCount = 0;

    mgr.setCallback([&](const dc::Alert&, double) { ++fireCount; });

    auto id = mgr.addCrossingAlert("Exact", dc::AlertCondition::CrossingUp, 50.0);
    mgr.setOneShot(id, false);

    mgr.evaluate(40.0);   // init
    mgr.evaluate(50.0);   // exactly at threshold — lastValue <= 50 && value > 50 is false (50 > 50 = false)
    check(fireCount == 0, "exact threshold does not fire CrossingUp");

    mgr.evaluate(50.1);   // just above — lastValue(50) <= 50 && value > 50 = true
    check(fireCount == 1, "just above threshold fires CrossingUp");
  }

  std::printf("=== D73.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
