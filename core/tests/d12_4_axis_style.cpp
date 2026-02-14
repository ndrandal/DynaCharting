// D12.4 — Axis Styling test
// Pure C++: build with custom colors, apply to Scene, read back DrawItem fields,
// verify they match config.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
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

static void requireColorEq(const float actual[4], const float expected[4], const char* name) {
  for (int i = 0; i < 4; i++) {
    if (std::fabs(actual[i] - expected[i]) > 0.001f) {
      std::fprintf(stderr, "ASSERT FAIL: %s color[%d] = %.3f, expected %.3f\n",
                   name, i, actual[i], expected[i]);
      std::exit(1);
    }
  }
}

int main() {
  std::printf("=== D12.4 Axis Styling ===\n");

  // --- Test 1: Custom tick/label colors applied via setDrawItemStyle ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "tickLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1})"), "labelLayer");

    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";
    cfg.tickColor[0] = 1.0f; cfg.tickColor[1] = 0.0f;
    cfg.tickColor[2] = 0.0f; cfg.tickColor[3] = 1.0f;
    cfg.labelColor[0] = 0.0f; cfg.labelColor[1] = 1.0f;
    cfg.labelColor[2] = 0.0f; cfg.labelColor[3] = 0.8f;
    cfg.tickLineWidth = 2.5f;

    dc::AxisRecipe axis(200, cfg);
    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "style create");

    // Verify tick draw item color
    const auto* yDi = scene.getDrawItem(axis.yTickDrawItemId());
    requireColorEq(yDi->color, cfg.tickColor, "yTick color");
    requireTrue(std::fabs(yDi->lineWidth - 2.5f) < 0.001f, "yTick lineWidth");

    const auto* xDi = scene.getDrawItem(axis.xTickDrawItemId());
    requireColorEq(xDi->color, cfg.tickColor, "xTick color");
    requireTrue(std::fabs(xDi->lineWidth - 2.5f) < 0.001f, "xTick lineWidth");

    // Verify label draw item color
    const auto* lDi = scene.getDrawItem(axis.labelDrawItemId());
    requireColorEq(lDi->color, cfg.labelColor, "label color");

    std::printf("  Test 1 (base tick/label colors) PASS\n");
  }

  // --- Test 2: Grid + spine colors ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "gridLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1})"), "tickLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1})"), "labelLayer");

    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 11;
    cfg.labelLayerId = 12;
    cfg.name = "axis";
    cfg.enableGrid = true;
    cfg.gridLayerId = 10;
    cfg.enableSpine = true;
    cfg.gridColor[0] = 0.1f; cfg.gridColor[1] = 0.2f;
    cfg.gridColor[2] = 0.3f; cfg.gridColor[3] = 0.4f;
    cfg.gridLineWidth = 1.5f;
    cfg.spineColor[0] = 0.9f; cfg.spineColor[1] = 0.8f;
    cfg.spineColor[2] = 0.7f; cfg.spineColor[3] = 1.0f;
    cfg.spineLineWidth = 3.0f;

    dc::AxisRecipe axis(200, cfg);
    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "grid+spine create");

    // Grid DI colors
    const auto* hDi = scene.getDrawItem(axis.hGridDrawItemId());
    requireColorEq(hDi->color, cfg.gridColor, "hGrid color");
    requireTrue(std::fabs(hDi->lineWidth - 1.5f) < 0.001f, "hGrid lineWidth");

    const auto* vDi = scene.getDrawItem(axis.vGridDrawItemId());
    requireColorEq(vDi->color, cfg.gridColor, "vGrid color");

    // Spine DI colors
    const auto* sDi = scene.getDrawItem(axis.spineDrawItemId());
    requireColorEq(sDi->color, cfg.spineColor, "spine color");
    requireTrue(std::fabs(sDi->lineWidth - 3.0f) < 0.001f, "spine lineWidth");

    std::printf("  Test 2 (grid + spine colors) PASS\n");
  }

  // --- Test 3: AA tick colors ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "tickLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1})"), "labelLayer");

    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";
    cfg.enableAALines = true;
    cfg.tickColor[0] = 0.3f; cfg.tickColor[1] = 0.4f;
    cfg.tickColor[2] = 0.5f; cfg.tickColor[3] = 0.9f;
    cfg.tickLineWidth = 1.5f;

    dc::AxisRecipe axis(200, cfg);
    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "aa create");

    const auto* yAaDi = scene.getDrawItem(axis.yTickAADrawItemId());
    requireColorEq(yAaDi->color, cfg.tickColor, "yTickAA color");
    requireTrue(std::fabs(yAaDi->lineWidth - 1.5f) < 0.001f, "yTickAA lineWidth");

    const auto* xAaDi = scene.getDrawItem(axis.xTickAADrawItemId());
    requireColorEq(xAaDi->color, cfg.tickColor, "xTickAA color");

    std::printf("  Test 3 (AA tick colors) PASS\n");
  }

  std::printf("D12.4 axis_style: ALL PASS\n");
  return 0;
}
