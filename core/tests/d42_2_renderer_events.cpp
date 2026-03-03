// D42.2 — Renderer event emission: GeometryClicked from renderPick
#include "dc/event/EventBus.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

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
  std::printf("=== D42.2 Renderer Event Emission ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("OSMesa not available — skipping\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  dc::EventBus bus;

  renderer.setEventBus(&bus);

  if (!renderer.init()) {
    std::fprintf(stderr, "Renderer init failed\n");
    return 1;
  }

  // -- Test 1: GeometryClicked emitted when pick hits a triangle --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di");

    // Full-screen triangle
    float tri[] = {-1, -1, 3, -1, -1, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf");
    gpuBufs.setCpuData(10, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})"), "bind");

    gpuBufs.uploadDirty();

    int clickEvents = 0;
    dc::Id receivedTarget = 0;
    double receivedX = -1.0, receivedY = -1.0;
    auto subId = bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData& e) {
      ++clickEvents;
      receivedTarget = e.targetId;
      receivedX = e.payload[0];
      receivedY = e.payload[1];
    });

    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result.drawItemId == 5, "renderPick returns correct drawItemId");
    check(clickEvents == 1, "GeometryClicked emitted on pick hit");
    check(receivedTarget == 5, "GeometryClicked targetId = drawItemId");
    check(receivedX == static_cast<double>(W / 2), "GeometryClicked payload[0] = pickX");
    check(receivedY == static_cast<double>(H / 2), "GeometryClicked payload[1] = pickY");

    bus.unsubscribe(subId);

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":100})"), "cleanup");
  }

  // -- Test 2: No GeometryClicked when pick hits background --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    // Small triangle in center
    float tri[] = {-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":24})"), "buf2");
    gpuBufs.setCpuData(30, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"triSolid@1","geometryId":200})"), "bind2");

    gpuBufs.uploadDirty();

    int clickEvents = 0;
    auto subId = bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData&) {
      ++clickEvents;
    });

    // Pick corner (background) — should NOT emit GeometryClicked
    auto result = renderer.renderPick(scene, gpuBufs, W, H, 2, 2);
    check(result.drawItemId == 0, "background pick returns drawItemId=0");
    check(clickEvents == 0, "no GeometryClicked on background pick");

    // Pick center (triangle) — should emit GeometryClicked
    auto result2 = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    check(result2.drawItemId == 22, "center pick returns drawItemId=22");
    check(clickEvents == 1, "GeometryClicked emitted on center pick");

    bus.unsubscribe(subId);

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":20})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":30})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":200})"), "cleanup");
  }

  // -- Test 3: Multiple picks emit multiple events --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":40,"name":"P3"})"), "pane3");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":41,"paneId":40})"), "layer3");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":41})"), "di3");

    float tri[] = {-1, -1, 3, -1, -1, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":50,"byteLength":24})"), "buf3");
    gpuBufs.setCpuData(50, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":500,"vertexBufferId":50,"vertexCount":3,"format":"pos2_clip"})"), "geom3");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"triSolid@1","geometryId":500})"), "bind3");

    gpuBufs.uploadDirty();

    int clickEvents = 0;
    auto subId = bus.subscribe(dc::EventType::GeometryClicked, [&](const dc::EventData&) {
      ++clickEvents;
    });

    renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    renderer.renderPick(scene, gpuBufs, W, H, 10, 10);
    renderer.renderPick(scene, gpuBufs, W, H, W / 4, H / 4);
    check(clickEvents == 3, "3 picks on geometry emit 3 GeometryClicked events");

    bus.unsubscribe(subId);

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":40})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":50})"), "cleanup");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":500})"), "cleanup");
  }

  // -- Test 4: No crash without EventBus wired --
  {
    dc::Renderer renderer2;
    // deliberately NOT calling setEventBus
    if (renderer2.init()) {
      requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":60,"name":"P4"})"), "pane4");
      requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":61,"paneId":60})"), "layer4");
      requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":62,"layerId":61})"), "di4");

      float tri[] = {-1, -1, 3, -1, -1, 3};
      requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":70,"byteLength":24})"), "buf4");
      gpuBufs.setCpuData(70, tri, sizeof(tri));
      requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":700,"vertexBufferId":70,"vertexCount":3,"format":"pos2_clip"})"), "geom4");
      requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":62,"pipeline":"triSolid@1","geometryId":700})"), "bind4");

      gpuBufs.uploadDirty();

      auto result = renderer2.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
      check(result.drawItemId == 62, "renderPick works without EventBus");

      // Clean up
      requireOk(cp.applyJsonText(R"({"cmd":"delete","id":60})"), "cleanup");
      requireOk(cp.applyJsonText(R"({"cmd":"delete","id":70})"), "cleanup");
      requireOk(cp.applyJsonText(R"({"cmd":"delete","id":700})"), "cleanup");
    }
  }

  std::printf("=== D42.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
