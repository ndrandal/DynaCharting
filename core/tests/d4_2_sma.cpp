// D4.2 — SmaRecipe test
// Pure C++: verify SMA values against hand-calculated, verify vertex count.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/SmaRecipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

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

static void requireNear(float a, float b, float tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // Test 1: SMA computation
  std::printf("=== SMA computation ===\n");
  {
    // 10 data points, period 3
    float prices[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float xPos[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    dc::SmaRecipeConfig cfg;
    cfg.period = 3;
    cfg.layerId = 2;
    cfg.name = "SMA3";
    dc::SmaRecipe sma(100, cfg);

    auto data = sma.compute(prices, xPos, 10, 0.0f, 10.0f, -1.0f, 1.0f);

    // SMA(3) values: 2, 3, 4, 5, 6, 7, 8, 9 → 8 valid points → 7 segments → 14 verts
    requireTrue(data.vertexCount == 14, "SMA3 of 10: 14 vertices");
    std::printf("  vertexCount=%u: OK\n", data.vertexCount);

    // Verify first SMA value maps correctly: SMA=2.0, mapped from [0,10]→[-1,1] = -0.6
    float expectedY0 = -1.0f + (2.0f / 10.0f) * 2.0f; // -0.6
    requireNear(data.lineVerts[1], expectedY0, 0.001f, "first SMA Y");
    std::printf("  first SMA Y=%.4f (expected %.4f): OK\n", data.lineVerts[1], expectedY0);
  }

  // Test 2: Recipe build produces correct commands
  std::printf("=== SmaRecipe build ===\n");
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"L"})"), "layer");

    dc::SmaRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 2;
    cfg.name = "TestSMA";
    cfg.createTransform = true;
    cfg.period = 20;
    dc::SmaRecipe sma(200, cfg);

    requireTrue(sma.bufferId() == 200, "bufferId==200");
    requireTrue(sma.geometryId() == 201, "geomId==201");
    requireTrue(sma.drawItemId() == 202, "diId==202");
    requireTrue(sma.transformId() == 203, "xformId==203");

    auto result = sma.build();
    requireTrue(result.createCommands.size() == 6, "6 create commands");
    requireTrue(result.disposeCommands.size() == 4, "4 dispose commands");

    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "sma create");
    }

    requireTrue(scene.hasBuffer(200), "buffer exists");
    const auto* di = scene.getDrawItem(202);
    requireTrue(di != nullptr, "di exists");
    requireTrue(di->pipeline == "line2d@1", "pipeline is line2d@1");
    requireTrue(di->transformId == 203, "transform attached");
    std::printf("  create: OK\n");

    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "sma dispose");
    }
    requireTrue(!scene.hasDrawItem(202), "di disposed");
    std::printf("  dispose: OK\n");
  }

  // Test 3: Edge case — period > count
  std::printf("=== SMA edge case ===\n");
  {
    float prices[] = {1, 2, 3};
    float xPos[] = {0, 1, 2};
    dc::SmaRecipeConfig cfg;
    cfg.period = 5;
    dc::SmaRecipe sma(300, cfg);
    auto data = sma.compute(prices, xPos, 3, 0.0f, 10.0f, -1.0f, 1.0f);
    requireTrue(data.vertexCount == 0, "period > count: no vertices");
    std::printf("  period > count: OK\n");
  }

  std::printf("\nD4.2 SMA PASS\n");
  return 0;
}
