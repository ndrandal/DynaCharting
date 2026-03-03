// D42.1 — CommandProcessor event emission via EventBus
// Tests: FrameCommitted, DrawItemVisibilityChanged, DataChanged (buffer commands)
#include "dc/event/EventBus.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
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

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
  }
}

int main() {
  std::printf("=== D42.1 CommandProcessor Event Emission ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::EventBus bus;
  cp.setEventBus(&bus);

  // Set up a minimal scene: pane -> layer -> drawItem
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");

  // -- Test 1: FrameCommitted event after successful commitFrame --
  {
    int frameEvents = 0;
    double lastPayload = -1.0;
    auto subId = bus.subscribe(dc::EventType::FrameCommitted, [&](const dc::EventData& e) {
      ++frameEvents;
      lastPayload = e.payload[0];
    });

    requireOk(cp.applyJsonText(R"({"cmd":"beginFrame","frameId":1})"), "beginFrame");
    check(frameEvents == 0, "no FrameCommitted before commitFrame");

    requireOk(cp.applyJsonText(R"({"cmd":"commitFrame","frameId":1})"), "commitFrame");
    check(frameEvents == 1, "FrameCommitted emitted after commitFrame");
    check(lastPayload == 1.0, "FrameCommitted payload[0] = frameId");

    bus.unsubscribe(subId);
  }

  // -- Test 2: FrameCommitted fires on multiple commits --
  {
    int frameEvents = 0;
    auto subId = bus.subscribe(dc::EventType::FrameCommitted, [&](const dc::EventData&) {
      ++frameEvents;
    });

    requireOk(cp.applyJsonText(R"({"cmd":"beginFrame","frameId":2})"), "beginFrame2");
    requireOk(cp.applyJsonText(R"({"cmd":"commitFrame","frameId":2})"), "commitFrame2");
    requireOk(cp.applyJsonText(R"({"cmd":"beginFrame","frameId":3})"), "beginFrame3");
    requireOk(cp.applyJsonText(R"({"cmd":"commitFrame","frameId":3})"), "commitFrame3");
    check(frameEvents == 2, "FrameCommitted fires on each commit");

    bus.unsubscribe(subId);
  }

  // -- Test 3: Failed commitFrame does NOT emit FrameCommitted --
  {
    int frameEvents = 0;
    auto subId = bus.subscribe(dc::EventType::FrameCommitted, [&](const dc::EventData&) {
      ++frameEvents;
    });

    // Begin frame, cause a failure with a bad command, then commit
    requireOk(cp.applyJsonText(R"({"cmd":"beginFrame","frameId":10})"), "beginFrame-fail");
    // This should fail (invalid paneId), marking frame as failed
    cp.applyJsonText(R"({"cmd":"createLayer","id":999,"paneId":888})");
    // commitFrame should reject
    auto r = cp.applyJsonText(R"({"cmd":"commitFrame","frameId":10})");
    check(!r.ok, "commitFrame fails when frame has errors");
    check(frameEvents == 0, "no FrameCommitted on failed commit");

    bus.unsubscribe(subId);
  }

  // -- Test 4: DrawItemVisibilityChanged event on setDrawItemVisible --
  {
    int visEvents = 0;
    dc::Id receivedTarget = 0;
    double receivedPayload = -1.0;
    auto subId = bus.subscribe(dc::EventType::DrawItemVisibilityChanged, [&](const dc::EventData& e) {
      ++visEvents;
      receivedTarget = e.targetId;
      receivedPayload = e.payload[0];
    });

    requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":3,"visible":false})"), "setVisible false");
    check(visEvents == 1, "DrawItemVisibilityChanged emitted on setVisible false");
    check(receivedTarget == 3, "targetId = drawItemId");
    check(receivedPayload == 0.0, "payload[0] = 0 for visible=false");

    requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":3,"visible":true})"), "setVisible true");
    check(visEvents == 2, "DrawItemVisibilityChanged emitted on setVisible true");
    check(receivedPayload == 1.0, "payload[0] = 1 for visible=true");

    bus.unsubscribe(subId);
  }

  // -- Test 5: DataChanged event on buffer commands --
  // Wire up IngestProcessor for cache-policy commands
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Create a buffer in the scene and ensure it exists in IngestProcessor
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer");
  ingest.ensureBuffer(10);

  // Push some data so evict/keepLast have something to work with
  {
    std::vector<std::uint8_t> data(256, 0xAB);
    ingest.setBufferData(10, data.data(), static_cast<std::uint32_t>(data.size()));
  }

  // Test 5a: bufferSetMaxBytes emits DataChanged
  {
    int dataEvents = 0;
    dc::Id receivedTarget = 0;
    auto subId = bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData& e) {
      ++dataEvents;
      receivedTarget = e.targetId;
    });

    requireOk(cp.applyJsonText(R"({"cmd":"bufferSetMaxBytes","bufferId":10,"maxBytes":1024})"), "bufferSetMaxBytes");
    check(dataEvents == 1, "DataChanged emitted on bufferSetMaxBytes");
    check(receivedTarget == 10, "DataChanged targetId = bufferId for setMaxBytes");

    bus.unsubscribe(subId);
  }

  // Test 5b: bufferEvictFront emits DataChanged
  {
    int dataEvents = 0;
    dc::Id receivedTarget = 0;
    auto subId = bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData& e) {
      ++dataEvents;
      receivedTarget = e.targetId;
    });

    requireOk(cp.applyJsonText(R"({"cmd":"bufferEvictFront","bufferId":10,"bytes":16})"), "bufferEvictFront");
    check(dataEvents == 1, "DataChanged emitted on bufferEvictFront");
    check(receivedTarget == 10, "DataChanged targetId = bufferId for evictFront");

    bus.unsubscribe(subId);
  }

  // Test 5c: bufferKeepLast emits DataChanged
  {
    int dataEvents = 0;
    dc::Id receivedTarget = 0;
    auto subId = bus.subscribe(dc::EventType::DataChanged, [&](const dc::EventData& e) {
      ++dataEvents;
      receivedTarget = e.targetId;
    });

    requireOk(cp.applyJsonText(R"({"cmd":"bufferKeepLast","bufferId":10,"bytes":64})"), "bufferKeepLast");
    check(dataEvents == 1, "DataChanged emitted on bufferKeepLast");
    check(receivedTarget == 10, "DataChanged targetId = bufferId for keepLast");

    bus.unsubscribe(subId);
  }

  // -- Test 6: No events without EventBus wired --
  {
    dc::Scene scene2;
    dc::ResourceRegistry reg2;
    dc::CommandProcessor cp2(scene2, reg2);
    // deliberately NOT setting eventBus

    requireOk(cp2.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Q"})"), "cp2 pane");
    requireOk(cp2.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "cp2 layer");
    requireOk(cp2.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "cp2 di");

    // These should succeed without crashing (no eventBus)
    requireOk(cp2.applyJsonText(R"({"cmd":"beginFrame","frameId":1})"), "cp2 beginFrame");
    requireOk(cp2.applyJsonText(R"({"cmd":"commitFrame","frameId":1})"), "cp2 commitFrame");
    requireOk(cp2.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":3,"visible":false})"), "cp2 setVisible");
    check(true, "no crash without EventBus wired");
  }

  // -- Test 7: Multiple listeners receive events --
  {
    int countA = 0, countB = 0;
    auto subA = bus.subscribe(dc::EventType::DrawItemVisibilityChanged, [&](const dc::EventData&) { ++countA; });
    auto subB = bus.subscribe(dc::EventType::DrawItemVisibilityChanged, [&](const dc::EventData&) { ++countB; });

    requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":3,"visible":false})"), "multi vis");
    check(countA == 1 && countB == 1, "multiple listeners both receive DrawItemVisibilityChanged");

    bus.unsubscribe(subA);
    bus.unsubscribe(subB);
  }

  std::printf("=== D42.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
