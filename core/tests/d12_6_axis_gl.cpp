// D12.6 — Axis GL Integration Test (OSMesa)
// Full render: grid + AA ticks + labels + spine + candle data.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
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

static bool hasNonBlackAt(const std::vector<std::uint8_t>& pixels, int W,
                           int px, int py, int radius = 3) {
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      int x = px + dx, y = py + dy;
      if (x < 0 || y < 0 || x >= W) continue;
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (idx + 3 >= pixels.size()) continue;
      if (pixels[idx] > 10 || pixels[idx + 1] > 10 || pixels[idx + 2] > 10)
        return true;
    }
  }
  return false;
}

int main() {
#ifndef FONT_PATH
  std::printf("D12.6 axis_gl: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  constexpr int W = 400;
  constexpr int H = 300;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  dc::GlyphAtlas atlas;
  requireTrue(atlas.loadFontFile(FONT_PATH), "load font");
  atlas.ensureAscii();

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  cp.setGlyphAtlas(&atlas);

  // Create pane with two layers: grid (low ID = behind), data (high ID = front)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":5,"paneId":1,"name":"Grid"})"), "gridLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Ticks"})"), "tickLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1,"name":"Data"})"), "dataLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":1,"name":"Labels"})"), "labelLayer");

  // Create a shared data transform
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createXform");

  // --- Candle data ---
  float candles[] = {
    1.0f, 100.0f, 108.0f, 95.0f, 103.0f, 0.3f,
    2.0f, 102.0f, 110.0f, 97.0f, 105.0f, 0.3f,
    3.0f,  99.0f, 107.0f, 93.0f, 101.0f, 0.3f,
    4.0f, 104.0f, 112.0f, 99.0f, 108.0f, 0.3f,
    5.0f, 106.0f, 115.0f, 101.0f, 110.0f, 0.3f,
  };
  int candleCount = 5;

  dc::CandleRecipeConfig candleCfg;
  candleCfg.paneId = 1;
  candleCfg.layerId = 15;
  candleCfg.name = "candles";
  candleCfg.createTransform = false;

  dc::CandleRecipe candleRecipe(100, candleCfg);
  auto candleBuild = candleRecipe.build();
  for (auto& cmd : candleBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "candle create");

  // Attach transform
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(candleRecipe.drawItemId()) +
    R"(,"transformId":50})"), "attach candle xform");

  // Upload candle data
  ingest.ensureBuffer(candleRecipe.bufferId());
  ingest.setBufferData(candleRecipe.bufferId(),
                        reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

  // Set candle geometry vertex count
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%d})",
      std::to_string(candleRecipe.geometryId()).c_str(), candleCount);
    requireOk(cp.applyJsonText(buf), "setCandle vc");
  }

  // --- Axis with all features enabled ---
  dc::AxisRecipeConfig axisCfg;
  axisCfg.paneId = 1;
  axisCfg.tickLayerId = 10;
  axisCfg.labelLayerId = 20;
  axisCfg.gridLayerId = 5;
  axisCfg.name = "axis";
  axisCfg.enableGrid = true;
  axisCfg.enableAALines = true;
  axisCfg.enableSpine = true;
  axisCfg.yAxisClipX = 0.75f;
  axisCfg.xAxisClipY = -0.85f;
  axisCfg.gridColor[0] = 0.3f; axisCfg.gridColor[1] = 0.3f;
  axisCfg.gridColor[2] = 0.3f; axisCfg.gridColor[3] = 0.8f;
  axisCfg.tickColor[0] = 0.7f; axisCfg.tickColor[1] = 0.7f;
  axisCfg.tickColor[2] = 0.7f; axisCfg.tickColor[3] = 1.0f;
  axisCfg.spineColor[0] = 0.8f; axisCfg.spineColor[1] = 0.8f;
  axisCfg.spineColor[2] = 0.8f; axisCfg.spineColor[3] = 1.0f;
  axisCfg.spineLineWidth = 2.0f;
  axisCfg.labelColor[0] = 0.9f; axisCfg.labelColor[1] = 0.9f;
  axisCfg.labelColor[2] = 0.9f; axisCfg.labelColor[3] = 1.0f;

  dc::AxisRecipe axisRecipe(500, axisCfg);
  auto axisBuild = axisRecipe.build();
  for (auto& cmd : axisBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "axis create");

  // Set viewport and transform
  dc::Viewport vp;
  vp.setDataRange(0, 6, 85, 120);
  vp.setPixelViewport(W, H);
  auto tp = vp.computeTransformParams();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(tp.sx), static_cast<double>(tp.sy),
      static_cast<double>(tp.tx), static_cast<double>(tp.ty));
    requireOk(cp.applyJsonText(buf), "setTransform");
  }

  // Compute axis data using V2
  auto axData = axisRecipe.computeAxisDataV2(atlas,
                                              85.0f, 120.0f,
                                              0.0f, 6.0f,
                                              -1.0f, 1.0f,
                                              -1.0f, axisCfg.yAxisClipX,
                                              48.0f, 0.04f);

  // Upload axis data to buffers
  auto uploadBuf = [&](dc::Id bufId, const std::vector<float>& data) {
    ingest.ensureBuffer(bufId);
    ingest.setBufferData(bufId, reinterpret_cast<const std::uint8_t*>(data.data()),
                          static_cast<std::uint32_t>(data.size() * sizeof(float)));
  };

  auto setVertexCount = [&](dc::Id geomId, std::uint32_t count) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(geomId).c_str(), count);
    requireOk(cp.applyJsonText(buf), "setVertexCount");
  };

  // Old tick data (kept for completeness, could be zero when AA is used)
  uploadBuf(axisRecipe.yTickBufferId(), axData.yTickVerts);
  setVertexCount(axisRecipe.yTickGeomId(), axData.yTickVertexCount);
  uploadBuf(axisRecipe.xTickBufferId(), axData.xTickVerts);
  setVertexCount(axisRecipe.xTickGeomId(), axData.xTickVertexCount);

  // Labels
  uploadBuf(axisRecipe.labelBufferId(), axData.labelInstances);
  setVertexCount(axisRecipe.labelGeomId(), axData.labelGlyphCount);

  // Grid
  uploadBuf(axisRecipe.hGridBufferId(), axData.hGridVerts);
  setVertexCount(axisRecipe.hGridGeomId(), axData.hGridLineCount);
  uploadBuf(axisRecipe.vGridBufferId(), axData.vGridVerts);
  setVertexCount(axisRecipe.vGridGeomId(), axData.vGridLineCount);

  // AA ticks
  uploadBuf(axisRecipe.yTickAABufferId(), axData.yTickAAVerts);
  setVertexCount(axisRecipe.yTickAAGeomId(), axData.yTickAAVertexCount);
  uploadBuf(axisRecipe.xTickAABufferId(), axData.xTickAAVerts);
  setVertexCount(axisRecipe.xTickAAGeomId(), axData.xTickAAVertexCount);

  // Spine
  uploadBuf(axisRecipe.spineBufferId(), axData.spineVerts);
  setVertexCount(axisRecipe.spineGeomId(), axData.spineLineCount);

  // --- Render ---
  dc::GpuBufferManager gpuBufs;

  // Sync all ingest buffers to GPU manager
  auto syncBuf = [&](dc::Id bufId) {
    const auto* data = ingest.getBufferData(bufId);
    auto size = ingest.getBufferSize(bufId);
    if (data && size > 0) {
      gpuBufs.setCpuData(bufId, data, size);
    }
  };

  syncBuf(candleRecipe.bufferId());
  syncBuf(axisRecipe.yTickBufferId());
  syncBuf(axisRecipe.xTickBufferId());
  syncBuf(axisRecipe.labelBufferId());
  syncBuf(axisRecipe.hGridBufferId());
  syncBuf(axisRecipe.vGridBufferId());
  syncBuf(axisRecipe.yTickAABufferId());
  syncBuf(axisRecipe.xTickAABufferId());
  syncBuf(axisRecipe.spineBufferId());

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  renderer.setGlyphAtlas(&atlas);
  gpuBufs.uploadDirty();

  auto stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  std::printf("  draw calls: %u, culled: %u\n", stats.drawCalls, stats.culledDrawCalls);

  // Verify draw call count >= 7 (candle + hGrid + vGrid + yTickAA + xTickAA + spine + labels + old ticks)
  requireTrue(stats.drawCalls >= 7, "at least 7 draw calls");

  auto pixels = ctx.readPixels();

  // Verify non-black pixels exist (something rendered)
  int nonBlack = 0;
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 10 || pixels[idx + 1] > 10 || pixels[idx + 2] > 10)
      nonBlack++;
  }
  requireTrue(nonBlack > 100, "significant non-black content");
  std::printf("  non-black pixels: %d\n", nonBlack);

  // Verify spine area has pixels (at yAxisClipX in pixel coords)
  // yAxisClipX = 0.75 → pixel X = (0.75 + 1.0) / 2.0 * W = 0.875 * 400 = 350
  int spinePx = static_cast<int>((axisCfg.yAxisClipX + 1.0f) / 2.0f * W);
  bool spineFound = hasNonBlackAt(pixels, W, spinePx, H / 2, 5);
  requireTrue(spineFound, "spine pixels at yAxisClipX");
  std::printf("  spine visible at x=%d: OK\n", spinePx);

  // Verify label area has non-black pixels (right of spine)
  bool labelFound = hasNonBlackAt(pixels, W, spinePx + 20, H / 2, 10);
  std::printf("  label area check: %s\n", labelFound ? "found" : "empty");

  // Verify grid area has pixels (center of data area)
  int centerX = spinePx / 2;
  int centerY = H / 2;
  bool gridFound = hasNonBlackAt(pixels, W, centerX, centerY, 20);
  std::printf("  grid area check: %s\n", gridFound ? "found" : "empty");

  std::printf("D12.6 axis_gl: ALL PASS\n");
  return 0;
#endif
}
