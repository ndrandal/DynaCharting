// D3.4 — Recipe system test
// Tests that recipes produce correct commands and that create/dispose work.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/TextRecipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

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

  // Create infrastructure: pane (id=1) + layer (id=2)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");

  // Test 1: LineRecipe
  std::printf("=== LineRecipe ===\n");
  {
    dc::LineRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 2;
    cfg.name = "TestLine";
    dc::LineRecipe line(100, cfg); // idBase=100

    requireTrue(line.bufferId() == 100, "line bufferId==100");
    requireTrue(line.geometryId() == 101, "line geomId==101");
    requireTrue(line.drawItemId() == 102, "line diId==102");
    requireTrue(line.transformId() == 103, "line xformId==103");

    auto result = line.build();
    std::printf("  create commands: %zu\n", result.createCommands.size());
    std::printf("  dispose commands: %zu\n", result.disposeCommands.size());
    requireTrue(result.createCommands.size() == 6, "6 create cmds (buf,geom,di,bind,xform,attach)");
    requireTrue(result.disposeCommands.size() == 4, "4 dispose cmds");

    // Apply create commands
    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "line create");
    }

    requireTrue(scene.hasBuffer(100), "buffer 100 exists");
    requireTrue(scene.hasGeometry(101), "geom 101 exists");
    requireTrue(scene.hasDrawItem(102), "di 102 exists");
    requireTrue(scene.hasTransform(103), "xform 103 exists");

    const auto* di = scene.getDrawItem(102);
    requireTrue(di->pipeline == "line2d@1", "pipeline is line2d@1");
    requireTrue(di->geometryId == 101, "geometryId bound");
    requireTrue(di->transformId == 103, "transformId attached");
    std::printf("  create: OK\n");

    // Apply dispose commands
    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "line dispose");
    }

    requireTrue(!scene.hasBuffer(100), "buffer 100 deleted");
    requireTrue(!scene.hasGeometry(101), "geom 101 deleted");
    requireTrue(!scene.hasDrawItem(102), "di 102 deleted");
    requireTrue(!scene.hasTransform(103), "xform 103 deleted");
    std::printf("  dispose: OK\n");
  }

  // Test 2: CandleRecipe
  std::printf("=== CandleRecipe ===\n");
  {
    dc::CandleRecipeConfig cfg;
    cfg.layerId = 2;
    cfg.name = "TestCandles";
    dc::CandleRecipe candle(200, cfg);

    auto result = candle.build();
    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "candle create");
    }

    requireTrue(scene.hasBuffer(200), "candle buf exists");
    const auto* di = scene.getDrawItem(202);
    requireTrue(di != nullptr, "candle di exists");
    requireTrue(di->pipeline == "instancedCandle@1", "pipeline is instancedCandle@1");
    std::printf("  create: OK (pipeline=%s)\n", di->pipeline.c_str());

    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "candle dispose");
    }
    requireTrue(!scene.hasDrawItem(202), "candle di deleted");
    std::printf("  dispose: OK\n");
  }

  // Test 3: TextRecipe
  std::printf("=== TextRecipe ===\n");
  {
    dc::TextRecipeConfig cfg;
    cfg.layerId = 2;
    cfg.name = "TestText";
    dc::TextRecipe text(300, cfg);

    auto result = text.build();
    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "text create");
    }

    const auto* di = scene.getDrawItem(302);
    requireTrue(di != nullptr && di->pipeline == "textSDF@1", "pipeline is textSDF@1");
    const auto* geom = scene.getGeometry(301);
    requireTrue(geom != nullptr && geom->format == dc::VertexFormat::Glyph8, "format is glyph8");
    std::printf("  create: OK\n");

    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "text dispose");
    }
    requireTrue(!scene.hasDrawItem(302), "text di deleted");
    std::printf("  dispose: OK\n");
  }

  // Test 4: Multiple recipes coexist
  std::printf("=== Multi-recipe coexistence ===\n");
  {
    dc::LineRecipeConfig lCfg;
    lCfg.layerId = 2;
    lCfg.name = "line1";
    dc::LineRecipe r1(400, lCfg);

    dc::CandleRecipeConfig cCfg;
    cCfg.layerId = 2;
    cCfg.name = "candles1";
    dc::CandleRecipe r2(410, cCfg);

    auto b1 = r1.build();
    auto b2 = r2.build();

    for (auto& cmd : b1.createCommands) requireOk(cp.applyJsonText(cmd), "multi r1");
    for (auto& cmd : b2.createCommands) requireOk(cp.applyJsonText(cmd), "multi r2");

    requireTrue(scene.hasDrawItem(402), "r1 di exists");
    requireTrue(scene.hasDrawItem(412), "r2 di exists");
    std::printf("  both recipes coexist: OK\n");

    // Dispose r1 only
    for (auto& cmd : b1.disposeCommands) requireOk(cp.applyJsonText(cmd), "dispose r1");
    requireTrue(!scene.hasDrawItem(402), "r1 di gone");
    requireTrue(scene.hasDrawItem(412), "r2 di still exists");
    std::printf("  selective dispose: OK\n");

    for (auto& cmd : b2.disposeCommands) requireOk(cp.applyJsonText(cmd), "dispose r2");
  }

  std::printf("\nD3.4 recipes PASS\n");
  return 0;
}
