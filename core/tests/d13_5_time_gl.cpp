// D13.5 — Time-axis GL integration test (OSMesa)
// Full render: time-axis candle chart with grid + AA + spine + labels.

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

int main() {
#ifndef FONT_PATH
  std::printf("D13.5 time_gl: SKIPPED (no FONT_PATH)\n");
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

  // Create pane + layers
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":5,"paneId":1,"name":"Grid"})"), "gridLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Ticks"})"), "tickLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1,"name":"Data"})"), "dataLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":1,"name":"Labels"})"), "labelLayer");

  // Shared data transform
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createXform");

  // --- Candle data with timestamps ---
  // 1-hour range, 5-minute candles = 12 candles
  float startTs = 1700000000.0f;
  float interval = 300.0f;
  int candleCount = 12;

  float candles[12 * 6];
  float price = 100.0f;
  std::uint32_t seed = 42;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  float priceMin = 1e9f, priceMax = -1e9f;
  for (int i = 0; i < candleCount; i++) {
    float x = startTs + static_cast<float>(i) * interval;
    float open = price;
    float high = price + rng() * 2.0f;
    float low  = price - rng() * 2.0f;
    price += (rng() - 0.5f) * 3.0f;
    float close = price;
    float hw = interval * 0.4f;

    candles[i * 6 + 0] = x;
    candles[i * 6 + 1] = open;
    candles[i * 6 + 2] = high;
    candles[i * 6 + 3] = low;
    candles[i * 6 + 4] = close;
    candles[i * 6 + 5] = hw;

    if (low < priceMin) priceMin = low;
    if (high > priceMax) priceMax = high;
  }

  dc::CandleRecipeConfig candleCfg;
  candleCfg.paneId = 1;
  candleCfg.layerId = 15;
  candleCfg.name = "candles";
  candleCfg.createTransform = false;

  dc::CandleRecipe candleRecipe(100, candleCfg);
  auto candleBuild = candleRecipe.build();
  for (auto& cmd : candleBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "candle create");

  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(candleRecipe.drawItemId()) +
    R"(,"transformId":50})"), "attach candle xform");

  ingest.ensureBuffer(candleRecipe.bufferId());
  ingest.setBufferData(candleRecipe.bufferId(),
                        reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%d})",
      std::to_string(candleRecipe.geometryId()).c_str(), candleCount);
    requireOk(cp.applyJsonText(buf), "setCandle vc");
  }

  // --- Axis with time-axis and all features ---
  dc::AxisRecipeConfig axisCfg;
  axisCfg.paneId = 1;
  axisCfg.tickLayerId = 10;
  axisCfg.labelLayerId = 20;
  axisCfg.gridLayerId = 5;
  axisCfg.name = "axis";
  axisCfg.xAxisIsTime = true;
  axisCfg.useUTC = true;
  axisCfg.enableGrid = true;
  axisCfg.enableAALines = true;
  axisCfg.enableSpine = true;
  axisCfg.yAxisClipX = 0.75f;
  axisCfg.xAxisClipY = -0.85f;

  dc::AxisRecipe axisRecipe(500, axisCfg);
  auto axisBuild = axisRecipe.build();
  for (auto& cmd : axisBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "axis create");

  // Set viewport: X range = timestamp range, Y = price range
  float xMin = startTs - interval;
  float xMax = startTs + static_cast<float>(candleCount) * interval + interval;
  float yMin = priceMin - 2.0f;
  float yMax = priceMax + 2.0f;

  dc::Viewport vp;
  vp.setDataRange(xMin, xMax, yMin, yMax);
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

  // Compute axis data (time-axis)
  auto axData = axisRecipe.computeAxisDataV2(atlas,
    yMin, yMax,
    xMin, xMax,
    -1.0f, 1.0f,
    -1.0f, axisCfg.yAxisClipX,
    48.0f, 0.04f);

  std::printf("  labelGlyphCount = %u\n", axData.labelGlyphCount);
  requireTrue(axData.labelGlyphCount > 0, "time-axis labels exist");

  // Upload axis data
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

  uploadBuf(axisRecipe.yTickBufferId(), axData.yTickVerts);
  setVertexCount(axisRecipe.yTickGeomId(), axData.yTickVertexCount);
  uploadBuf(axisRecipe.xTickBufferId(), axData.xTickVerts);
  setVertexCount(axisRecipe.xTickGeomId(), axData.xTickVertexCount);
  uploadBuf(axisRecipe.labelBufferId(), axData.labelInstances);
  setVertexCount(axisRecipe.labelGeomId(), axData.labelGlyphCount);
  uploadBuf(axisRecipe.hGridBufferId(), axData.hGridVerts);
  setVertexCount(axisRecipe.hGridGeomId(), axData.hGridLineCount);
  uploadBuf(axisRecipe.vGridBufferId(), axData.vGridVerts);
  setVertexCount(axisRecipe.vGridGeomId(), axData.vGridLineCount);
  uploadBuf(axisRecipe.yTickAABufferId(), axData.yTickAAVerts);
  setVertexCount(axisRecipe.yTickAAGeomId(), axData.yTickAAVertexCount);
  uploadBuf(axisRecipe.xTickAABufferId(), axData.xTickAAVerts);
  setVertexCount(axisRecipe.xTickAAGeomId(), axData.xTickAAVertexCount);
  uploadBuf(axisRecipe.spineBufferId(), axData.spineVerts);
  setVertexCount(axisRecipe.spineGeomId(), axData.spineLineCount);

  // --- Render ---
  dc::GpuBufferManager gpuBufs;

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

  // Verify: draw calls >= 7
  requireTrue(stats.drawCalls >= 7, "at least 7 draw calls");

  auto pixels = ctx.readPixels();
  int nonBlack = 0;
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 10 || pixels[idx + 1] > 10 || pixels[idx + 2] > 10)
      nonBlack++;
  }
  requireTrue(nonBlack > 100, "significant non-black content");
  std::printf("  non-black pixels: %d\n", nonBlack);

  std::printf("D13.5 time_gl: ALL PASS\n");
  return 0;
#endif
}
