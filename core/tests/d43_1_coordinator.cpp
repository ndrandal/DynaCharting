// D43.1 — InteractionCoordinator: click->selection, key->focus, drag->lifecycle
#include "dc/interaction/InteractionCoordinator.hpp"
#include "dc/interaction/DragDropState.hpp"
#include "dc/interaction/FocusManager.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/viewport/InputMapper.hpp"
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
  std::printf("=== D43.1 InteractionCoordinator Tests ===\n");

  dc::Scene scene;
  dc::IngestProcessor ingest;
  dc::Viewport vp;
  vp.setPixelViewport(800, 600);
  vp.setDataRange(0, 100, 0, 100);

  // Test 1: Default config
  {
    dc::InteractionCoordinator coord;
    check(coord.config().enableBoxSelect == true, "default enableBoxSelect true");
    check(coord.config().enableDragDrop == true, "default enableDragDrop true");
    check(coord.config().dragThreshold == 5.0, "default dragThreshold 5.0");
  }

  // Test 2: Custom config
  {
    dc::InteractionCoordinator coord;
    dc::InteractionConfig cfg;
    cfg.enableBoxSelect = false;
    cfg.enableDragDrop = false;
    cfg.dragThreshold = 10.0;
    coord.setConfig(cfg);
    check(coord.config().enableBoxSelect == false, "custom enableBoxSelect false");
    check(coord.config().enableDragDrop == false, "custom enableDragDrop false");
    check(coord.config().dragThreshold == 10.0, "custom dragThreshold 10.0");
  }

  // Test 3: processFrame with no wires — returns clean result
  {
    dc::InteractionCoordinator coord;
    dc::ViewportInputState input;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(!result.viewportChanged, "no wires: viewportChanged false");
    check(!result.selectionChanged, "no wires: selectionChanged false");
    check(!result.dragActive, "no wires: dragActive false");
    check(!result.focusChanged, "no wires: focusChanged false");
    check(result.clickedDrawItemId == 0, "no wires: clickedDrawItemId 0");
  }

  // Test 4: Click -> selection state changes
  {
    dc::InteractionCoordinator coord;
    dc::SelectionState sel;
    coord.setSelectionState(&sel);

    check(!sel.hasSelection(), "no selection initially");

    dc::ViewportInputState input;
    input.clicked = true;
    input.cursorX = 400;
    input.cursorY = 300;

    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(result.selectionChanged, "click -> selectionChanged true");
    check(result.clickedDrawItemId == 0, "click -> clickedDrawItemId placeholder 0");
    check(sel.hasSelection(), "selection state has selection after click");
  }

  // Test 5: Click without selection state wired — no crash, selectionChanged false
  {
    dc::InteractionCoordinator coord;
    dc::ViewportInputState input;
    input.clicked = true;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(!result.selectionChanged, "click without sel state -> selectionChanged false");
  }

  // Test 6: Key press -> focus changes
  {
    dc::InteractionCoordinator coord;
    dc::FocusManager fm;
    fm.addFocusable({10, 0, 1});
    fm.addFocusable({20, 1, 1});
    fm.addFocusable({30, 2, 2});
    coord.setFocusManager(&fm);

    // Tab key -> focusNext
    dc::ViewportInputState input;
    input.keyPressed = dc::KeyCode::Tab;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(result.focusChanged, "Tab -> focusChanged true");
    check(fm.focusedId() == 10, "Tab focuses first item (id=10)");

    // Another Tab
    auto result2 = coord.processFrame(input, scene, ingest, &vp, 1);
    check(result2.focusChanged, "second Tab -> focusChanged true");
    check(fm.focusedId() == 20, "second Tab -> focused id=20");
  }

  // Test 7: Key press without focus manager — no crash
  {
    dc::InteractionCoordinator coord;
    dc::ViewportInputState input;
    input.keyPressed = dc::KeyCode::Tab;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(!result.focusChanged, "key without fm -> focusChanged false");
  }

  // Test 8: No key pressed -> focusChanged false
  {
    dc::InteractionCoordinator coord;
    dc::FocusManager fm;
    fm.addFocusable({10, 0, 1});
    coord.setFocusManager(&fm);

    dc::ViewportInputState input;
    input.keyPressed = dc::KeyCode::None;
    auto result = coord.processFrame(input, scene, ingest, &vp, 1);
    check(!result.focusChanged, "no key -> focusChanged false");
  }

  // Test 9: Drag lifecycle — detect -> drag -> drop
  {
    dc::InteractionCoordinator coord;
    dc::DragDropState dds;
    dds.setThreshold(5.0);
    coord.setDragDropState(&dds);

    check(dds.phase() == dc::DragDropPhase::Idle, "drag: initial Idle");

    // Frame 1: start dragging
    dc::ViewportInputState input;
    input.dragging = true;
    input.cursorX = 100;
    input.cursorY = 200;
    auto r1 = coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Detecting, "drag: Detecting after first drag frame");
    check(r1.dragActive, "drag: dragActive true during Detecting");

    // Frame 2: still dragging, move beyond threshold
    input.cursorX = 200;
    input.cursorY = 300;
    auto r2 = coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Dragging, "drag: Dragging after beyond-threshold move");
    check(r2.dragActive, "drag: dragActive true during Dragging");

    // Frame 3: release (no longer dragging) -> drop
    input.dragging = false;
    auto r3 = coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Dropped, "drag: Dropped after release");
    check(!r3.dragActive, "drag: dragActive false after Dropped");
  }

  // Test 10: Drag with enableDragDrop=false -> no lifecycle
  {
    dc::InteractionCoordinator coord;
    dc::DragDropState dds;
    coord.setDragDropState(&dds);

    dc::InteractionConfig cfg;
    cfg.enableDragDrop = false;
    coord.setConfig(cfg);

    dc::ViewportInputState input;
    input.dragging = true;
    input.cursorX = 100;
    input.cursorY = 200;
    auto r = coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Idle, "drag disabled: stays Idle");
    check(!r.dragActive, "drag disabled: dragActive false");
  }

  // Test 11: Drag cancel (release during Detecting, before threshold)
  {
    dc::InteractionCoordinator coord;
    dc::DragDropState dds;
    dds.setThreshold(50.0);
    coord.setDragDropState(&dds);

    // Start drag
    dc::ViewportInputState input;
    input.dragging = true;
    input.cursorX = 100;
    input.cursorY = 200;
    coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Detecting, "cancel: Detecting");

    // Small move, still below threshold
    input.cursorX = 102;
    input.cursorY = 202;
    coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Detecting, "cancel: still Detecting");

    // Release
    input.dragging = false;
    coord.processFrame(input, scene, ingest, &vp, 1);
    check(dds.phase() == dc::DragDropPhase::Idle, "cancel: back to Idle on release");
  }

  // Test 12: InputMapper integration
  {
    dc::InteractionCoordinator coord;
    dc::InputMapper mapper;
    dc::Viewport vp2;
    vp2.setPixelViewport(800, 600);
    vp2.setDataRange(0, 100, 0, 100);
    vp2.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    mapper.setViewports({&vp2});

    coord.setInputMapper(&mapper);

    // Scroll should cause viewport change
    dc::ViewportInputState input;
    input.cursorX = 400;
    input.cursorY = 300;
    input.scrollDelta = 1.0;
    auto result = coord.processFrame(input, scene, ingest, &vp2, 1);
    check(result.viewportChanged, "scroll -> viewportChanged true");
  }

  std::printf("=== D43.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
