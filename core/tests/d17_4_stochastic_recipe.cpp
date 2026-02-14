// D17.4 — StochasticRecipe: %K + %D lines + reference lines via lineAA@1

#include "dc/recipe/StochasticRecipe.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: build() creates correct scene objects ----
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");

    dc::StochasticRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "Stoch";
    cfg.showRefLines = true;

    dc::StochasticRecipe recipe(200, cfg);
    auto build = recipe.build();

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create ok");
    }

    // Verify %K line objects
    requireTrue(scene.hasBuffer(200), "%K buffer");
    requireTrue(scene.hasGeometry(201), "%K geometry");
    requireTrue(scene.hasDrawItem(202), "%K drawItem");
    const auto* kDi = scene.getDrawItem(202);
    requireTrue(kDi && kDi->pipeline == "lineAA@1", "%K pipeline lineAA@1");

    // Verify %D line objects
    requireTrue(scene.hasBuffer(203), "%D buffer");
    requireTrue(scene.hasGeometry(204), "%D geometry");
    requireTrue(scene.hasDrawItem(205), "%D drawItem");
    const auto* dDi = scene.getDrawItem(205);
    requireTrue(dDi && dDi->pipeline == "lineAA@1", "%D pipeline lineAA@1");

    // Verify ref line objects
    requireTrue(scene.hasBuffer(206), "ref buffer");
    requireTrue(scene.hasGeometry(207), "ref geometry");
    requireTrue(scene.hasDrawItem(208), "ref drawItem");
    const auto* refDi = scene.getDrawItem(208);
    requireTrue(refDi && refDi->pipeline == "lineAA@1", "ref pipeline lineAA@1");

    // Verify subscriptions
    requireTrue(build.subscriptions.size() == 3, "3 subscriptions");
    requireTrue(build.subscriptions[0].bufferId == 200, "sub0 %K bufferId");
    requireTrue(build.subscriptions[0].format == dc::VertexFormat::Rect4, "sub0 rect4");
    requireTrue(build.subscriptions[1].bufferId == 203, "sub1 %D bufferId");
    requireTrue(build.subscriptions[2].bufferId == 206, "sub2 ref bufferId");

    // Verify dispose commands
    requireTrue(build.disposeCommands.size() == 9, "9 dispose commands");

    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeStochastic with data ----
  {
    dc::StochasticRecipeConfig cfg;
    cfg.kPeriod = 14;
    cfg.dPeriod = 3;
    dc::StochasticRecipe recipe(300, cfg);

    // 30 data points with upward trend + noise
    float highs[30], lows[30], closes[30], xCoords[30];
    for (int i = 0; i < 30; i++) {
      float base = 100.0f + static_cast<float>(i) * 1.0f;
      highs[i] = base + 2.0f;
      lows[i] = base - 2.0f;
      closes[i] = base + 1.0f;  // closing near high
      xCoords[i] = static_cast<float>(i);
    }

    auto data = recipe.computeStochastic(highs, lows, closes, 30, xCoords);

    // %K valid from index 13 (kPeriod-1), so segments from [13,14]...[28,29] = 16 segments
    requireTrue(data.kCount > 0, "has %K segments");
    requireTrue(data.kCount == 16, "16 %K segments");
    requireTrue(data.kSegments.size() == data.kCount * 4, "4 floats per %K segment");

    // %D valid from index 15 (kPeriod-1 + dPeriod-1), so segments from [15,16]...[28,29] = 14 segments
    requireTrue(data.dCount > 0, "has %D segments");
    requireTrue(data.dCount == 14, "14 %D segments");
    requireTrue(data.dSegments.size() == data.dCount * 4, "4 floats per %D segment");

    // %K values should be high (closing near top of range)
    requireTrue(data.kSegments[1] > 50.0f, "%K y0 > 50 (near high)");

    std::printf("  Test 2 (compute): PASS (kCount=%u, dCount=%u)\n",
                data.kCount, data.dCount);
  }

  // ---- Test 3: computeRefLines produces 2 segments ----
  {
    dc::StochasticRecipeConfig cfg;
    cfg.overboughtLevel = 80.0f;
    cfg.oversoldLevel = 20.0f;
    dc::StochasticRecipe recipe(400, cfg);

    auto ref = recipe.computeRefLines(0.0f, 50.0f);

    requireTrue(ref.segmentCount == 2, "2 ref segments");
    requireTrue(ref.lineSegments.size() == 8, "8 floats");

    // Overbought: (0, 80) -> (50, 80)
    requireTrue(ref.lineSegments[0] == 0.0f, "ob x0");
    requireTrue(ref.lineSegments[1] == 80.0f, "ob y");
    requireTrue(ref.lineSegments[2] == 50.0f, "ob x1");
    requireTrue(ref.lineSegments[3] == 80.0f, "ob y1");

    // Oversold: (0, 20) -> (50, 20)
    requireTrue(ref.lineSegments[4] == 0.0f, "os x0");
    requireTrue(ref.lineSegments[5] == 20.0f, "os y");
    requireTrue(ref.lineSegments[6] == 50.0f, "os x1");
    requireTrue(ref.lineSegments[7] == 20.0f, "os y1");

    std::printf("  Test 3 (computeRefLines): PASS\n");
  }

  // ---- Test 4: seriesInfoList returns %K and %D ----
  {
    dc::StochasticRecipeConfig cfg;
    cfg.name = "Stoch";
    cfg.kColor[0] = 0.0f; cfg.kColor[1] = 0.5f;
    cfg.kColor[2] = 1.0f; cfg.kColor[3] = 1.0f;
    cfg.dColor[0] = 1.0f; cfg.dColor[1] = 0.5f;
    cfg.dColor[2] = 0.0f; cfg.dColor[3] = 1.0f;
    dc::StochasticRecipe recipe(500, cfg);

    auto info = recipe.seriesInfoList();
    requireTrue(info.size() == 2, "2 series");

    // %K info
    requireTrue(info[0].name == "Stoch %K", "%K name");
    requireTrue(info[0].colorHint[2] == 1.0f, "%K blue");
    requireTrue(info[0].drawItemIds.size() == 1, "%K 1 drawItem");
    requireTrue(info[0].drawItemIds[0] == 502, "%K drawItemId = 502");

    // %D info
    requireTrue(info[1].name == "Stoch %D", "%D name");
    requireTrue(info[1].colorHint[0] == 1.0f, "%D red");
    requireTrue(info[1].drawItemIds.size() == 1, "%D 1 drawItem");
    requireTrue(info[1].drawItemIds[0] == 505, "%D drawItemId = 505");

    std::printf("  Test 4 (seriesInfoList): PASS\n");
  }

  // ---- Test 5: drawItemIds ----
  {
    dc::StochasticRecipeConfig cfg;
    cfg.showRefLines = true;
    dc::StochasticRecipe recipe(600, cfg);
    auto ids = recipe.drawItemIds();
    requireTrue(ids.size() == 3, "3 drawItems with refLines");
    requireTrue(ids[0] == 602, "kDrawItemId");
    requireTrue(ids[1] == 605, "dDrawItemId");
    requireTrue(ids[2] == 608, "refDrawItemId");

    dc::StochasticRecipeConfig cfg2;
    cfg2.showRefLines = false;
    dc::StochasticRecipe recipe2(700, cfg2);
    auto ids2 = recipe2.drawItemIds();
    requireTrue(ids2.size() == 2, "2 drawItems without refLines");

    std::printf("  Test 5 (drawItemIds): PASS\n");
  }

  // ---- Test 6: computeStochastic with too few data ----
  {
    dc::StochasticRecipeConfig cfg;
    cfg.kPeriod = 14;
    dc::StochasticRecipe recipe(800, cfg);

    float h[5] = {102, 103, 104, 105, 106};
    float l[5] = {98, 99, 100, 101, 102};
    float c[5] = {100, 101, 102, 103, 104};
    float x[5] = {0, 1, 2, 3, 4};

    auto data = recipe.computeStochastic(h, l, c, 5, x);
    requireTrue(data.kCount == 0, "no %K segments for short data");
    requireTrue(data.dCount == 0, "no %D segments for short data");

    std::printf("  Test 6 (short data): PASS\n");
  }

  std::printf("D17.4 stochastic_recipe: ALL PASS\n");
  return 0;
}
