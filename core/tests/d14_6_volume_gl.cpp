// D14.6 — GL integration test: candles + volume pane + legend + time axis
// Two-pane layout: price (top) with candles + axis, volume (bottom) with bars.
// Legend overlay on price pane. DrawItem visibility toggle verified.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/VolumeRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/recipe/LegendRecipe.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/layout/LayoutManager.hpp"

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
  std::printf("D14.6 volume_gl: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  constexpr int W = 400, H = 400;

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

  // ---- Layout: 2 panes (70% price, 30% volume) ----
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "price pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2})"), "volume pane");

  dc::LayoutManager layout;
  layout.addPane(1, 0.7f);
  layout.addPane(2, 0.3f);
  layout.applyLayout(cp);

  // Layers: grid(5), ticks(10), data(15), labels(20), overlay(25)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":5,"paneId":1})"), "gridLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "tickLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1})"), "dataLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":1})"), "labelLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":25,"paneId":1})"), "legendLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":30,"paneId":2})"), "volDataLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":35,"paneId":2})"), "volLabelLayer");

  // Shared data transforms
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "priceXform");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":51})"), "volXform");

  // ---- Generate candle + volume data ----
  constexpr int CANDLE_COUNT = 20;
  float startTs = 1700000000.0f;
  float interval = 300.0f;

  float candles[CANDLE_COUNT * 6];
  float volumes[CANDLE_COUNT];
  float price = 100.0f;
  float priceMin = 1e9f, priceMax = -1e9f;
  float volMax = 0.0f;
  std::uint32_t seed = 42;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  for (int i = 0; i < CANDLE_COUNT; i++) {
    float x = startTs + static_cast<float>(i) * interval;
    float open = price;
    float high = price + rng() * 2.0f;
    float low = price - rng() * 2.0f;
    price += (rng() - 0.5f) * 3.0f;
    float close = price;
    float hw = interval * 0.35f;

    candles[i * 6 + 0] = x;
    candles[i * 6 + 1] = open;
    candles[i * 6 + 2] = high;
    candles[i * 6 + 3] = low;
    candles[i * 6 + 4] = close;
    candles[i * 6 + 5] = hw;

    if (low < priceMin) priceMin = low;
    if (high > priceMax) priceMax = high;

    volumes[i] = 500.0f + rng() * 2000.0f;
    if (volumes[i] > volMax) volMax = volumes[i];
  }

  // ---- CandleRecipe ----
  dc::CandleRecipeConfig candleCfg;
  candleCfg.paneId = 1;
  candleCfg.layerId = 15;
  candleCfg.name = "BTCUSD";
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
      R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%d})",
      static_cast<unsigned>(candleRecipe.geometryId()), CANDLE_COUNT);
    requireOk(cp.applyJsonText(buf), "candle vc");
  }

  // ---- VolumeRecipe ----
  dc::VolumeRecipeConfig volCfg;
  volCfg.paneId = 2;
  volCfg.layerId = 30;
  volCfg.name = "Volume";
  volCfg.createTransform = false;

  dc::VolumeRecipe volRecipe(200, volCfg);
  auto volBuild = volRecipe.build();
  for (auto& cmd : volBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "volume create");

  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(volRecipe.drawItemId()) +
    R"(,"transformId":51})"), "attach vol xform");

  // Compute volume bars from candle data
  auto volData = volRecipe.computeVolumeBars(candles, volumes, CANDLE_COUNT, interval * 0.35f);

  ingest.ensureBuffer(volRecipe.bufferId());
  ingest.setBufferData(volRecipe.bufferId(),
                        reinterpret_cast<const std::uint8_t*>(volData.candle6.data()),
                        static_cast<std::uint32_t>(volData.candle6.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
      static_cast<unsigned>(volRecipe.geometryId()), volData.barCount);
    requireOk(cp.applyJsonText(buf), "vol vc");
  }

  // ---- Axis (time axis on price pane) ----
  dc::AxisRecipeConfig axisCfg;
  axisCfg.paneId = 1;
  axisCfg.tickLayerId = 10;
  axisCfg.labelLayerId = 20;
  axisCfg.gridLayerId = 5;
  axisCfg.name = "axis";
  axisCfg.xAxisIsTime = true;
  axisCfg.useUTC = true;
  axisCfg.enableGrid = true;
  axisCfg.enableSpine = true;
  axisCfg.yAxisClipX = 0.75f;
  axisCfg.xAxisClipY = -0.85f;

  dc::AxisRecipe axisRecipe(500, axisCfg);
  auto axisBuild = axisRecipe.build();
  for (auto& cmd : axisBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "axis create");

  // ---- LegendRecipe ----
  dc::LegendRecipeConfig legendCfg;
  legendCfg.paneId = 1;
  legendCfg.layerId = 25;
  legendCfg.name = "legend";

  dc::LegendRecipe legendRecipe(700, legendCfg);
  auto legendBuild = legendRecipe.build();
  for (auto& cmd : legendBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "legend create");

  // Compute legend from all series
  std::vector<dc::SeriesInfo> allSeries;
  for (auto& s : candleRecipe.seriesInfoList()) allSeries.push_back(s);
  for (auto& s : volRecipe.seriesInfoList()) allSeries.push_back(s);

  auto legendData = legendRecipe.computeLegend(allSeries, atlas);
  requireTrue(legendData.swatchCount == 2, "2 legend swatches");
  requireTrue(legendData.glyphCount > 0, "legend has text");

  // Upload legend data
  ingest.ensureBuffer(legendRecipe.swatchBufferId());
  ingest.setBufferData(legendRecipe.swatchBufferId(),
                        reinterpret_cast<const std::uint8_t*>(legendData.swatchRects.data()),
                        static_cast<std::uint32_t>(legendData.swatchRects.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
      static_cast<unsigned>(legendRecipe.swatchGeometryId()), legendData.swatchCount);
    requireOk(cp.applyJsonText(buf), "swatch vc");
  }

  ingest.ensureBuffer(legendRecipe.textBufferId());
  ingest.setBufferData(legendRecipe.textBufferId(),
                        reinterpret_cast<const std::uint8_t*>(legendData.textGlyphs.data()),
                        static_cast<std::uint32_t>(legendData.textGlyphs.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
      static_cast<unsigned>(legendRecipe.textGeometryId()), legendData.glyphCount);
    requireOk(cp.applyJsonText(buf), "text vc");
  }

  // ---- Set viewports ----
  float xMin = startTs - interval;
  float xMax = startTs + static_cast<float>(CANDLE_COUNT) * interval + interval;
  float yMin = priceMin - 2.0f;
  float yMax = priceMax + 2.0f;

  dc::Viewport priceVp;
  priceVp.setDataRange(xMin, xMax, yMin, yMax);
  priceVp.setPixelViewport(W, H);
  auto ptp = priceVp.computeTransformParams();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(ptp.sx), static_cast<double>(ptp.sy),
      static_cast<double>(ptp.tx), static_cast<double>(ptp.ty));
    requireOk(cp.applyJsonText(buf), "priceXform");
  }

  dc::Viewport volVp;
  volVp.setDataRange(xMin, xMax, 0.0f, volMax * 1.1f);
  volVp.setPixelViewport(W, H);
  auto vtp = volVp.computeTransformParams();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":51,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(vtp.sx), static_cast<double>(vtp.sy),
      static_cast<double>(vtp.tx), static_cast<double>(vtp.ty));
    requireOk(cp.applyJsonText(buf), "volXform");
  }

  // Compute axis data (time axis)
  const auto* pricePane = scene.getPane(1);
  auto axData = axisRecipe.computeAxisDataV2(atlas,
    yMin, yMax, xMin, xMax,
    pricePane->region.clipYMin, pricePane->region.clipYMax,
    -1.0f, axisCfg.yAxisClipX,
    48.0f, 0.04f);

  // Upload axis data
  auto uploadBuf = [&](dc::Id bufId, const std::vector<float>& data) {
    ingest.ensureBuffer(bufId);
    ingest.setBufferData(bufId, reinterpret_cast<const std::uint8_t*>(data.data()),
                          static_cast<std::uint32_t>(data.size() * sizeof(float)));
  };
  auto setVC = [&](dc::Id geomId, std::uint32_t count) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%u,"vertexCount":%u})",
      static_cast<unsigned>(geomId), count);
    requireOk(cp.applyJsonText(buf), "setVC");
  };

  uploadBuf(axisRecipe.yTickBufferId(), axData.yTickVerts);
  setVC(axisRecipe.yTickGeomId(), axData.yTickVertexCount);
  uploadBuf(axisRecipe.xTickBufferId(), axData.xTickVerts);
  setVC(axisRecipe.xTickGeomId(), axData.xTickVertexCount);
  uploadBuf(axisRecipe.labelBufferId(), axData.labelInstances);
  setVC(axisRecipe.labelGeomId(), axData.labelGlyphCount);
  uploadBuf(axisRecipe.hGridBufferId(), axData.hGridVerts);
  setVC(axisRecipe.hGridGeomId(), axData.hGridLineCount);
  uploadBuf(axisRecipe.vGridBufferId(), axData.vGridVerts);
  setVC(axisRecipe.vGridGeomId(), axData.vGridLineCount);
  uploadBuf(axisRecipe.spineBufferId(), axData.spineVerts);
  setVC(axisRecipe.spineGeomId(), axData.spineLineCount);

  // ---- Render ----
  dc::GpuBufferManager gpuBufs;

  auto syncBuf = [&](dc::Id bufId) {
    const auto* data = ingest.getBufferData(bufId);
    auto size = ingest.getBufferSize(bufId);
    if (data && size > 0) gpuBufs.setCpuData(bufId, data, size);
  };

  // Sync all buffers
  syncBuf(candleRecipe.bufferId());
  syncBuf(volRecipe.bufferId());
  syncBuf(legendRecipe.swatchBufferId());
  syncBuf(legendRecipe.textBufferId());
  syncBuf(axisRecipe.yTickBufferId());
  syncBuf(axisRecipe.xTickBufferId());
  syncBuf(axisRecipe.labelBufferId());
  syncBuf(axisRecipe.hGridBufferId());
  syncBuf(axisRecipe.vGridBufferId());
  syncBuf(axisRecipe.spineBufferId());

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  renderer.setGlyphAtlas(&atlas);
  gpuBufs.uploadDirty();

  auto stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  std::printf("  draw calls: %u, culled: %u\n", stats.drawCalls, stats.culledDrawCalls);
  requireTrue(stats.drawCalls >= 5, "at least 5 draw calls");

  auto pixels = ctx.readPixels();
  int nonBlack = 0;
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 10 || pixels[idx + 1] > 10 || pixels[idx + 2] > 10)
      nonBlack++;
  }
  requireTrue(nonBlack > 200, "significant non-black content");
  std::printf("  non-black pixels: %d\n", nonBlack);

  // ---- Test visibility toggle ----
  std::printf("  Testing visibility toggle...\n");

  // Hide candles
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemVisible","drawItemId":)" +
    std::to_string(candleRecipe.drawItemId()) + R"(,"visible":false})"), "hide candles");

  auto stats2 = renderer.render(scene, gpuBufs, W, H);
  std::printf("  after hiding candles: %u draw calls\n", stats2.drawCalls);
  requireTrue(stats2.drawCalls < stats.drawCalls, "fewer draw calls after hide");

  // Re-show candles
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemVisible","drawItemId":)" +
    std::to_string(candleRecipe.drawItemId()) + R"(,"visible":true})"), "show candles");

  auto stats3 = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats3.drawCalls == stats.drawCalls, "same draw calls after re-show");

  // ---- Test legend hit test ----
  int hitIdx = legendRecipe.hitTest(legendData,
    legendData.entries[0].swatchRect[0] + 0.01f,
    (legendData.entries[0].swatchRect[1] + legendData.entries[0].swatchRect[3]) / 2.0f);
  requireTrue(hitIdx == 0, "legend hit test finds first entry");
  std::printf("  legend hit test: entry %d\n", hitIdx);

  std::printf("D14.6 volume_gl: ALL PASS\n");
  return 0;
#endif
}
