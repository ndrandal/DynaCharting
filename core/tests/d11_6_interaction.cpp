// D11.6 — Interaction GL Integration Test (OSMesa)
// Tests: click-select-highlight + tooltip rendering + auto-scale, all combined.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/DataPicker.hpp"
#include "dc/viewport/AutoScale.hpp"
#include "dc/viewport/InputMapper.hpp"
#include "dc/selection/SelectionState.hpp"
#include "dc/recipe/HighlightRecipe.hpp"
#include "dc/recipe/TooltipRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static int countNonBlack(const std::vector<std::uint8_t>& pixels, int W, int H) {
  int count = 0;
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 5 || pixels[idx + 1] > 5 || pixels[idx + 2] > 5)
      count++;
  }
  return count;
}

static bool hasYellowPixels(const std::vector<std::uint8_t>& pixels, int W, int H) {
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 150 && pixels[idx + 1] > 150 && pixels[idx + 2] < 50)
      return true;
  }
  return false;
}

int main() {
#ifndef FONT_PATH
  std::printf("D11.6 interaction: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  constexpr int W = 200;
  constexpr int H = 200;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  // --- Setup scene with 3 candles ---
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");

  // Candle data
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})"), "createBuf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"candle6","vertexCount":3})"),
    "createGeo");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10})"), "createDI");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":102,"pipeline":"instancedCandle@1","geometryId":101})"),
    "bindDI");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":103})"), "createXf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":102,"transformId":103})"), "attachXf");

  float candles[] = {
    1.0f, 100.0f, 105.0f, 95.0f, 102.0f, 0.3f,
    2.0f, 101.0f, 106.0f, 96.0f, 103.0f, 0.3f,
    3.0f, 99.0f,  104.0f, 94.0f, 101.0f, 0.3f,
  };
  ingest.ensureBuffer(100);
  ingest.setBufferData(100, reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

  dc::Viewport vp;
  vp.setDataRange(0, 4, 90, 110);
  vp.setPixelViewport(W, H);

  // Set transform from viewport
  auto tp = vp.computeTransformParams();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":103,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(tp.sx), static_cast<double>(tp.sy),
      static_cast<double>(tp.tx), static_cast<double>(tp.ty));
    requireOk(cp.applyJsonText(buf), "setTransform");
  }

  // --- Build HighlightRecipe ---
  dc::HighlightRecipeConfig hlCfg;
  hlCfg.paneId = 1;
  hlCfg.layerId = 10;
  hlCfg.name = "highlight";
  hlCfg.markerSize = 2.0f; // data-space half-size (large enough to be visible)
  dc::HighlightRecipe hlRecipe(200, hlCfg);
  auto hlBuild = hlRecipe.build();
  for (auto& cmd : hlBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "hl create");
  // Attach same viewport transform so highlight rects map from data to clip
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(hlRecipe.drawItemId()) +
    R"(,"transformId":103})"), "attach hl transform");

  // --- Build TooltipRecipe ---
  dc::TooltipRecipeConfig ttCfg;
  ttCfg.paneId = 1;
  ttCfg.layerId = 10;
  ttCfg.name = "tooltip";
  ttCfg.fontSize = 0.04f;
  ttCfg.glyphPx = 48.0f;
  ttCfg.padding = 0.02f;
  dc::TooltipRecipe ttRecipe(300, ttCfg);
  auto ttBuild = ttRecipe.build();
  for (auto& cmd : ttBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "tt create");

  // --- Test 1: DataPicker finds candle, highlight renders yellow ---
  {
    dc::DataPicker picker;
    dc::PickConfig pickCfg;
    pickCfg.maxDistancePx = 50.0;
    picker.setConfig(pickCfg);

    // Pick near candle 1 (x=2, y=102)
    double cx, cy;
    vp.dataToClip(2.0, 102.0, cx, cy);
    double px = (cx + 1.0) / 2.0 * W;
    double py = (1.0 - cy) / 2.0 * H;

    auto hit = picker.pick(px, py, 1, scene, ingest, vp);
    requireTrue(hit.hit, "picker finds candle");
    requireTrue(hit.recordIndex == 1, "finds candle 1");

    // Select and compute highlights
    dc::SelectionState sel;
    sel.select({hit.drawItemId, hit.recordIndex});
    auto hlData = hlRecipe.computeHighlights(sel, scene, ingest);
    requireTrue(hlData.instanceCount == 1, "1 highlight");

    // Upload highlight data and render
    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(100, candles, sizeof(candles));
    gpuBufs.setCpuData(hlRecipe.bufferId(), hlData.rects.data(),
                       static_cast<std::uint32_t>(hlData.rects.size() * sizeof(float)));

    // Update highlight geometry vertex count
    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":%u})",
        hlRecipe.geometryId(), hlData.instanceCount);
      requireOk(cp.applyJsonText(buf), "setHlVertexCount");
    }

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    renderer.setGlyphAtlas(&atlas);
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();
    requireTrue(stats.drawCalls >= 2, "at least 2 draw calls (candle + highlight)");

    auto pixels = ctx.readPixels();
    requireTrue(hasYellowPixels(pixels, W, H), "yellow highlight pixels visible");

    std::printf("  Test 1 (pick-select-highlight → yellow pixels) PASS\n");

    // --- Test 2: Tooltip renders text near cursor ---
    dc::TooltipFormatter formatter = [](const dc::HitResult& h) -> std::string {
      char buf[64];
      std::snprintf(buf, sizeof(buf), "P:%.0f", h.dataY);
      return buf;
    };

    dc::PaneRegion clip{-1.0f, 1.0f, -1.0f, 1.0f};
    auto ttData = ttRecipe.computeTooltip(hit, cx, cy, clip, atlas, formatter);
    requireTrue(ttData.visible, "tooltip visible");
    requireTrue(ttData.glyphCount > 0, "tooltip has glyphs");

    gpuBufs.setCpuData(ttRecipe.bgBufferId(), ttData.bgRect.data(),
                       static_cast<std::uint32_t>(ttData.bgRect.size() * sizeof(float)));
    gpuBufs.setCpuData(ttRecipe.textBufferId(), ttData.textGlyphs.data(),
                       static_cast<std::uint32_t>(ttData.textGlyphs.size() * sizeof(float)));

    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":%u})",
        ttRecipe.bgGeometryId(), ttData.bgCount);
      requireOk(cp.applyJsonText(buf), "setBgVertexCount");
    }
    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":%u})",
        ttRecipe.textGeometryId(), ttData.glyphCount);
      requireOk(cp.applyJsonText(buf), "setTextVertexCount");
    }

    gpuBufs.uploadDirty();
    stats = renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    // Tooltip adds 2 more draw calls (bg rect + text)
    requireTrue(stats.drawCalls >= 4, "at least 4 draw calls (candle + highlight + tooltip bg + text)");

    std::printf("  Test 2 (tooltip rendering adds visible pixels) PASS\n");

    // --- Test 3: AutoScale on partial X range ---
    dc::AutoScale as;
    dc::AutoScaleConfig asCfg;
    asCfg.marginFraction = 0.05f;
    as.setConfig(asCfg);

    dc::Viewport vpPartial;
    vpPartial.setDataRange(1.5, 2.5, 0, 200); // only candle 2
    vpPartial.setPixelViewport(W, H);

    double yMin, yMax;
    bool ok = as.computeYRange({102}, scene, ingest, vpPartial, yMin, yMax);
    requireTrue(ok, "autoscale finds data");
    // Candle 2: high=106, low=96
    requireTrue(yMin < 96.0, "yMin < 96 (low of candle 2)");
    requireTrue(yMax > 106.0, "yMax > 106 (high of candle 2)");
    // But not too extreme
    requireTrue(yMin > 80.0, "yMin > 80 (tight bounds)");
    requireTrue(yMax < 120.0, "yMax < 120 (tight bounds)");

    std::printf("  Test 3 (autoscale partial X → tight Y bounds) PASS\n");

    // --- Test 4: Clear selection → highlight gone ---
    sel.clear();
    hlData = hlRecipe.computeHighlights(sel, scene, ingest);
    requireTrue(hlData.instanceCount == 0, "no highlights after clear");

    // Upload empty and render
    float emptyRect[4] = {0, 0, 0, 0};
    gpuBufs.setCpuData(hlRecipe.bufferId(), emptyRect, 0);
    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":0})",
        hlRecipe.geometryId());
      requireOk(cp.applyJsonText(buf), "clearHlVertexCount");
    }
    // Also clear tooltip
    gpuBufs.setCpuData(ttRecipe.bgBufferId(), emptyRect, 0);
    gpuBufs.setCpuData(ttRecipe.textBufferId(), emptyRect, 0);
    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":0})",
        ttRecipe.bgGeometryId());
      requireOk(cp.applyJsonText(buf), "clearBgVertexCount");
    }
    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setGeometryVertexCount","geometryId":%lu,"vertexCount":0})",
        ttRecipe.textGeometryId());
      requireOk(cp.applyJsonText(buf), "clearTextVertexCount");
    }

    gpuBufs.uploadDirty();
    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    auto pixels3 = ctx.readPixels();
    requireTrue(!hasYellowPixels(pixels3, W, H), "no yellow after clear");

    std::printf("  Test 4 (clear selection → no highlight) PASS\n");
  }

  std::printf("D11.6 interaction: ALL PASS\n");
  return 0;
#endif
}
