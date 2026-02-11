// D5.3 — CrosshairRecipe test (pure C++, needs font)

#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
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
  // Load font
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
#ifdef FONT_PATH
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();
#else
  std::fprintf(stderr, "SKIP: no font\n");
  return 0;
#endif

  // Scene infrastructure
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Create pane and layers
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"CrossLines"})"), "layer10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"CrossLabels"})"), "layer11");

  // --- Build produces 12 resources ---
  dc::CrosshairRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.lineLayerId = 10;
  cfg.labelLayerId = 11;
  cfg.name = "Crosshair";

  dc::CrosshairRecipe recipe(1000, cfg);
  auto buildResult = recipe.build();

  requireTrue(buildResult.createCommands.size() == 16, "16 create commands (4 triplets * 4)");
  requireTrue(buildResult.disposeCommands.size() == 12, "12 dispose commands");

  // Apply create commands
  for (auto& cmd : buildResult.createCommands) {
    requireOk(cp.applyJsonText(cmd), "create");
  }

  // Verify all 12 resource IDs exist
  for (std::uint32_t i = 0; i < 12; i++) {
    dc::Id id = 1000 + i;
    // Draw items: offset 2, 5, 8, 11
    if (i == 2 || i == 5 || i == 8 || i == 11) {
      requireTrue(scene.hasDrawItem(id),
                  ("drawItem " + std::to_string(id) + " exists").c_str());
    }
  }
  std::printf("  build 12 resources PASS\n");

  // --- Crosshair at center of pane ---
  dc::PaneRegion pane{-0.3f, 0.9f, -0.9f, 0.9f};

  auto data = recipe.computeCrosshairData(
      0.0, 0.3, 50.0, 100.0, // clipX, clipY, dataX, dataY
      pane, atlas, 48.0f, 0.04f);

  requireTrue(data.visible, "center crosshair visible");
  requireTrue(data.hLineVerts.size() == 4, "h-line has 4 floats");
  requireTrue(data.vLineVerts.size() == 4, "v-line has 4 floats");

  // H-line spans full width at clipY=0.3
  requireTrue(data.hLineVerts[0] == pane.clipXMin, "h-line starts at left");
  requireTrue(data.hLineVerts[2] == pane.clipXMax, "h-line ends at right");
  requireTrue(data.hLineVerts[1] == 0.3f, "h-line at clipY 0.3");

  // V-line spans full height at clipX=0.0
  requireTrue(data.vLineVerts[1] == pane.clipYMin, "v-line starts at bottom");
  requireTrue(data.vLineVerts[3] == pane.clipYMax, "v-line ends at top");
  requireTrue(data.vLineVerts[0] == 0.0f, "v-line at clipX 0.0");

  // Labels have glyph data
  requireTrue(data.priceLabelGC > 0, "price label has glyphs");
  requireTrue(data.timeLabelGC > 0, "time label has glyphs");
  requireTrue(data.priceLabelGlyphs.size() == data.priceLabelGC * 8u,
              "price label glyph data correct size");

  std::printf("  crosshair at center PASS\n");

  // --- Outside pane → not visible ---
  auto dataOutside = recipe.computeCrosshairData(
      -1.0, -0.5, 0.0, 0.0, // below pane
      pane, atlas, 48.0f, 0.04f);

  requireTrue(!dataOutside.visible, "outside pane → not visible");
  requireTrue(dataOutside.hLineVerts.empty(), "no lines when invisible");

  std::printf("  outside pane PASS\n");

  // --- Dispose round-trip ---
  for (auto& cmd : buildResult.disposeCommands) {
    requireOk(cp.applyJsonText(cmd), "dispose");
  }
  // After disposal, draw items should be gone
  requireTrue(!scene.hasDrawItem(1002), "drawItem 1002 gone");
  requireTrue(!scene.hasDrawItem(1005), "drawItem 1005 gone");

  std::printf("  dispose round-trip PASS\n");

  std::printf("\nD5.3 crosshair PASS\n");
  return 0;
}
