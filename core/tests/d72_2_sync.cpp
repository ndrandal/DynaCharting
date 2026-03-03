// D72.2 — SessionBridge: multi event types, enable/disable sync, removeSession,
// multiple independent groups.
#include "dc/session/SessionBridge.hpp"

#include <cstdio>
#include <cmath>
#include <string>

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

static bool feq(double a, double b, double tol = 1e-9) {
  return std::fabs(a - b) < tol;
}

int main() {
  std::printf("=== D72.2 SessionBridge Multi-Type & Sync Control ===\n");

  // ---- Test 1: Multiple event types ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    auto sB = group.addSession();

    int crosshairCount = 0;
    int rangeCount = 0;
    int symbolCount = 0;
    int intervalCount = 0;
    double rangeStart = 0, rangeEnd = 0;
    std::string receivedSymbol;
    std::string receivedInterval;

    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++crosshairCount; });
    group.subscribe(sB, dc::SyncEventType::TimeRangeChange,
      [&](const dc::SyncEvent& e) {
        ++rangeCount;
        rangeStart = e.rangeStart;
        rangeEnd = e.rangeEnd;
      });
    group.subscribe(sB, dc::SyncEventType::SymbolChange,
      [&](const dc::SyncEvent& e) {
        ++symbolCount;
        receivedSymbol = e.symbol;
      });
    group.subscribe(sB, dc::SyncEventType::IntervalChange,
      [&](const dc::SyncEvent& e) {
        ++intervalCount;
        receivedInterval = e.interval;
      });

    // Crosshair
    dc::SyncEvent crossEv;
    crossEv.type = dc::SyncEventType::CrosshairMove;
    crossEv.sourceSessionId = sA;
    crossEv.dataX = 10.0;
    group.publish(crossEv);
    check(crosshairCount == 1, "CrosshairMove delivered");
    check(rangeCount == 0, "TimeRangeChange not triggered by crosshair");

    // TimeRange
    dc::SyncEvent rangeEv;
    rangeEv.type = dc::SyncEventType::TimeRangeChange;
    rangeEv.sourceSessionId = sA;
    rangeEv.rangeStart = 1000.0;
    rangeEv.rangeEnd = 2000.0;
    group.publish(rangeEv);
    check(rangeCount == 1, "TimeRangeChange delivered");
    check(feq(rangeStart, 1000.0), "rangeStart correct");
    check(feq(rangeEnd, 2000.0), "rangeEnd correct");

    // SymbolChange
    dc::SyncEvent symEv;
    symEv.type = dc::SyncEventType::SymbolChange;
    symEv.sourceSessionId = sA;
    symEv.symbol = "AAPL";
    group.publish(symEv);
    check(symbolCount == 1, "SymbolChange delivered");
    check(receivedSymbol == "AAPL", "symbol value correct");

    // IntervalChange
    dc::SyncEvent intEv;
    intEv.type = dc::SyncEventType::IntervalChange;
    intEv.sourceSessionId = sA;
    intEv.interval = "1h";
    group.publish(intEv);
    check(intervalCount == 1, "IntervalChange delivered");
    check(receivedInterval == "1h", "interval value correct");
  }

  // ---- Test 2: Enable/disable sync ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    auto sB = group.addSession();

    int crosshairCount = 0;
    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++crosshairCount; });

    // Defaults to enabled
    check(group.isSyncEnabled(dc::SyncEventType::CrosshairMove), "CrosshairMove enabled by default");

    dc::SyncEvent ev;
    ev.type = dc::SyncEventType::CrosshairMove;
    ev.sourceSessionId = sA;
    group.publish(ev);
    check(crosshairCount == 1, "event delivered while enabled");

    // Disable CrosshairMove
    group.setSyncEnabled(dc::SyncEventType::CrosshairMove, false);
    check(!group.isSyncEnabled(dc::SyncEventType::CrosshairMove), "CrosshairMove disabled");

    group.publish(ev);
    check(crosshairCount == 1, "event NOT delivered while disabled");

    // Re-enable
    group.setSyncEnabled(dc::SyncEventType::CrosshairMove, true);
    group.publish(ev);
    check(crosshairCount == 2, "event delivered after re-enable");

    // Disabling one type does not affect others
    int rangeCount = 0;
    group.subscribe(sB, dc::SyncEventType::TimeRangeChange,
      [&](const dc::SyncEvent&) { ++rangeCount; });

    group.setSyncEnabled(dc::SyncEventType::CrosshairMove, false);
    check(group.isSyncEnabled(dc::SyncEventType::TimeRangeChange), "TimeRangeChange still enabled");

    dc::SyncEvent rangeEv;
    rangeEv.type = dc::SyncEventType::TimeRangeChange;
    rangeEv.sourceSessionId = sA;
    group.publish(rangeEv);
    check(rangeCount == 1, "TimeRangeChange still works when CrosshairMove disabled");
  }

  // ---- Test 3: removeSession stops callbacks ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    auto sB = group.addSession();

    int bCount = 0;
    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++bCount; });

    dc::SyncEvent ev;
    ev.type = dc::SyncEventType::CrosshairMove;
    ev.sourceSessionId = sA;
    group.publish(ev);
    check(bCount == 1, "B receives before removal");

    group.removeSession(sB);
    check(group.sessionCount() == 1, "session count is 1 after removal");

    group.publish(ev);
    check(bCount == 1, "B does NOT receive after removal");

    // Verify A is still there
    auto ids = group.sessionIds();
    check(ids.size() == 1 && ids[0] == sA, "only A remains");
  }

  // ---- Test 4: Multiple groups are independent ----
  {
    dc::SessionBridge bridge;
    auto g1 = bridge.createGroup();
    auto g2 = bridge.createGroup();
    check(g1 != g2, "group ids are different");
    check(bridge.groupCount() == 2, "bridge has 2 groups");

    auto* group1 = bridge.getGroup(g1);
    auto* group2 = bridge.getGroup(g2);

    auto s1A = group1->addSession();
    auto s1B = group1->addSession();
    auto s2A = group2->addSession();
    auto s2B = group2->addSession();

    int g1bCount = 0, g2bCount = 0;
    group1->subscribe(s1B, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++g1bCount; });
    group2->subscribe(s2B, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++g2bCount; });

    // Publish in group 1 — only group 1 gets it
    dc::SyncEvent ev;
    ev.type = dc::SyncEventType::CrosshairMove;
    ev.sourceSessionId = s1A;
    group1->publish(ev);
    check(g1bCount == 1, "group1 B received");
    check(g2bCount == 0, "group2 B did NOT receive");

    // Publish in group 2 — only group 2 gets it
    dc::SyncEvent ev2;
    ev2.type = dc::SyncEventType::CrosshairMove;
    ev2.sourceSessionId = s2A;
    group2->publish(ev2);
    check(g1bCount == 1, "group1 B still 1");
    check(g2bCount == 1, "group2 B received");

    // Remove group 1
    bridge.removeGroup(g1);
    check(bridge.groupCount() == 1, "bridge has 1 group after removal");
    check(bridge.getGroup(g1) == nullptr, "removed group returns null");
    check(bridge.getGroup(g2) != nullptr, "group2 still exists");
  }

  // ---- Test 5: Three sessions, broadcast to all others ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    auto sB = group.addSession();
    auto sC = group.addSession();

    int bCount = 0, cCount = 0;
    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++bCount; });
    group.subscribe(sC, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++cCount; });

    dc::SyncEvent ev;
    ev.type = dc::SyncEventType::CrosshairMove;
    ev.sourceSessionId = sA;
    group.publish(ev);
    check(bCount == 1, "B received from A");
    check(cCount == 1, "C received from A");
  }

  // ---- Test 6: Subscribe replaces callback for same type ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    auto sB = group.addSession();

    int firstCb = 0, secondCb = 0;
    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++firstCb; });
    group.subscribe(sB, dc::SyncEventType::CrosshairMove,
      [&](const dc::SyncEvent&) { ++secondCb; });

    dc::SyncEvent ev;
    ev.type = dc::SyncEventType::CrosshairMove;
    ev.sourceSessionId = sA;
    group.publish(ev);
    check(firstCb == 0, "first callback replaced");
    check(secondCb == 1, "second callback active");
  }

  // ---- Test 7: removeSession for non-existent id is safe ----
  {
    dc::SessionGroup group;
    auto sA = group.addSession();
    group.removeSession(9999); // should not crash
    check(group.sessionCount() == 1, "remove non-existent id is safe");
    (void)sA;
  }

  std::printf("=== D72.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
