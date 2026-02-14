// D14.1 — VolumeRecipe unit test
// Verifies volume bar generation from candle data + volume values.

#include "dc/recipe/VolumeRecipe.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // ---- Test 1: build() produces valid commands ----
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");

    dc::VolumeRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "volume";

    dc::VolumeRecipe recipe(200, cfg);
    auto build = recipe.build();

    requireTrue(build.createCommands.size() >= 5, "at least 5 create commands");
    requireTrue(build.disposeCommands.size() >= 3, "at least 3 dispose commands");

    // Apply commands
    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create command ok");
    }

    requireTrue(scene.hasBuffer(200), "buffer 200 exists");
    requireTrue(scene.hasGeometry(201), "geometry 201 exists");
    requireTrue(scene.hasDrawItem(202), "drawItem 202 exists");
    requireTrue(scene.hasTransform(203), "transform 203 exists");

    // Verify pipeline
    const auto* di = scene.getDrawItem(202);
    requireTrue(di != nullptr, "drawItem exists");
    requireTrue(di->pipeline == "instancedCandle@1", "pipeline is instancedCandle@1");

    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeVolumeBars ----
  {
    dc::VolumeRecipeConfig cfg;
    dc::VolumeRecipe recipe(300, cfg);

    // 3 candles: up, down, up
    float candles[] = {
      // x, open, high, low, close, hw
      100.0f, 50.0f, 55.0f, 48.0f, 53.0f, 5.0f,   // up (close > open)
      200.0f, 53.0f, 54.0f, 49.0f, 50.0f, 5.0f,   // down (close < open)
      300.0f, 50.0f, 58.0f, 49.0f, 56.0f, 5.0f,   // up
    };
    float volumes[] = {1000.0f, 1500.0f, 800.0f};

    auto data = recipe.computeVolumeBars(candles, volumes, 3, 4.0f);

    requireTrue(data.barCount == 3, "3 bars");
    requireTrue(data.candle6.size() == 18, "18 floats");

    // Bar 0 (up): open=0, close=1000
    requireTrue(data.candle6[0] == 100.0f, "bar0 x");
    requireTrue(data.candle6[1] == 0.0f, "bar0 open (up → 0)");
    requireTrue(data.candle6[2] == 1000.0f, "bar0 high");
    requireTrue(data.candle6[3] == 0.0f, "bar0 low");
    requireTrue(data.candle6[4] == 1000.0f, "bar0 close (up → vol)");
    requireTrue(data.candle6[5] == 4.0f, "bar0 hw");

    // Bar 1 (down): open=1500, close=0
    requireTrue(data.candle6[6] == 200.0f, "bar1 x");
    requireTrue(data.candle6[7] == 1500.0f, "bar1 open (down → vol)");
    requireTrue(data.candle6[8] == 1500.0f, "bar1 high");
    requireTrue(data.candle6[9] == 0.0f, "bar1 low");
    requireTrue(data.candle6[10] == 0.0f, "bar1 close (down → 0)");

    // Bar 2 (up): open=0, close=800
    requireTrue(data.candle6[13] == 0.0f, "bar2 open (up → 0)");
    requireTrue(data.candle6[16] == 800.0f, "bar2 close (up → vol)");

    std::printf("  Test 2 (computeVolumeBars): PASS\n");
  }

  // ---- Test 3: edge cases ----
  {
    dc::VolumeRecipeConfig cfg;
    dc::VolumeRecipe recipe(400, cfg);

    // Null / zero
    auto empty = recipe.computeVolumeBars(nullptr, nullptr, 0, 1.0f);
    requireTrue(empty.barCount == 0, "empty for null");
    requireTrue(empty.candle6.empty(), "no data for null");

    // Equal open/close (considered up)
    float candle[] = {0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 5.0f};
    float vol[] = {500.0f};
    auto result = recipe.computeVolumeBars(candle, vol, 1, 3.0f);
    requireTrue(result.barCount == 1, "1 bar");
    requireTrue(result.candle6[1] == 0.0f, "equal o/c → up → open=0");
    requireTrue(result.candle6[4] == 500.0f, "equal o/c → up → close=vol");

    std::printf("  Test 3 (edge cases): PASS\n");
  }

  // ---- Test 4: ID accessors ----
  {
    dc::VolumeRecipeConfig cfg;
    dc::VolumeRecipe recipe(500, cfg);

    requireTrue(recipe.bufferId() == 500, "bufferId");
    requireTrue(recipe.geometryId() == 501, "geometryId");
    requireTrue(recipe.drawItemId() == 502, "drawItemId");
    requireTrue(recipe.transformId() == 503, "transformId");
    requireTrue(recipe.drawItemIds().size() == 1, "1 drawItem");
    requireTrue(recipe.drawItemIds()[0] == 502, "drawItemId in list");

    std::printf("  Test 4 (ID accessors): PASS\n");
  }

  std::printf("D14.1 volume: ALL PASS\n");
  return 0;
}
