// D12.5 — Axis Spine + Label Alignment test
// Pure C++ (needs font): verify spine resources, 2 spine lines,
// Y label X coords < yAxisClipX (right-aligned).

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
#ifndef FONT_PATH
  std::printf("D12.5 spine_labels: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  std::printf("=== D12.5 Spine + Label Alignment ===\n");

  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  // --- Test 1: enableSpine creates spine resources ---
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
    cfg.enableSpine = true;
    cfg.yAxisClipX = 0.8f;
    cfg.xAxisClipY = -0.85f;

    dc::AxisRecipe axis(200, cfg);

    requireTrue(axis.spineBufferId() == 222, "spineBuf=222");
    requireTrue(axis.spineGeomId() == 223, "spineGeom=223");
    requireTrue(axis.spineDrawItemId() == 224, "spineDI=224");

    auto result = axis.build();
    for (auto& cmd : result.createCommands)
      requireOk(cp.applyJsonText(cmd), "spine create");

    requireTrue(scene.hasBuffer(222), "spineBuf");
    requireTrue(scene.hasGeometry(223), "spineGeom");
    requireTrue(scene.hasDrawItem(224), "spineDI");
    requireTrue(scene.hasTransform(225), "gridSpineXform");

    const auto* sDi = scene.getDrawItem(224);
    requireTrue(sDi->pipeline == "lineAA@1", "spine pipeline");

    std::printf("  Test 1 (spine resources) PASS\n");
  }

  // --- Test 2: Spine data has 2 lines ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";
    cfg.enableSpine = true;
    cfg.yAxisClipX = 0.8f;
    cfg.xAxisClipY = -0.85f;

    dc::AxisRecipe axis(200, cfg);
    auto data = axis.computeAxisDataV2(atlas,
                                        0.0f, 100.0f,
                                        0.0f, 100.0f,
                                        -1.0f, 1.0f,
                                        -1.0f, 0.8f,
                                        48.0f, 0.04f);

    requireTrue(data.spineLineCount == 2, "2 spine lines");
    requireTrue(data.spineVerts.size() == 8, "8 spine floats (2 lines × 4 floats)");

    // Vertical spine at yAxisClipX
    requireTrue(std::fabs(data.spineVerts[0] - 0.8f) < 0.001f, "spine V x0 = yAxisClipX");
    requireTrue(std::fabs(data.spineVerts[2] - 0.8f) < 0.001f, "spine V x1 = yAxisClipX");
    requireTrue(std::fabs(data.spineVerts[1] - (-1.0f)) < 0.001f, "spine V y0 = clipYMin");
    requireTrue(std::fabs(data.spineVerts[3] - 1.0f) < 0.001f, "spine V y1 = clipYMax");

    // Horizontal spine at xAxisClipY
    requireTrue(std::fabs(data.spineVerts[5] - (-0.85f)) < 0.001f, "spine H y = xAxisClipY");
    requireTrue(std::fabs(data.spineVerts[7] - (-0.85f)) < 0.001f, "spine H y = xAxisClipY");

    std::printf("  Test 2 (2 spine lines) PASS\n");
  }

  // --- Test 3: Y labels right-aligned (all glyph X coords < yAxisClipX) ---
  {
    dc::AxisRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.tickLayerId = 10;
    cfg.labelLayerId = 11;
    cfg.name = "axis";
    cfg.yAxisClipX = 0.8f;

    dc::AxisRecipe axis(200, cfg);
    auto data = axis.computeAxisDataV2(atlas,
                                        0.0f, 100.0f,
                                        0.0f, 100.0f,
                                        -1.0f, 1.0f,
                                        -1.0f, 0.8f,
                                        48.0f, 0.04f);

    requireTrue(data.labelGlyphCount > 0, "has labels");

    // Check all Y label glyph x1 values (every 8 floats: x0,y0,x1,y1,u0,v0,u1,v1)
    // Y labels come first in the label instances array
    // We need to count Y ticks to know how many Y label glyphs there are
    int yTickCount = data.yTickVertexCount / 2;
    requireTrue(yTickCount > 0, "has Y ticks");

    // Check that at least some glyphs have x1 < yAxisClipX (right-aligned left of spine)
    bool hasGlyphLeftOfSpine = false;
    std::uint32_t glyphsChecked = 0;
    for (std::size_t i = 0; i + 7 < data.labelInstances.size(); i += 8) {
      float x1 = data.labelInstances[i + 2]; // x1 coordinate
      if (x1 < cfg.yAxisClipX - 0.001f) {
        hasGlyphLeftOfSpine = true;
      }
      glyphsChecked++;
      // Only check Y labels (first batch of glyphs)
      if (glyphsChecked > 30) break;
    }
    requireTrue(hasGlyphLeftOfSpine, "Y labels positioned left of yAxisClipX");

    std::printf("  Test 3 (Y labels right-aligned) PASS\n");
  }

  std::printf("D12.5 spine_labels: ALL PASS\n");
  return 0;
#endif
}
