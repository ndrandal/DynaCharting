// D17.2 — RSIRecipe: RSI indicator line + reference lines via lineAA@1

#include "dc/recipe/RSIRecipe.hpp"
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

    dc::RSIRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "RSI14";
    cfg.showRefLines = true;

    dc::RSIRecipe recipe(200, cfg);
    auto build = recipe.build();

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create ok");
    }

    // Verify RSI line objects
    requireTrue(scene.hasBuffer(200), "RSI line buffer");
    requireTrue(scene.hasGeometry(201), "RSI line geometry");
    requireTrue(scene.hasDrawItem(202), "RSI line drawItem");

    const auto* di = scene.getDrawItem(202);
    requireTrue(di && di->pipeline == "lineAA@1", "pipeline is lineAA@1");

    // Verify ref line objects
    requireTrue(scene.hasBuffer(203), "ref buffer");
    requireTrue(scene.hasGeometry(204), "ref geometry");
    requireTrue(scene.hasDrawItem(205), "ref drawItem");

    const auto* refDi = scene.getDrawItem(205);
    requireTrue(refDi && refDi->pipeline == "lineAA@1", "ref pipeline is lineAA@1");

    // Verify subscriptions
    requireTrue(build.subscriptions.size() == 2, "2 subscriptions");
    requireTrue(build.subscriptions[0].bufferId == 200, "sub0 bufferId");
    requireTrue(build.subscriptions[0].format == dc::VertexFormat::Rect4, "sub0 rect4");
    requireTrue(build.subscriptions[1].bufferId == 203, "sub1 bufferId");

    // Verify dispose commands exist
    requireTrue(build.disposeCommands.size() == 6, "6 dispose commands");

    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeRSI with trending data ----
  {
    dc::RSIRecipeConfig cfg;
    cfg.period = 14;
    dc::RSIRecipe recipe(300, cfg);

    // 30 data points trending up
    float closes[30];
    float xCoords[30];
    for (int i = 0; i < 30; i++) {
      closes[i] = 100.0f + static_cast<float>(i) * 2.0f;
      xCoords[i] = static_cast<float>(i);
    }

    auto data = recipe.computeRSI(closes, 30, xCoords);

    // First valid RSI at index 14, so segments start from [14, 15], ..., [28, 29]
    // That's 15 segments (indices 14..28 paired with 15..29)
    requireTrue(data.segmentCount > 0, "has segments");
    requireTrue(data.segmentCount == 15, "15 segments for 30 trending-up points, period 14");
    requireTrue(data.lineSegments.size() == data.segmentCount * 4, "4 floats per segment");

    // First segment should start at x=14
    requireTrue(data.lineSegments[0] == 14.0f, "first seg x0 = 14");

    // RSI for all-up data should be 100
    requireTrue(data.lineSegments[1] == 100.0f, "first seg y0 = 100 (all gains)");

    std::printf("  Test 2 (computeRSI trending): PASS (%u segments)\n", data.segmentCount);
  }

  // ---- Test 3: computeRefLines produces 2 segments ----
  {
    dc::RSIRecipeConfig cfg;
    cfg.overboughtLevel = 70.0f;
    cfg.oversoldLevel = 30.0f;
    dc::RSIRecipe recipe(400, cfg);

    auto ref = recipe.computeRefLines(0.0f, 100.0f);

    requireTrue(ref.segmentCount == 2, "2 ref segments");
    requireTrue(ref.lineSegments.size() == 8, "8 floats");

    // Overbought: (0, 70) -> (100, 70)
    requireTrue(ref.lineSegments[0] == 0.0f, "ob x0");
    requireTrue(ref.lineSegments[1] == 70.0f, "ob y");
    requireTrue(ref.lineSegments[2] == 100.0f, "ob x1");
    requireTrue(ref.lineSegments[3] == 70.0f, "ob y1");

    // Oversold: (0, 30) -> (100, 30)
    requireTrue(ref.lineSegments[4] == 0.0f, "os x0");
    requireTrue(ref.lineSegments[5] == 30.0f, "os y");
    requireTrue(ref.lineSegments[6] == 100.0f, "os x1");
    requireTrue(ref.lineSegments[7] == 30.0f, "os y1");

    std::printf("  Test 3 (computeRefLines): PASS\n");
  }

  // ---- Test 4: seriesInfoList returns RSI info ----
  {
    dc::RSIRecipeConfig cfg;
    cfg.name = "RSI14";
    cfg.color[0] = 0.5f; cfg.color[1] = 0.0f;
    cfg.color[2] = 1.0f; cfg.color[3] = 1.0f;
    dc::RSIRecipe recipe(500, cfg);

    auto info = recipe.seriesInfoList();
    requireTrue(info.size() == 1, "1 series");
    requireTrue(info[0].name == "RSI14", "series name");
    requireTrue(info[0].colorHint[0] == 0.5f, "color R");
    requireTrue(info[0].colorHint[2] == 1.0f, "color B");
    requireTrue(info[0].defaultVisible, "default visible");
    requireTrue(info[0].drawItemIds.size() == 1, "1 drawItem");
    requireTrue(info[0].drawItemIds[0] == 502, "drawItemId = 502");

    std::printf("  Test 4 (seriesInfoList): PASS\n");
  }

  // ---- Test 5: drawItemIds ----
  {
    dc::RSIRecipeConfig cfg;
    cfg.showRefLines = true;
    dc::RSIRecipe recipe(600, cfg);
    auto ids = recipe.drawItemIds();
    requireTrue(ids.size() == 2, "2 drawItems with refLines");
    requireTrue(ids[0] == 602, "lineDrawItemId");
    requireTrue(ids[1] == 605, "refDrawItemId");

    dc::RSIRecipeConfig cfg2;
    cfg2.showRefLines = false;
    dc::RSIRecipe recipe2(700, cfg2);
    auto ids2 = recipe2.drawItemIds();
    requireTrue(ids2.size() == 1, "1 drawItem without refLines");

    std::printf("  Test 5 (drawItemIds): PASS\n");
  }

  // ---- Test 6: computeRSI with too few data points ----
  {
    dc::RSIRecipeConfig cfg;
    cfg.period = 14;
    dc::RSIRecipe recipe(800, cfg);

    float closes[5] = {100, 101, 102, 103, 104};
    float xCoords[5] = {0, 1, 2, 3, 4};

    auto data = recipe.computeRSI(closes, 5, xCoords);
    requireTrue(data.segmentCount == 0, "no segments for short data");

    std::printf("  Test 6 (short data): PASS\n");
  }

  std::printf("D17.2 rsi_recipe: ALL PASS\n");
  return 0;
}
