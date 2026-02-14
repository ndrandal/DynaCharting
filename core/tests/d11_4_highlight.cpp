// D11.4 — HighlightRecipe test
// Tests: candle data, select record 0 → 1 rect, select both → 2 rects, clear → 0 rects.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/recipe/HighlightRecipe.hpp"

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
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;

  // Create candle setup
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

  // Two candles
  float candles[] = {
    5.0f,  100.0f, 105.0f, 95.0f, 102.0f, 0.3f,
    10.0f, 200.0f, 210.0f, 190.0f, 205.0f, 0.3f,
  };
  ingest.ensureBuffer(20);
  ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

  dc::HighlightRecipeConfig hlCfg;
  hlCfg.paneId = 1;
  hlCfg.layerId = 10;
  hlCfg.name = "highlight";
  hlCfg.markerSize = 0.02f;

  dc::HighlightRecipe hlRecipe(1000, hlCfg);

  // Build produces 3 resources
  auto buildResult = hlRecipe.build();
  requireTrue(buildResult.createCommands.size() >= 4, "build creates 4+ commands");

  dc::SelectionState sel;
  sel.setMode(dc::SelectionMode::Toggle);

  // --- Test 1: Select record 0 → 1 rect ---
  sel.select({40, 0});
  auto data = hlRecipe.computeHighlights(sel, scene, ingest);
  requireTrue(data.instanceCount == 1, "1 highlight rect");
  requireTrue(data.rects.size() == 4, "4 floats for 1 rect");

  // Verify rect center ≈ candle 0 position (x=5, y=(100+102)/2=101)
  float cx = (data.rects[0] + data.rects[2]) * 0.5f;
  float cy = (data.rects[1] + data.rects[3]) * 0.5f;
  requireTrue(std::fabs(cx - 5.0f) < 0.01f, "rect center x ≈ 5");
  requireTrue(std::fabs(cy - 101.0f) < 0.01f, "rect center y ≈ 101");

  std::printf("  Test 1 (select 1 → 1 rect) PASS\n");

  // --- Test 2: Select both → 2 rects ---
  sel.select({40, 1});
  data = hlRecipe.computeHighlights(sel, scene, ingest);
  requireTrue(data.instanceCount == 2, "2 highlight rects");
  requireTrue(data.rects.size() == 8, "8 floats for 2 rects");

  std::printf("  Test 2 (select both → 2 rects) PASS\n");

  // --- Test 3: Clear → 0 rects ---
  sel.clear();
  data = hlRecipe.computeHighlights(sel, scene, ingest);
  requireTrue(data.instanceCount == 0, "0 highlight rects after clear");
  requireTrue(data.rects.empty(), "no rect data after clear");

  std::printf("  Test 3 (clear → 0 rects) PASS\n");

  std::printf("D11.4 highlight: ALL PASS\n");
  return 0;
}
