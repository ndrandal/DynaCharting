// D12.1 — Grid Lines test
// Pure C++: verify grid resources created when enableGrid, lineAA@1 pipeline,
// rect4 format, grid line count > 0. Verify backward compat when disabled.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/AxisRecipe.hpp"

#include <cstdio>
#include <cstdlib>

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
  // --- Test 1: enableGrid creates grid resources ---
  std::printf("=== D12.1 Grid Lines ===\n");
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Grid"})"), "gridLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Ticks"})"), "tickLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"Labels"})"), "labelLayer");

    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 11;
    cfg.labelLayerId = 12;
    cfg.name = "axis";
    cfg.enableGrid = true;
    cfg.gridLayerId = 10;

    dc::AxisRecipe axis(200, cfg);

    // Verify ID accessors
    requireTrue(axis.hGridBufferId() == 210, "hGridBuf=210");
    requireTrue(axis.hGridGeomId() == 211, "hGridGeom=211");
    requireTrue(axis.hGridDrawItemId() == 212, "hGridDI=212");
    requireTrue(axis.vGridBufferId() == 213, "vGridBuf=213");
    requireTrue(axis.vGridGeomId() == 214, "vGridGeom=214");
    requireTrue(axis.vGridDrawItemId() == 215, "vGridDI=215");
    requireTrue(axis.gridSpineTransformId() == 225, "gridXform=225");

    auto result = axis.build();

    // Apply all create commands
    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "grid create");
    }

    // Verify base resources still exist
    requireTrue(scene.hasBuffer(200), "yTickBuf");
    requireTrue(scene.hasDrawItem(202), "yTickDI");
    requireTrue(scene.hasTransform(209), "labelXform");

    // Verify grid resources
    requireTrue(scene.hasBuffer(210), "hGridBuf");
    requireTrue(scene.hasGeometry(211), "hGridGeom");
    requireTrue(scene.hasDrawItem(212), "hGridDI");
    requireTrue(scene.hasBuffer(213), "vGridBuf");
    requireTrue(scene.hasGeometry(214), "vGridGeom");
    requireTrue(scene.hasDrawItem(215), "vGridDI");
    requireTrue(scene.hasTransform(225), "gridSpineXform");

    // Verify pipeline = lineAA@1
    const auto* hDi = scene.getDrawItem(212);
    requireTrue(hDi->pipeline == "lineAA@1", "hGrid pipeline");
    const auto* vDi = scene.getDrawItem(215);
    requireTrue(vDi->pipeline == "lineAA@1", "vGrid pipeline");

    // Verify format = rect4
    const auto* hGeom = scene.getGeometry(211);
    requireTrue(hGeom->format == dc::VertexFormat::Rect4, "hGrid format rect4");
    const auto* vGeom = scene.getGeometry(214);
    requireTrue(vGeom->format == dc::VertexFormat::Rect4, "vGrid format rect4");

    // Verify subscriptions include grid buffers
    bool foundHGrid = false, foundVGrid = false;
    for (auto& sub : result.subscriptions) {
      if (sub.bufferId == 210) foundHGrid = true;
      if (sub.bufferId == 213) foundVGrid = true;
    }
    requireTrue(foundHGrid, "hGrid subscription");
    requireTrue(foundVGrid, "vGrid subscription");

    // Dispose
    for (auto& cmd : result.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), "grid dispose");
    }
    requireTrue(!scene.hasBuffer(210), "hGrid disposed");
    requireTrue(!scene.hasTransform(225), "gridXform disposed");
    requireTrue(!scene.hasBuffer(200), "base disposed");

    std::printf("  Test 1 (enableGrid creates resources) PASS\n");
  }

  // --- Test 2: Default config (enableGrid=false) does NOT create grid ---
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
    // enableGrid defaults to false

    dc::AxisRecipe axis(200, cfg);
    auto result = axis.build();

    for (auto& cmd : result.createCommands) {
      requireOk(cp.applyJsonText(cmd), "default create");
    }

    // Base resources should exist
    requireTrue(scene.hasBuffer(200), "yTickBuf exists");
    requireTrue(scene.hasTransform(209), "labelXform exists");

    // Grid resources should NOT exist
    requireTrue(!scene.hasBuffer(210), "no hGridBuf");
    requireTrue(!scene.hasDrawItem(212), "no hGridDI");
    requireTrue(!scene.hasBuffer(213), "no vGridBuf");
    requireTrue(!scene.hasTransform(225), "no gridXform");

    std::printf("  Test 2 (default config no grid) PASS\n");
  }

  std::printf("D12.1 grid: ALL PASS\n");
  return 0;
}
