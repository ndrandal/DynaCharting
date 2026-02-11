// D4.6 — Full demo integration test
// GL test: build multi-pane chart, render, verify non-black output.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"

#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/TextRecipe.hpp"
#include "dc/recipe/AreaRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/recipe/MacdRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/layout/PaneLayout.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <vector>

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
  constexpr int W = 400, H = 300;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Create panes and layers
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane1");
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"MACD"})"), "pane2");

  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Fill"})"), "layer10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Grid"})"), "layer11");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"Volume"})"), "layer12");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":13,"paneId":1,"name":"Candles"})"), "layer13");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":14,"paneId":1,"name":"Lines"})"), "layer14");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":2,"name":"MacdHist"})"), "layer20");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":2,"name":"MacdLines"})"), "layer21");

  // Build recipes
  dc::LineRecipeConfig gridCfg;
  gridCfg.layerId = 11; gridCfg.name = "Grid"; gridCfg.createTransform = false;
  dc::LineRecipe gridRecipe(100, gridCfg);

  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 13; candleCfg.name = "OHLC"; candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(300, candleCfg);

  dc::SmaRecipeConfig smaCfg;
  smaCfg.layerId = 14; smaCfg.name = "SMA20"; smaCfg.createTransform = false; smaCfg.period = 10;
  dc::SmaRecipe smaRecipe(400, smaCfg);

  dc::BollingerRecipeConfig bbCfg;
  bbCfg.lineLayerId = 14; bbCfg.fillLayerId = 10; bbCfg.name = "BB";
  bbCfg.createTransform = false; bbCfg.period = 10; bbCfg.numStdDev = 2.0f;
  dc::BollingerRecipe bbRecipe(500, bbCfg);

  dc::AreaRecipeConfig areaCfg;
  areaCfg.layerId = 12; areaCfg.name = "Volume"; areaCfg.createTransform = false;
  dc::AreaRecipe areaRecipe(600, areaCfg);

  dc::MacdRecipeConfig macdCfg;
  macdCfg.lineLayerId = 21; macdCfg.histLayerId = 20; macdCfg.name = "MACD";
  macdCfg.createTransform = false; macdCfg.fastPeriod = 5; macdCfg.slowPeriod = 10; macdCfg.signalPeriod = 3;
  dc::MacdRecipe macdRecipe(700, macdCfg);

  // Apply all recipes
  auto applyRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.createCommands) requireOk(cp.applyJsonText(cmd), name);
  };

  applyRecipe(gridRecipe.build(), "grid");
  applyRecipe(candleRecipe.build(), "candle");
  applyRecipe(smaRecipe.build(), "sma");
  applyRecipe(bbRecipe.build(), "bb");
  applyRecipe(areaRecipe.build(), "area");
  applyRecipe(macdRecipe.build(), "macd");

  // Generate 50 candles
  constexpr int N = 50;
  float closePrices[N], xPos[N];
  float price = 100.0f;
  std::uint32_t seed = 42;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  float barW = 1.8f / static_cast<float>(N);
  float hw = barW * 0.35f;
  float priceMin = 1e9f, priceMax = -1e9f;

  struct CandleData { float x, open, high, low, close, halfWidth; };
  std::vector<CandleData> candles(N);

  for (int i = 0; i < N; i++) {
    float x = -0.9f + barW * (static_cast<float>(i) + 0.5f);
    float change = (rng() - 0.5f) * 4.0f;
    float open = price;
    float close = price + change;
    float high = std::fmax(open, close) + rng() * 2.0f;
    float low  = std::fmin(open, close) - rng() * 2.0f;
    price = close;

    candles[static_cast<std::size_t>(i)] = {x, open, high, low, close, hw};
    closePrices[i] = close;
    xPos[i] = x;
    priceMin = std::fmin(priceMin, low);
    priceMax = std::fmax(priceMax, high);
  }

  // Normalize candles to price pane clip space [-0.35, 0.90]
  float pClipMin = -0.35f, pClipMax = 0.85f;
  std::vector<CandleData> normCandles(N);
  for (int i = 0; i < N; i++) {
    auto& c = candles[static_cast<std::size_t>(i)];
    auto& nc = normCandles[static_cast<std::size_t>(i)];
    nc.x = c.x;
    nc.open = dc::normalizeToClip(c.open, priceMin, priceMax, pClipMin, pClipMax);
    nc.high = dc::normalizeToClip(c.high, priceMin, priceMax, pClipMin, pClipMax);
    nc.low  = dc::normalizeToClip(c.low, priceMin, priceMax, pClipMin, pClipMax);
    nc.close = dc::normalizeToClip(c.close, priceMin, priceMax, pClipMin, pClipMax);
    nc.halfWidth = hw;
  }

  // Compute indicators
  auto smaData = smaRecipe.compute(closePrices, xPos, N, priceMin, priceMax, pClipMin, pClipMax);
  auto bbData = bbRecipe.compute(closePrices, xPos, N, priceMin, priceMax, pClipMin, pClipMax);

  float mClipMin = -1.0f, mClipMax = -0.40f;
  auto macdData = macdRecipe.compute(closePrices, xPos, N, hw * 0.8f, mClipMin, mClipMax);

  // Volume area
  std::vector<float> volX(N), volY(N);
  for (int i = 0; i < N; i++) {
    volX[static_cast<std::size_t>(i)] = xPos[i];
    volY[static_cast<std::size_t>(i)] = pClipMin + rng() * 0.15f;
  }
  auto areaData = areaRecipe.compute(volX.data(), volY.data(), N, pClipMin);

  // Grid lines
  std::vector<float> gridVerts;
  for (int i = -4; i <= 4; i++) {
    float y = pClipMin + (pClipMax - pClipMin) * (static_cast<float>(i + 4) / 8.0f);
    gridVerts.push_back(-0.9f); gridVerts.push_back(y);
    gridVerts.push_back(0.9f);  gridVerts.push_back(y);
  }

  // Binary ingest helper
  auto writeU32LE = [](std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  auto appendRec = [&writeU32LE](std::vector<std::uint8_t>& b, std::uint8_t op,
                     std::uint32_t bid, std::uint32_t off,
                     const void* payload, std::uint32_t len) {
    b.push_back(op);
    writeU32LE(b, bid); writeU32LE(b, off); writeU32LE(b, len);
    const auto* p = static_cast<const std::uint8_t*>(payload);
    b.insert(b.end(), p, p + len);
  };

  std::vector<std::uint8_t> batch;
  auto ingestFloat = [&](std::uint32_t bid, const std::vector<float>& v) {
    if (!v.empty())
      appendRec(batch, 1, bid, 0, v.data(), static_cast<std::uint32_t>(v.size() * sizeof(float)));
  };

  ingestFloat(100, gridVerts);
  appendRec(batch, 1, 300, 0, normCandles.data(),
            static_cast<std::uint32_t>(normCandles.size() * sizeof(CandleData)));
  ingestFloat(400, smaData.lineVerts);
  ingestFloat(500, bbData.middleVerts);
  ingestFloat(503, bbData.upperVerts);
  ingestFloat(506, bbData.lowerVerts);
  ingestFloat(509, bbData.fillVerts);
  ingestFloat(600, areaData.triVerts);
  ingestFloat(700, macdData.macdLineVerts);
  ingestFloat(703, macdData.signalLineVerts);
  ingestFloat(706, macdData.posHistRects);
  ingestFloat(709, macdData.negHistRects);

  auto ir = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  std::printf("Ingested %u bytes, %zu buffers\n", ir.payloadBytes, ir.touchedBufferIds.size());

  // Set vertex counts
  auto setVC = [&](dc::Id geomId, std::uint32_t vc) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryVertexCount","geometryId":)" + std::to_string(geomId) +
      R"(,"vertexCount":)" + std::to_string(vc) + "}"), "setVC");
  };

  setVC(101, static_cast<std::uint32_t>(gridVerts.size() / 2));
  setVC(301, static_cast<std::uint32_t>(N));
  if (smaData.vertexCount > 0) setVC(401, smaData.vertexCount);
  if (bbData.middleVC > 0) setVC(501, bbData.middleVC);
  if (bbData.upperVC > 0) setVC(504, bbData.upperVC);
  if (bbData.lowerVC > 0) setVC(507, bbData.lowerVC);
  if (bbData.fillVC > 0) setVC(510, bbData.fillVC);
  if (areaData.vertexCount > 0) setVC(601, areaData.vertexCount);
  if (macdData.macdVC > 0) setVC(701, macdData.macdVC);
  if (macdData.signalVC > 0) setVC(704, macdData.signalVC);
  if (macdData.posHistCount > 0) setVC(707, macdData.posHistCount);
  if (macdData.negHistCount > 0) setVC(710, macdData.negHistCount);

  // Set colors
  auto setColor = [&](dc::Id diId, float r, float g, float b, float a) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":%llu,"r":%.2f,"g":%.2f,"b":%.2f,"a":%.2f})",
      static_cast<unsigned long long>(diId), r, g, b, a);
    requireOk(cp.applyJsonText(buf), "setColor");
  };

  setColor(102, 0.2f, 0.2f, 0.3f, 1.0f); // grid: dark gray
  setColor(402, 1.0f, 0.8f, 0.0f, 1.0f); // SMA: yellow
  setColor(502, 0.4f, 0.6f, 1.0f, 1.0f); // BB middle: blue
  setColor(505, 0.3f, 0.5f, 0.8f, 0.7f); // BB upper
  setColor(508, 0.3f, 0.5f, 0.8f, 0.7f); // BB lower
  setColor(511, 0.2f, 0.3f, 0.5f, 0.2f); // BB fill: translucent
  setColor(602, 0.2f, 0.5f, 0.8f, 0.3f); // Volume: translucent blue
  setColor(702, 0.0f, 0.9f, 0.9f, 1.0f); // MACD line: cyan
  setColor(705, 1.0f, 0.5f, 0.0f, 1.0f); // Signal: orange
  setColor(708, 0.0f, 0.8f, 0.0f, 0.8f); // Pos hist: green
  setColor(711, 0.8f, 0.0f, 0.0f, 0.8f); // Neg hist: red

  // Upload and render
  dc::GpuBufferManager gpuBufs;
  for (dc::Id id : ir.touchedBufferIds) {
    gpuBufs.setCpuData(id, ingest.getBufferData(id), ingest.getBufferSize(id));
  }

  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  std::printf("Rendered: %u draw calls\n", stats.drawCalls);
  requireTrue(stats.drawCalls >= 5, "at least 5 draw calls");

  // Verify non-black output
  auto pixels = ctx.readPixels();
  int nonBlackPixels = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx] > 5 || pixels[idx+1] > 5 || pixels[idx+2] > 5) {
        nonBlackPixels++;
      }
    }
  }
  std::printf("Non-black pixels: %d / %d\n", nonBlackPixels, W * H);
  requireTrue(nonBlackPixels > 500, "substantial rendering output");

  std::printf("\nD4.6 demo PASS\n");
  return 0;
}
