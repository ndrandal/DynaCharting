// D12.2 — Anti-Aliased Tick Marks test
// Pure C++: verify AA tick resources created when enableAALines,
// lineAA@1 pipeline, rect4 format, tick length. Backward compat when disabled.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

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
  std::printf("=== D12.2 AA Tick Marks ===\n");

  // --- Test 1: enableAALines creates AA tick resources ---
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
    cfg.yTickLength = 0.05f;
    cfg.xTickLength = 0.04f;

    dc::AxisRecipe axis(300, cfg);

    // Verify ID accessors
    requireTrue(axis.yTickAABufferId() == 316, "yTickAABuf=316");
    requireTrue(axis.yTickAAGeomId() == 317, "yTickAAGeom=317");
    requireTrue(axis.yTickAADrawItemId() == 318, "yTickAADI=318");
    requireTrue(axis.xTickAABufferId() == 319, "xTickAABuf=319");
    requireTrue(axis.xTickAAGeomId() == 320, "xTickAAGeom=320");
    requireTrue(axis.xTickAADrawItemId() == 321, "xTickAADI=321");

    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "aa create");

    // Old tick resources still exist
    requireTrue(scene.hasBuffer(300), "yTickBuf");
    requireTrue(scene.hasDrawItem(302), "yTickDI");
    const auto* oldDi = scene.getDrawItem(302);
    requireTrue(oldDi->pipeline == "line2d@1", "old yTick pipeline");

    // AA tick resources exist
    requireTrue(scene.hasBuffer(316), "yTickAABuf");
    requireTrue(scene.hasGeometry(317), "yTickAAGeom");
    requireTrue(scene.hasDrawItem(318), "yTickAADI");
    requireTrue(scene.hasBuffer(319), "xTickAABuf");
    requireTrue(scene.hasGeometry(320), "xTickAAGeom");
    requireTrue(scene.hasDrawItem(321), "xTickAADI");

    // Pipeline = lineAA@1
    const auto* yAaDi = scene.getDrawItem(318);
    requireTrue(yAaDi->pipeline == "lineAA@1", "yTickAA pipeline");
    const auto* xAaDi = scene.getDrawItem(321);
    requireTrue(xAaDi->pipeline == "lineAA@1", "xTickAA pipeline");

    // Format = rect4
    const auto* yAaGeom = scene.getGeometry(317);
    requireTrue(yAaGeom->format == dc::VertexFormat::Rect4, "yTickAA format rect4");
    const auto* xAaGeom = scene.getGeometry(320);
    requireTrue(xAaGeom->format == dc::VertexFormat::Rect4, "xTickAA format rect4");

    // Dispose
    for (auto& cmd : result.disposeCommands)
      requireOk(cp.applyJsonText(cmd), "aa dispose");
    requireTrue(!scene.hasBuffer(316), "yTickAA disposed");
    requireTrue(!scene.hasBuffer(319), "xTickAA disposed");

    std::printf("  Test 1 (enableAALines creates resources) PASS\n");
  }

  // --- Test 2: Default config does NOT create AA ticks ---
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

    dc::AxisRecipe axis(300, cfg);
    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "default create");

    requireTrue(!scene.hasBuffer(316), "no yTickAABuf");
    requireTrue(!scene.hasBuffer(319), "no xTickAABuf");

    std::printf("  Test 2 (default no AA ticks) PASS\n");
  }

  // --- Test 3: AA tick vertex data has correct tick length ---
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
    cfg.yTickLength = 0.05f;
    cfg.xTickLength = 0.04f;
    cfg.yAxisClipX = 0.8f;
    cfg.xAxisClipY = -0.9f;

    dc::AxisRecipe axis(300, cfg);
    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "create");

    // Need a font atlas for computeAxisData — use minimal stub
    dc::GlyphAtlas atlas;
    // Without a font loaded, labels won't render but tick data will
    auto data = axis.computeAxisData(atlas, 0.0f, 100.0f, 10,
                                      -1.0f, 1.0f, -1.0f, 1.0f,
                                      48.0f, 0.04f);

    requireTrue(data.yTickAAVertexCount > 0, "yTickAA count > 0");
    requireTrue(data.xTickAAVertexCount > 0, "xTickAA count > 0");

    // Check Y-tick AA: each entry is rect4(yAxisClipX - yTickLength, clipY, yAxisClipX, clipY)
    // First AA tick entry: 4 floats
    requireTrue(data.yTickAAVerts.size() >= 4, "yTickAA has data");
    float x0 = data.yTickAAVerts[0];
    float x1 = data.yTickAAVerts[2];
    float tickLen = x1 - x0;
    requireTrue(std::fabs(tickLen - 0.05f) < 0.001f, "yTick length matches config");

    // Check X-tick AA: each entry is rect4(clipX, xAxisClipY, clipX, xAxisClipY + xTickLength)
    requireTrue(data.xTickAAVerts.size() >= 4, "xTickAA has data");
    float y0 = data.xTickAAVerts[1];
    float y1 = data.xTickAAVerts[3];
    float xTickLen = y1 - y0;
    requireTrue(std::fabs(xTickLen - 0.04f) < 0.001f, "xTick length matches config");

    std::printf("  Test 3 (tick length matches config) PASS\n");
  }

  std::printf("D12.2 aa_ticks: ALL PASS\n");
  return 0;
}
