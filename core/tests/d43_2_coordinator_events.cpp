// D43.2 — InteractionCoordinator EventBus emissions: click->GeometryClicked+SelectionChanged
#include "dc/interaction/InteractionCoordinator.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/ingest/IngestProcessor.hpp"

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
  std::printf("=== D43.2 InteractionCoordinator Event Tests ===\n");

  dc::Scene scene;
  dc::IngestProcessor ingest;
  dc::Viewport vp;
  vp.setPixelViewport(800, 600);
  vp.setDataRange(0, 100, 0, 100);

  // Test 1: Click emits GeometryClicked event
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    int geomClickCount = 0;
    dc::Id receivedTarget = 999;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData& e) {
      ++geomClickCount;
      receivedTarget = e.targetId;
    });

    dc::ViewportInputState input;
    input.clicked = true;
    input.cursorX = 400;
    input.cursorY = 300;
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(geomClickCount == 1, "click emits GeometryClicked once");
    check(receivedTarget == 0, "GeometryClicked targetId is 0 (placeholder)");
  }

  // Test 2: Click emits SelectionChanged event
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    int selChangedCount = 0;
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) {
      ++selChangedCount;
    });

    dc::ViewportInputState input;
    input.clicked = true;
    input.cursorX = 200;
    input.cursorY = 150;
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(selChangedCount == 1, "click emits SelectionChanged once");
  }

  // Test 3: Click emits both events (with SelectionState wired)
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    dc::SelectionState sel;
    coord.setEventBus(&bus);
    coord.setSelectionState(&sel);

    int geomCount = 0, selCount = 0;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData&) {
      ++geomCount;
    });
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) {
      ++selCount;
    });

    dc::ViewportInputState input;
    input.clicked = true;
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(geomCount == 1, "both wired: GeometryClicked emitted");
    check(selCount == 1, "both wired: SelectionChanged emitted");
    check(sel.hasSelection(), "selection state updated");
  }

  // Test 4: No click -> no events
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    int count = 0;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData&) { ++count; });
    bus.subscribe(dc::EventType::SelectionChanged, [&](const dc::EventData&) { ++count; });

    dc::ViewportInputState input;
    input.clicked = false;
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(count == 0, "no click -> no events emitted");
  }

  // Test 5: Multiple clicks -> multiple events
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    int geomCount = 0;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData&) {
      ++geomCount;
    });

    dc::ViewportInputState input;
    input.clicked = true;
    coord.processFrame(input, scene, ingest, &vp, 1);
    coord.processFrame(input, scene, ingest, &vp, 1);
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(geomCount == 3, "3 clicks -> 3 GeometryClicked events");
  }

  // Test 6: Click with no EventBus -> no crash
  {
    dc::InteractionCoordinator coord;
    dc::SelectionState sel;
    coord.setSelectionState(&sel);

    dc::ViewportInputState input;
    input.clicked = true;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);

    check(result.selectionChanged, "click without bus: selectionChanged still true");
    check(sel.hasSelection(), "click without bus: selection state updated");
  }

  // Test 7: GeometryClicked payload carries data coordinates from viewport
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    double receivedX = -1, receivedY = -1;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData& e) {
      receivedX = e.payload[0];
      receivedY = e.payload[1];
    });

    dc::ViewportInputState input;
    input.clicked = true;
    input.cursorX = 400;  // center of 800px
    input.cursorY = 300;  // center of 600px
    coord.processFrame(input, scene, ingest, &vp, 1);

    // center of viewport in data space: dataRange is 0..100 x 0..100
    // pixelToData(400, 300) for 800x600 fb with clip -1..1 should give roughly 50, 50
    check(receivedX > 40 && receivedX < 60, "GeometryClicked payload[0] near center X");
    check(receivedY > 40 && receivedY < 60, "GeometryClicked payload[1] near center Y");
  }

  // Test 8: Click with nullptr viewport -> coordinates are zero
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    double receivedX = -1, receivedY = -1;
    bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData& e) {
      receivedX = e.payload[0];
      receivedY = e.payload[1];
    });

    dc::ViewportInputState input;
    input.clicked = true;
    input.cursorX = 400;
    input.cursorY = 300;
    coord.processFrame(input, scene, ingest, nullptr, 1);

    check(receivedX == 0.0, "null viewport: payload[0] is 0");
    check(receivedY == 0.0, "null viewport: payload[1] is 0");
  }

  // Test 9: Type filtering — click events don't trigger other types
  {
    dc::InteractionCoordinator coord;
    dc::EventBus bus;
    coord.setEventBus(&bus);

    int dataCount = 0, hoverCount = 0;
    bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData&) { ++dataCount; });
    bus.subscribe(dc::EventType::HoverChanged, [&](const dc::EventData&) { ++hoverCount; });

    dc::ViewportInputState input;
    input.clicked = true;
    coord.processFrame(input, scene, ingest, &vp, 1);

    check(dataCount == 0, "click does not emit DataChanged");
    check(hoverCount == 0, "click does not emit HoverChanged");
  }

  std::printf("=== D43.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
