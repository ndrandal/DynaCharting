// D4.3 — AxisRecipe test
// Pure C++: verify NiceTicks output, verify recipe creates 10 resources.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/math/NiceTicks.hpp"
#include "dc/recipe/AxisRecipe.hpp"

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

int main() {
  // Test 1: NiceTicks
  std::printf("=== NiceTicks ===\n");
  {
    auto t = dc::computeNiceTicks(0.0f, 100.0f, 5);
    std::printf("  range [0, 100], target 5: step=%.1f, %zu ticks\n",
                t.step, t.values.size());
    requireTrue(t.step > 0.0f, "step > 0");
    requireTrue(t.values.size() >= 3, "at least 3 ticks");
    requireTrue(t.min <= 0.0f, "min <= 0");
    requireTrue(t.max >= 100.0f, "max >= 100");

    // Verify ticks are evenly spaced
    for (std::size_t i = 1; i < t.values.size(); i++) {
      float diff = t.values[i] - t.values[i-1];
      requireTrue(std::fabs(diff - t.step) < 0.01f, "ticks evenly spaced");
    }
    std::printf("  NiceTicks: OK\n");
  }

  // Test 2: NiceTicks small range
  {
    auto t = dc::computeNiceTicks(95.0f, 105.0f, 5);
    std::printf("  range [95, 105], target 5: step=%.1f, %zu ticks\n",
                t.step, t.values.size());
    requireTrue(t.step == 2.0f || t.step == 2.5f || t.step == 5.0f,
                "step is a nice number");
    std::printf("  NiceTicks small: OK\n");
  }

  // Test 3: AxisRecipe build
  std::printf("=== AxisRecipe build ===\n");
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Ticks"})"), "tickLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Labels"})"), "labelLayer");

    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "YAxis";

    dc::AxisRecipe axis(200, cfg);

    requireTrue(axis.yTickBufferId() == 200, "yTickBuf=200");
    requireTrue(axis.labelTransformId() == 209, "labelXform=209");

    auto result = axis.build();
    std::printf("  create commands: %zu\n", result.createCommands.size());
    std::printf("  dispose commands: %zu\n", result.disposeCommands.size());

    // Apply all create commands
    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "axis create");
    }

    // Verify 10 resources created
    requireTrue(scene.hasBuffer(200), "yTickBuf");
    requireTrue(scene.hasGeometry(201), "yTickGeom");
    requireTrue(scene.hasDrawItem(202), "yTickDI");
    requireTrue(scene.hasBuffer(203), "xTickBuf");
    requireTrue(scene.hasGeometry(204), "xTickGeom");
    requireTrue(scene.hasDrawItem(205), "xTickDI");
    requireTrue(scene.hasBuffer(206), "labelBuf");
    requireTrue(scene.hasGeometry(207), "labelGeom");
    requireTrue(scene.hasDrawItem(208), "labelDI");
    requireTrue(scene.hasTransform(209), "labelXform");
    std::printf("  10 resources: OK\n");

    // Verify pipelines
    const auto* yDi = scene.getDrawItem(202);
    requireTrue(yDi->pipeline == "line2d@1", "yTick pipeline");
    const auto* lDi = scene.getDrawItem(208);
    requireTrue(lDi->pipeline == "textSDF@1", "label pipeline");

    // Dispose
    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "axis dispose");
    }
    requireTrue(!scene.hasBuffer(200), "all disposed");
    requireTrue(!scene.hasTransform(209), "xform disposed");
    std::printf("  dispose: OK\n");
  }

  std::printf("\nD4.3 axis PASS\n");
  return 0;
}
