// D10.2 — DataPicker (Hit Testing) test
// Tests: pick candle center → hit, pick far away → no hit,
// pick near line vertex → hit, two overlapping drawItems → closest wins.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/DataPicker.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Pick candle at center → hit with correct recordIndex ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"candle6","vertexCount":2})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"instancedCandle@1","geometryId":30})"),
      "bindDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachTransform");

    // Two candles: at x=5 and x=15
    float candles[] = {
      5.0f,  10.0f, 12.0f, 8.0f, 11.0f, 0.5f,  // cx=5
      15.0f, 20.0f, 25.0f, 18.0f, 22.0f, 0.5f   // cx=15
    };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

    dc::Viewport vp;
    vp.setDataRange(0, 20, 0, 30);
    vp.setPixelViewport(400, 300);

    dc::DataPicker picker;
    dc::PickConfig cfg;
    cfg.maxDistancePx = 50.0;
    picker.setConfig(cfg);

    // Pick near candle 0 (x=5, y=10.5)
    double px, py;
    // Convert data (5, 10.5) to pixel
    double cx, cy;
    vp.dataToClip(5.0, 10.5, cx, cy);
    // clip to pixel
    px = (cx + 1.0) / 2.0 * 400;
    py = (1.0 - cy) / 2.0 * 300; // flip Y for pixel coords (top-left origin)

    auto result = picker.pick(px, py, 1, scene, ingest, vp);
    requireTrue(result.hit, "hit candle 0");
    requireTrue(result.drawItemId == 40, "correct drawItemId");
    requireTrue(result.recordIndex == 0, "correct recordIndex=0");

    std::printf("  Test 1 (pick candle center → hit) PASS\n");
  }

  // --- Test 2: Pick far away → no hit ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"candle6","vertexCount":1})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"instancedCandle@1","geometryId":30})"),
      "bindDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachTransform");

    float candle[] = { 5.0f, 10.0f, 12.0f, 8.0f, 11.0f, 0.5f };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(candle), sizeof(candle));

    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(400, 300);

    dc::DataPicker picker;
    dc::PickConfig cfg;
    cfg.maxDistancePx = 10.0;
    picker.setConfig(cfg);

    // Pick at far corner (data 90, 90) → should not hit
    double cx, cy;
    vp.dataToClip(90.0, 90.0, cx, cy);
    double px = (cx + 1.0) / 2.0 * 400;
    double py = (1.0 - cy) / 2.0 * 300;

    auto result = picker.pick(px, py, 1, scene, ingest, vp);
    requireTrue(!result.hit, "no hit far away");

    std::printf("  Test 2 (pick far away → no hit) PASS\n");
  }

  // --- Test 3: Pick near line vertex → hit ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":4})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"line2d@1","geometryId":30})"),
      "bindDI");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachTransform");

    // Two line segments: (10,20)-(30,40) and (50,60)-(70,80)
    float verts[] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(400, 300);

    dc::DataPicker picker;
    dc::PickConfig cfg;
    cfg.maxDistancePx = 20.0;
    picker.setConfig(cfg);

    // Pick near vertex (10, 20)
    double cx, cy;
    vp.dataToClip(10.5, 20.5, cx, cy);
    double px = (cx + 1.0) / 2.0 * 400;
    double py = (1.0 - cy) / 2.0 * 300;

    auto result = picker.pick(px, py, 1, scene, ingest, vp);
    requireTrue(result.hit, "hit near line vertex");
    requireTrue(result.recordIndex == 0, "closest vertex is index 0");

    std::printf("  Test 3 (pick near line vertex → hit) PASS\n");
  }

  // --- Test 4: Two overlapping drawItems → closest wins ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");

    // DrawItem A with point at (10, 10)
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBufA");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":1})"),
      "createGeoA");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI_A");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"points@1","geometryId":30})"), "bindDI_A");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createXf");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachA");

    float vertA[] = { 10.0f, 10.0f };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(vertA), sizeof(vertA));

    // DrawItem B with point at (12, 10) — closer to our pick location
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":21,"byteLength":0})"), "createBufB");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":31,"vertexBufferId":21,"format":"pos2_clip","vertexCount":1})"),
      "createGeoB");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":41,"layerId":10})"), "createDI_B");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":41,"pipeline":"points@1","geometryId":31})"), "bindDI_B");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":41,"transformId":50})"), "attachB");

    float vertB[] = { 12.0f, 10.0f };
    ingest.ensureBuffer(21);
    ingest.setBufferData(21, reinterpret_cast<const std::uint8_t*>(vertB), sizeof(vertB));

    dc::Viewport vp;
    vp.setDataRange(0, 20, 0, 20);
    vp.setPixelViewport(400, 300);

    dc::DataPicker picker;
    dc::PickConfig cfg;
    cfg.maxDistancePx = 100.0;
    picker.setConfig(cfg);

    // Pick at (13, 10) — closer to B (12,10) than A (10,10)
    double cx, cy;
    vp.dataToClip(13.0, 10.0, cx, cy);
    double px = (cx + 1.0) / 2.0 * 400;
    double py = (1.0 - cy) / 2.0 * 300;

    auto result = picker.pick(px, py, 1, scene, ingest, vp);
    requireTrue(result.hit, "hit overlapping");
    requireTrue(result.drawItemId == 41, "closest drawItem B wins");

    std::printf("  Test 4 (two overlapping → closest wins) PASS\n");
  }

  std::printf("D10.2 hit_test: ALL PASS\n");
  return 0;
}
