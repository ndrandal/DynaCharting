// D16.4 — DrawingRecipe: renders drawings as lineAA@1

#include "dc/recipe/DrawingRecipe.hpp"
#include "dc/drawing/DrawingStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: build() ----
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");

    dc::DrawingRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "drawings";

    dc::DrawingRecipe recipe(800, cfg);
    auto build = recipe.build();

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create ok");
    }

    requireTrue(scene.hasBuffer(800), "buffer");
    requireTrue(scene.hasGeometry(801), "geom");
    requireTrue(scene.hasDrawItem(802), "drawItem");

    const auto* di = scene.getDrawItem(802);
    requireTrue(di && di->pipeline == "lineAA@1", "pipeline is lineAA@1");
    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeDrawings ----
  {
    dc::DrawingRecipeConfig cfg;
    dc::DrawingRecipe recipe(850, cfg);

    dc::DrawingStore store;
    store.addTrendline(10.0, 50.0, 20.0, 60.0);
    store.addHorizontalLevel(75.0);

    auto data = recipe.computeDrawings(store, 0.0, 100.0, 0.0, 200.0);

    requireTrue(data.segmentCount == 2, "2 segments");
    requireTrue(data.lineSegments.size() == 8, "8 floats");

    // Trendline: (10,50) → (20,60)
    requireTrue(data.lineSegments[0] == 10.0f, "tl x0");
    requireTrue(data.lineSegments[1] == 50.0f, "tl y0");
    requireTrue(data.lineSegments[2] == 20.0f, "tl x1");
    requireTrue(data.lineSegments[3] == 60.0f, "tl y1");

    // H-level: (0,75) → (100,75)
    requireTrue(data.lineSegments[4] == 0.0f, "hl x0");
    requireTrue(data.lineSegments[5] == 75.0f, "hl y");
    requireTrue(data.lineSegments[6] == 100.0f, "hl x1");
    requireTrue(data.lineSegments[7] == 75.0f, "hl y1");
    std::printf("  Test 2 (computeDrawings): PASS\n");
  }

  // ---- Test 3: empty store ----
  {
    dc::DrawingRecipeConfig cfg;
    dc::DrawingRecipe recipe(860, cfg);
    dc::DrawingStore store;

    auto data = recipe.computeDrawings(store, 0, 100, 0, 200);
    requireTrue(data.segmentCount == 0, "empty");
    std::printf("  Test 3 (empty): PASS\n");
  }

  std::printf("D16.4 drawing_recipe: ALL PASS\n");
  return 0;
}
