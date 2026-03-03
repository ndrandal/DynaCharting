// D72.1 — SessionBridge: create group, add sessions, publish/subscribe crosshair events.
// Verify receiver gets event and sender does NOT receive its own event.
#include "dc/session/SessionBridge.hpp"

#include <cstdio>
#include <cmath>

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
  std::printf("=== D72.1 SessionBridge Basic Pub/Sub ===\n");

  dc::SessionBridge bridge;

  // Test 1: Create a group
  auto groupId = bridge.createGroup();
  check(groupId > 0, "createGroup returns positive id");
  check(bridge.groupCount() == 1, "bridge has 1 group");

  auto* group = bridge.getGroup(groupId);
  check(group != nullptr, "getGroup returns non-null");

  // Test 2: Add two sessions
  auto sessionA = group->addSession();
  auto sessionB = group->addSession();
  check(sessionA > 0, "session A has positive id");
  check(sessionB > 0, "session B has positive id");
  check(sessionA != sessionB, "session A and B have different ids");
  check(group->sessionCount() == 2, "group has 2 sessions");

  // Test 3: Subscribe session B to crosshair events
  int bReceived = 0;
  double bDataX = 0, bDataY = 0;
  std::uint32_t bSourceId = 0;
  group->subscribe(sessionB, dc::SyncEventType::CrosshairMove,
    [&](const dc::SyncEvent& e) {
      ++bReceived;
      bDataX = e.dataX;
      bDataY = e.dataY;
      bSourceId = e.sourceSessionId;
    });

  // Test 4: Subscribe session A to crosshair events (to verify it does NOT fire for own event)
  int aReceived = 0;
  group->subscribe(sessionA, dc::SyncEventType::CrosshairMove,
    [&](const dc::SyncEvent& e) {
      (void)e;
      ++aReceived;
    });

  // Test 5: Publish crosshair from session A
  dc::SyncEvent ev;
  ev.type = dc::SyncEventType::CrosshairMove;
  ev.sourceSessionId = sessionA;
  ev.dataX = 100.5;
  ev.dataY = 42.3;
  group->publish(ev);

  check(bReceived == 1, "session B received the event");
  check(feq(bDataX, 100.5), "session B got correct dataX");
  check(feq(bDataY, 42.3), "session B got correct dataY");
  check(bSourceId == sessionA, "session B sees sourceSessionId = A");
  check(aReceived == 0, "session A did NOT receive its own event");

  // Test 6: Publish crosshair from session B — A should receive, B should not
  dc::SyncEvent ev2;
  ev2.type = dc::SyncEventType::CrosshairMove;
  ev2.sourceSessionId = sessionB;
  ev2.dataX = 200.0;
  ev2.dataY = 80.0;
  group->publish(ev2);

  check(aReceived == 1, "session A received event from B");
  check(bReceived == 1, "session B did NOT receive its own event (still 1)");

  // Test 7: Session IDs list
  auto ids = group->sessionIds();
  check(ids.size() == 2, "sessionIds returns 2 entries");
  bool hasA = false, hasB = false;
  for (auto id : ids) {
    if (id == sessionA) hasA = true;
    if (id == sessionB) hasB = true;
  }
  check(hasA && hasB, "sessionIds contains both A and B");

  // Test 8: addSessionToGroup convenience
  auto sessionC = bridge.addSessionToGroup(groupId);
  check(sessionC > 0, "addSessionToGroup returns positive id");
  check(group->sessionCount() == 3, "group has 3 sessions after convenience add");

  // Test 9: addSessionToGroup for non-existent group returns 0
  auto bad = bridge.addSessionToGroup(9999);
  check(bad == 0, "addSessionToGroup for bad group returns 0");

  // Test 10: getGroup for non-existent group returns null
  check(bridge.getGroup(9999) == nullptr, "getGroup for bad id returns null");

  std::printf("=== D72.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
