// D80.2: BindingEvaluator — selection-driven filtering, visibility, color
#include "dc/binding/BindingEvaluator.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/event/EventBus.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-5f; }

// Helper: build a minimal scene with source buffer, output buffer,
// geometry, pane, layer, drawItem — ready for binding evaluation.
struct TestFixture {
  dc::Scene scene;
  dc::ResourceRegistry registry;
  dc::CommandProcessor cp;
  dc::IngestProcessor ingest;
  dc::DerivedBufferManager derivedBufs;
  dc::SelectionState selection;
  dc::EventBus eventBus;

  TestFixture() : cp(scene, registry) {
    cp.setIngestProcessor(&ingest);
  }

  void buildScene() {
    // Source buffer (100): 4 records, pos2_clip format (8 bytes each)
    // Record 0: (0.0, 0.1), Record 1: (1.0, 1.1), Record 2: (2.0, 2.1), Record 3: (3.0, 3.1)
    cp.applyJsonText(R"({"cmd":"beginFrame"})");
    cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})");
    cp.applyJsonText(R"({"cmd":"createBuffer","id":110,"byteLength":0})");
    cp.applyJsonText(R"({"cmd":"createTransform","id":50})");
    cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"main"})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"data"})");
    cp.applyJsonText(R"({"cmd":"createGeometry","id":200,"vertexBufferId":100,"format":"pos2_clip","vertexCount":4})");
    cp.applyJsonText(R"({"cmd":"createGeometry","id":210,"vertexBufferId":110,"format":"pos2_clip","vertexCount":0})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":300,"layerId":10,"pipeline":"triSolid@1","geometryId":200})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":310,"layerId":10,"pipeline":"triSolid@1","geometryId":210})");
    cp.applyJsonText(R"({"cmd":"commitFrame"})");

    // Load source data: 4 records × 8 bytes = 32 bytes
    ingest.ensureBuffer(100);
    ingest.ensureBuffer(110);
    float srcData[] = {0.0f, 0.1f, 1.0f, 1.1f, 2.0f, 2.1f, 3.0f, 3.1f};
    ingest.setBufferData(100,
      reinterpret_cast<const std::uint8_t*>(srcData),
      sizeof(srcData));
  }
};

// ---- Test 1: Selection → filterBuffer basic ----
static void testSelectionFilterBasic() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[1001] = b;

  eval.loadBindings(bindings);
  assert(eval.bindingCount() == 1);

  // Select record 2 on drawItem 300
  f.selection.select({300, 2});
  auto touched = eval.onSelectionChanged(f.selection);

  // Output buffer should have been touched
  assert(touched.size() == 1);
  assert(touched[0] == 110);

  // Verify output buffer contains record 2 only: (2.0, 2.1)
  const std::uint8_t* outData = f.ingest.getBufferData(110);
  std::uint32_t outSize = f.ingest.getBufferSize(110);
  assert(outSize == 8);  // 1 record × 8 bytes

  float x = 0, y = 0;
  std::memcpy(&x, outData, sizeof(float));
  std::memcpy(&y, outData + 4, sizeof(float));
  assert(feq(x, 2.0f));
  assert(feq(y, 2.1f));

  // Verify geometry vertex count was updated
  const auto* geom = f.scene.getGeometry(210);
  assert(geom != nullptr);
  assert(geom->vertexCount == 1);

  std::printf("  PASS: selection → filterBuffer basic\n");
}

// ---- Test 2: Selection cleared → empty output ----
static void testSelectionClearedEmptyOutput() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[1001] = b;

  eval.loadBindings(bindings);

  // Select then clear
  f.selection.select({300, 1});
  eval.onSelectionChanged(f.selection);
  f.selection.clear();
  eval.onSelectionChanged(f.selection);

  // Output should be empty
  assert(f.ingest.getBufferSize(110) == 0);
  assert(f.scene.getGeometry(210)->vertexCount == 0);

  std::printf("  PASS: selection cleared → empty output\n");
}

// ---- Test 3: Multi-select → multiple records in output ----
static void testMultiSelect() {
  TestFixture f;
  f.buildScene();
  f.selection.setMode(dc::SelectionMode::Toggle);

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[1001] = b;

  eval.loadBindings(bindings);

  // Select records 0 and 3
  f.selection.toggle({300, 0});
  f.selection.toggle({300, 3});
  eval.onSelectionChanged(f.selection);

  assert(f.ingest.getBufferSize(110) == 16);  // 2 records × 8 bytes
  assert(f.scene.getGeometry(210)->vertexCount == 2);

  std::printf("  PASS: multi-select → multiple records in output\n");
}

// ---- Test 4: Selection on wrong drawItem → no effect ----
static void testWrongDrawItemNoEffect() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;  // binding watches drawItem 300
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[1001] = b;

  eval.loadBindings(bindings);

  // Select on drawItem 999 (not watched)
  f.selection.select({999, 2});
  eval.onSelectionChanged(f.selection);

  // Output should still be empty (no matching trigger)
  assert(f.ingest.getBufferSize(110) == 0);

  std::printf("  PASS: selection on wrong drawItem → no effect\n");
}

// ---- Test 5: Selection → setVisible ----
static void testSelectionSetVisible() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "setVisible";
  b.effect.drawItemId = 310;
  b.effect.visible = true;
  b.effect.defaultVisible = false;
  bindings[2001] = b;

  eval.loadBindings(bindings);

  // Initially set drawItem 310 invisible
  f.cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":310,"visible":false})");
  assert(f.scene.getDrawItem(310)->visible == false);

  // Select → should make it visible
  f.selection.select({300, 0});
  eval.onSelectionChanged(f.selection);
  assert(f.scene.getDrawItem(310)->visible == true);

  // Clear selection → should revert to defaultVisible (false)
  f.selection.clear();
  eval.onSelectionChanged(f.selection);
  assert(f.scene.getDrawItem(310)->visible == false);

  std::printf("  PASS: selection → setVisible\n");
}

// ---- Test 6: Selection → setColor ----
static void testSelectionSetColor() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "setColor";
  b.effect.drawItemId = 310;
  b.effect.color[0] = 1; b.effect.color[1] = 0; b.effect.color[2] = 0; b.effect.color[3] = 1;
  b.effect.defaultColor[0] = 0.5f; b.effect.defaultColor[1] = 0.5f;
  b.effect.defaultColor[2] = 0.5f; b.effect.defaultColor[3] = 1.0f;
  bindings[3001] = b;

  eval.loadBindings(bindings);

  // Select → should set color to red
  f.selection.select({300, 1});
  eval.onSelectionChanged(f.selection);
  const auto* di = f.scene.getDrawItem(310);
  assert(feq(di->color[0], 1.0f));
  assert(feq(di->color[1], 0.0f));

  // Clear → should revert to gray
  f.selection.clear();
  eval.onSelectionChanged(f.selection);
  di = f.scene.getDrawItem(310);
  assert(feq(di->color[0], 0.5f));
  assert(feq(di->color[1], 0.5f));

  std::printf("  PASS: selection → setColor\n");
}

// ---- Test 7: Hover → filterBuffer ----
static void testHoverFilter() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "hover";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[4001] = b;

  eval.loadBindings(bindings);

  // Hover over record 1
  auto touched = eval.onHoverChanged(300, 1);
  assert(touched.size() == 1);
  assert(f.ingest.getBufferSize(110) == 8);

  float x = 0;
  std::memcpy(&x, f.ingest.getBufferData(110), sizeof(float));
  assert(feq(x, 1.0f));

  // Hover leaves (invalid index)
  eval.onHoverChanged(300, static_cast<std::uint32_t>(-1));
  assert(f.ingest.getBufferSize(110) == 0);

  std::printf("  PASS: hover → filterBuffer\n");
}

// ---- Test 8: Viewport → rangeBuffer ----
static void testViewportRange() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "viewport";
  b.trigger.viewportName = "main";
  b.effect.type = "rangeBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  b.effect.xFieldOffset = 0;  // x is first float in each record
  bindings[5001] = b;

  eval.loadBindings(bindings);

  // Viewport shows x range [0.5, 2.5] → records 1 and 2 match
  auto touched = eval.onViewportChanged("main", 0.5, 2.5);
  assert(touched.size() == 1);
  assert(f.ingest.getBufferSize(110) == 16);  // 2 records
  assert(f.scene.getGeometry(210)->vertexCount == 2);

  std::printf("  PASS: viewport → rangeBuffer\n");
}

// ---- Test 9: Threshold → setVisible ----
static void testThresholdVisible() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "threshold";
  b.trigger.sourceBufferId = 100;
  b.trigger.fieldOffset = 0;      // check x-field
  b.trigger.condition = "greaterThan";
  b.trigger.value = 2.5;
  b.effect.type = "setVisible";
  b.effect.drawItemId = 310;
  b.effect.recordStride = 8;  // needed for record size calculation
  b.effect.visible = true;
  b.effect.defaultVisible = false;
  bindings[6001] = b;

  eval.loadBindings(bindings);

  // Start invisible
  f.cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":310,"visible":false})");

  // Current last record has x=3.0 > 2.5 → condition met
  eval.onDataChanged({100});
  assert(f.scene.getDrawItem(310)->visible == true);

  // Overwrite buffer with lower values: last record x=1.0
  float newData[] = {0.0f, 0.0f, 1.0f, 0.0f};
  f.ingest.setBufferData(100,
    reinterpret_cast<const std::uint8_t*>(newData), sizeof(newData));
  eval.onDataChanged({100});
  assert(f.scene.getDrawItem(310)->visible == false);

  std::printf("  PASS: threshold → setVisible\n");
}

// ---- Test 10: EventBus integration ----
static void testEventBusIntegration() {
  TestFixture f;
  f.buildScene();

  dc::BindingEvaluator eval(f.cp, f.ingest, f.derivedBufs);

  std::map<dc::Id, dc::DocBinding> bindings;
  dc::DocBinding b;
  b.trigger.type = "selection";
  b.trigger.drawItemId = 300;
  b.effect.type = "filterBuffer";
  b.effect.sourceBufferId = 100;
  b.effect.outputBufferId = 110;
  b.effect.recordStride = 8;
  b.effect.geometryId = 210;
  bindings[1001] = b;

  eval.loadBindings(bindings);
  eval.attach(f.eventBus, f.selection);

  // Select and emit event — binding should fire via EventBus
  f.selection.select({300, 0});
  dc::EventData ev;
  ev.type = dc::EventType::SelectionChanged;
  f.eventBus.emit(ev);

  // Verify the binding ran
  assert(f.ingest.getBufferSize(110) == 8);

  eval.detach(f.eventBus);

  std::printf("  PASS: EventBus integration\n");
}

int main() {
  std::printf("D80.2: BindingEvaluator\n");
  testSelectionFilterBasic();
  testSelectionClearedEmptyOutput();
  testMultiSelect();
  testWrongDrawItemNoEffect();
  testSelectionSetVisible();
  testSelectionSetColor();
  testHoverFilter();
  testViewportRange();
  testThresholdVisible();
  testEventBusIntegration();
  std::printf("All D80.2 tests passed.\n");
  return 0;
}
