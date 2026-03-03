// D31.1 — EventBus: subscribe, emit, unsubscribe, multi-listener, type filtering
#include "dc/event/EventBus.hpp"

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
  std::printf("=== D31.1 EventBus Tests ===\n");

  dc::EventBus bus;

  // Test 1: Subscribe and emit
  {
    int count = 0;
    auto id = bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData& e) {
      (void)e;
      ++count;
    });
    check(id > 0, "subscribe returns positive id");

    dc::EventData ev;
    ev.type = dc::EventType::DataChanged;
    bus.emit(ev);
    check(count == 1, "callback invoked on emit");

    bus.emit(ev);
    check(count == 2, "callback invoked again on second emit");

    bus.unsubscribe(id);
  }

  // Test 2: Unsubscribe stops delivery
  {
    int count = 0;
    auto id = bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData&) {
      ++count;
    });

    dc::EventData ev;
    ev.type = dc::EventType::DataChanged;
    bus.emit(ev);
    check(count == 1, "emit before unsubscribe works");

    bus.unsubscribe(id);
    bus.emit(ev);
    check(count == 1, "no delivery after unsubscribe");
  }

  // Test 3: Type filtering — only matching type delivered
  {
    int dataCount = 0, vpCount = 0;
    bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData&) { ++dataCount; });
    bus.subscribe(dc::EventType::ViewportChanged, [&](const dc::EventData&) { ++vpCount; });

    dc::EventData dataEv;
    dataEv.type = dc::EventType::DataChanged;
    bus.emit(dataEv);
    check(dataCount == 1 && vpCount == 0, "only DataChanged listener fires");

    dc::EventData vpEv;
    vpEv.type = dc::EventType::ViewportChanged;
    bus.emit(vpEv);
    check(dataCount == 1 && vpCount == 1, "only ViewportChanged listener fires");
  }

  bus.clear();

  // Test 4: Multi-listener for same event
  {
    int a = 0, b = 0, c = 0;
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) { ++a; });
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) { ++b; });
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) { ++c; });

    check(bus.subscriberCount(dc::EventType::SelectionChanged) == 3, "3 subscribers");

    dc::EventData ev;
    ev.type = dc::EventType::SelectionChanged;
    bus.emit(ev);
    check(a == 1 && b == 1 && c == 1, "all 3 listeners fired");
  }

  // Test 5: EventData payload
  {
    dc::Id receivedTarget = 0;
    double receivedPayload = 0;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData& e) {
      receivedTarget = e.targetId;
      receivedPayload = e.payload[0];
    });

    dc::EventData ev;
    ev.type = dc::EventType::GeometryClicked;
    ev.targetId = 42;
    ev.payload[0] = 3.14;
    bus.emit(ev);
    check(receivedTarget == 42, "targetId passed through");
    check(receivedPayload == 3.14, "payload[0] passed through");
  }

  // Test 6: Clear removes all subscriptions
  bus.clear();
  check(bus.subscriberCount(dc::EventType::DataChanged) == 0, "clear removes all DataChanged");
  check(bus.subscriberCount(dc::EventType::SelectionChanged) == 0, "clear removes all SelectionChanged");
  check(bus.subscriberCount(dc::EventType::GeometryClicked) == 0, "clear removes all GeometryClicked");

  // Test 7: Unsubscribe non-existent id is safe
  bus.unsubscribe(9999);
  check(true, "unsubscribe non-existent id is safe");

  // Test 8: subscriberCount for empty type
  check(bus.subscriberCount(dc::EventType::FrameCommitted) == 0, "no subscribers for FrameCommitted");

  std::printf("=== D31.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
