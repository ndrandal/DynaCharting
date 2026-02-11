// D4.4 — BollingerRecipe test
// Pure C++: verify computation, verify 13 resources created/disposed.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/BollingerRecipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

int main() {
  // Test 1: Bollinger computation
  std::printf("=== Bollinger computation ===\n");
  {
    // Generate 30 data points
    std::vector<float> prices(30), xPos(30);
    for (int i = 0; i < 30; i++) {
      prices[static_cast<std::size_t>(i)] = 100.0f + static_cast<float>(i);
      xPos[static_cast<std::size_t>(i)] = static_cast<float>(i) * 0.1f;
    }

    dc::BollingerRecipeConfig cfg;
    cfg.period = 5;
    cfg.numStdDev = 2.0f;
    cfg.lineLayerId = 2;
    cfg.fillLayerId = 3;
    cfg.name = "BB";
    dc::BollingerRecipe bb(100, cfg);

    auto data = bb.compute(prices.data(), xPos.data(), 30,
                            95.0f, 135.0f, -0.8f, 0.8f);

    // 30 - 5 + 1 = 26 valid points → 25 segments
    requireTrue(data.middleVC == 50, "middle: 25×2=50 verts");
    requireTrue(data.upperVC == 50, "upper: 50 verts");
    requireTrue(data.lowerVC == 50, "lower: 50 verts");
    requireTrue(data.fillVC == 150, "fill: 25×6=150 verts");
    std::printf("  vertexCounts: middle=%u upper=%u lower=%u fill=%u: OK\n",
                data.middleVC, data.upperVC, data.lowerVC, data.fillVC);

    // Upper should be above middle, lower below
    // Check first segment start Y values
    float midY = data.middleVerts[1];
    float upY = data.upperVerts[1];
    float loY = data.lowerVerts[1];
    requireTrue(upY > midY, "upper > middle");
    requireTrue(loY < midY, "lower < middle");
    std::printf("  band ordering (up=%.3f > mid=%.3f > lo=%.3f): OK\n", upY, midY, loY);
  }

  // Test 2: Recipe build — 13 resources
  std::printf("=== BollingerRecipe build ===\n");
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"Lines"})"), "lineLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":3,"paneId":1,"name":"Fill"})"), "fillLayer");

    dc::BollingerRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.lineLayerId = 2;
    cfg.fillLayerId = 3;
    cfg.name = "BB";
    cfg.createTransform = true;
    dc::BollingerRecipe bb(500, cfg);

    auto result = bb.build();
    std::printf("  create commands: %zu\n", result.createCommands.size());
    std::printf("  dispose commands: %zu\n", result.disposeCommands.size());

    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "bb create");
    }

    // Verify all 13 resources
    for (int i = 0; i < 12; i++) {
      dc::Id id = 500 + static_cast<dc::Id>(i);
      bool exists = scene.hasBuffer(id) || scene.hasGeometry(id) ||
                    scene.hasDrawItem(id) || scene.hasTransform(id);
      if (!exists) {
        std::fprintf(stderr, "ASSERT FAIL: resource %llu not found\n",
                     static_cast<unsigned long long>(id));
        std::exit(1);
      }
    }
    requireTrue(scene.hasTransform(512), "shared transform 512");
    std::printf("  13 resources: OK\n");

    // Verify pipelines
    requireTrue(scene.getDrawItem(502)->pipeline == "line2d@1", "middle line2d");
    requireTrue(scene.getDrawItem(505)->pipeline == "line2d@1", "upper line2d");
    requireTrue(scene.getDrawItem(508)->pipeline == "line2d@1", "lower line2d");
    requireTrue(scene.getDrawItem(511)->pipeline == "triSolid@1", "fill triSolid");
    std::printf("  pipelines: OK\n");

    // Dispose
    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "bb dispose");
    }
    requireTrue(!scene.hasDrawItem(502), "middle disposed");
    requireTrue(!scene.hasDrawItem(511), "fill disposed");
    requireTrue(!scene.hasTransform(512), "xform disposed");
    std::printf("  dispose: OK\n");
  }

  std::printf("\nD4.4 bollinger PASS\n");
  return 0;
}
