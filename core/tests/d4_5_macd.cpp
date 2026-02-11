// D4.5 — MacdRecipe test
// Pure C++: verify EMA, MACD/signal values, histogram rects.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/math/Ema.hpp"
#include "dc/recipe/MacdRecipe.hpp"

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

static void requireNear(float a, float b, float tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg,
                 static_cast<double>(a), static_cast<double>(b));
    std::exit(1);
  }
}

int main() {
  // Test 1: EMA computation
  std::printf("=== EMA computation ===\n");
  {
    float input[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    float output[10] = {};
    dc::computeEma(input, output, 10, 3);

    // SMA seed: (1+2+3)/3 = 2.0
    requireNear(output[2], 2.0f, 0.001f, "EMA3 seed = SMA");

    // k = 2/(3+1) = 0.5
    // EMA[3] = 4 * 0.5 + 2.0 * 0.5 = 3.0
    requireNear(output[3], 3.0f, 0.001f, "EMA3[3] = 3.0");

    // EMA[4] = 5 * 0.5 + 3.0 * 0.5 = 4.0
    requireNear(output[4], 4.0f, 0.001f, "EMA3[4] = 4.0");

    std::printf("  EMA values verified: OK\n");
  }

  // Test 2: MACD computation
  std::printf("=== MACD computation ===\n");
  {
    // Generate 50 data points with a trend
    std::vector<float> prices(50), xPos(50);
    for (int i = 0; i < 50; i++) {
      prices[static_cast<std::size_t>(i)] = 100.0f + static_cast<float>(i) * 0.5f;
      xPos[static_cast<std::size_t>(i)] = -0.9f + static_cast<float>(i) * 0.036f;
    }

    dc::MacdRecipeConfig cfg;
    cfg.fastPeriod = 5;
    cfg.slowPeriod = 10;
    cfg.signalPeriod = 3;
    cfg.lineLayerId = 2;
    cfg.histLayerId = 3;
    cfg.name = "MACD";
    dc::MacdRecipe macd(700, cfg);

    auto data = macd.compute(prices.data(), xPos.data(), 50, 0.01f, -0.9f, 0.9f);

    requireTrue(data.macdVC > 0, "MACD line has vertices");
    requireTrue(data.signalVC > 0, "signal line has vertices");
    requireTrue(data.posHistCount + data.negHistCount > 0, "histogram has bars");

    std::printf("  macdVC=%u signalVC=%u posHist=%u negHist=%u\n",
                data.macdVC, data.signalVC, data.posHistCount, data.negHistCount);
    std::printf("  MACD computation: OK\n");
  }

  // Test 3: MacdRecipe build — 14 slots
  std::printf("=== MacdRecipe build ===\n");
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"Lines"})"), "lineLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":3,"paneId":1,"name":"Hist"})"), "histLayer");

    dc::MacdRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.lineLayerId = 2;
    cfg.histLayerId = 3;
    cfg.name = "MACD";
    cfg.createTransform = true;
    dc::MacdRecipe macd(700, cfg);

    auto result = macd.build();
    std::printf("  create commands: %zu\n", result.createCommands.size());

    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "macd create");
    }

    // Verify key resources
    requireTrue(scene.hasDrawItem(702), "MACD line DI");
    requireTrue(scene.hasDrawItem(705), "signal line DI");
    requireTrue(scene.hasDrawItem(708), "posHist DI");
    requireTrue(scene.hasDrawItem(711), "negHist DI");
    requireTrue(scene.hasTransform(712), "shared transform");

    requireTrue(scene.getDrawItem(702)->pipeline == "line2d@1", "MACD line pipeline");
    requireTrue(scene.getDrawItem(708)->pipeline == "instancedRect@1", "posHist pipeline");
    std::printf("  resources: OK\n");

    // Dispose
    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "macd dispose");
    }
    requireTrue(!scene.hasDrawItem(702), "macd disposed");
    requireTrue(!scene.hasTransform(712), "xform disposed");
    std::printf("  dispose: OK\n");
  }

  std::printf("\nD4.5 MACD PASS\n");
  return 0;
}
