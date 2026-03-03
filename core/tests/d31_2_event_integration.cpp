// D31.2 — EventBus integration with ChartSession
#include "dc/event/EventBus.hpp"
#include "dc/session/ChartSession.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/data/DataSource.hpp"
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

// Minimal data source that returns no data — just exercises the update path
struct NullSource : dc::DataSource {
  void start() override {}
  void stop() override {}
  bool poll(std::vector<std::uint8_t>&) override { return false; }
  bool isRunning() const override { return false; }
};

int main() {
  std::printf("=== D31.2 EventBus Integration Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  dc::ChartSession session(cp, ingest);

  dc::EventBus bus;
  session.setEventBus(&bus);

  dc::Viewport vp;
  vp.setDataRange(0, 100, 0, 100);
  vp.setPixelViewport(800, 600);
  session.setViewport(&vp);

  NullSource nullSrc;

  // Test 1: ViewportChanged event fires when session has managed transforms
  {
    int vpChanges = 0;
    auto subId = bus.subscribe(dc::EventType::ViewportChanged, [&](const dc::EventData&) {
      ++vpChanges;
    });

    // Create a pane/layer/transform so session syncs on update
    cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
    cp.applyJsonText(R"({"cmd":"createTransform","id":100})");

    // Add a managed transform via addPaneViewport
    session.addPaneViewport(1, &vp, 100);
    session.update(nullSrc);

    check(vpChanges >= 1, "ViewportChanged event emitted on update with managed transform");
    bus.unsubscribe(subId);
  }

  // Test 2: SelectionChanged event
  {
    int selChanges = 0;
    auto subId = bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) {
      ++selChanges;
    });

    session.notifySelectionChanged();
    session.update(nullSrc);

    check(selChanges >= 1, "SelectionChanged event emitted after notifySelectionChanged + update");
    bus.unsubscribe(subId);
  }

  // Test 3: Multiple event types can be observed simultaneously
  {
    int vpCount = 0, selCount = 0;
    auto sub1 = bus.subscribe(dc::EventType::ViewportChanged, [&](const dc::EventData&) {
      ++vpCount;
    });
    auto sub2 = bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) {
      ++selCount;
    });

    session.notifySelectionChanged();
    session.update(nullSrc);

    check(vpCount >= 1, "ViewportChanged fires in combined update");
    check(selCount >= 1, "SelectionChanged fires in combined update");
    bus.unsubscribe(sub1);
    bus.unsubscribe(sub2);
  }

  std::printf("=== D31.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
