// D15.4 — ScrollIndicatorRecipe test

#include "dc/recipe/ScrollIndicatorRecipe.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

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

    dc::ScrollIndicatorConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "scroll";

    dc::ScrollIndicatorRecipe recipe(900, cfg);
    auto build = recipe.build();

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, "create command ok");
    }

    requireTrue(scene.hasBuffer(900), "track buf");
    requireTrue(scene.hasDrawItem(902), "track DI");
    requireTrue(scene.hasBuffer(903), "thumb buf");
    requireTrue(scene.hasDrawItem(905), "thumb DI");
    std::printf("  Test 1 (build): PASS\n");
  }

  // ---- Test 2: computeIndicator ----
  {
    dc::ScrollIndicatorConfig cfg;
    cfg.barXMin = -0.9f;
    cfg.barXMax = 0.9f;
    dc::ScrollIndicatorRecipe recipe(950, cfg);

    // Full data range 0-1000, viewing 200-400
    auto data = recipe.computeIndicator(0, 1000, 200, 400);

    // Track should span full bar width
    requireTrue(data.trackRect[0] == cfg.barXMin, "track x0");
    requireTrue(data.trackRect[2] == cfg.barXMax, "track x1");

    // Thumb should be 20% width starting at 20% position
    float barW = cfg.barXMax - cfg.barXMin;
    float expectedThumbX0 = cfg.barXMin + 0.2f * barW;
    float expectedThumbX1 = cfg.barXMin + 0.4f * barW;
    requireTrue(std::fabs(data.thumbRect[0] - expectedThumbX0) < 0.01f, "thumb x0");
    requireTrue(std::fabs(data.thumbRect[2] - expectedThumbX1) < 0.01f, "thumb x1");

    std::printf("  Test 2 (computeIndicator): PASS\n");
  }

  // ---- Test 3: viewing all data ----
  {
    dc::ScrollIndicatorConfig cfg;
    dc::ScrollIndicatorRecipe recipe(960, cfg);

    auto data = recipe.computeIndicator(0, 100, 0, 100);
    // Thumb should span full width
    requireTrue(std::fabs(data.thumbRect[0] - cfg.barXMin) < 0.01f, "full view x0");
    requireTrue(std::fabs(data.thumbRect[2] - cfg.barXMax) < 0.01f, "full view x1");
    std::printf("  Test 3 (full view): PASS\n");
  }

  std::printf("D15.4 scroll_indicator: ALL PASS\n");
  return 0;
}
