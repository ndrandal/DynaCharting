// D5.7 — Interactive Multi-Pane Chart Demo
// GLFW: continuous render loop with pan/zoom/crosshair
// OSMesa fallback: single frame with simulated viewport offset, writes PPM

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GlContext.hpp"

#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/AreaRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/recipe/MacdRecipe.hpp"
#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/recipe/LevelLineRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/layout/PaneLayout.hpp"
#include "dc/text/TextLayout.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/InputMapper.hpp"

#ifdef DC_HAS_GLFW
#include "dc/gl/GlfwContext.hpp"
#endif
#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>
#include <string>

// ---- Helpers ----

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload, std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void setColor(dc::CommandProcessor& cp, dc::Id diId,
                      float r, float g, float b, float a) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setDrawItemColor","drawItemId":%llu,"r":%.3f,"g":%.3f,"b":%.3f,"a":%.3f})",
    static_cast<unsigned long long>(diId),
    static_cast<double>(r), static_cast<double>(g),
    static_cast<double>(b), static_cast<double>(a));
  requireOk(cp.applyJsonText(buf), "setColor");
}

static void setVC(dc::CommandProcessor& cp, dc::Id geomId, std::uint32_t vc) {
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":)" + std::to_string(geomId) +
    R"(,"vertexCount":)" + std::to_string(vc) + "}"), "setVC");
}

static void ingestFloat(std::vector<std::uint8_t>& batch, std::uint32_t bid,
                          const std::vector<float>& v) {
  if (!v.empty())
    appendRecord(batch, 1, bid, 0, v.data(),
                 static_cast<std::uint32_t>(v.size() * sizeof(float)));
}

static void setTransformCmd(dc::CommandProcessor& cp, dc::Id xfId, const dc::TransformParams& tp) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%llu,"tx":%.6f,"ty":%.6f,"sx":%.6f,"sy":%.6f})",
    static_cast<unsigned long long>(xfId),
    static_cast<double>(tp.tx), static_cast<double>(tp.ty),
    static_cast<double>(tp.sx), static_cast<double>(tp.sy));
  requireOk(cp.applyJsonText(buf), "setTransform");
}

static void writePPM(const char* filename, const std::vector<std::uint8_t>& pixels,
                      int w, int h) {
  FILE* f = std::fopen(filename, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
      std::fputc(pixels[idx + 0], f);
      std::fputc(pixels[idx + 1], f);
      std::fputc(pixels[idx + 2], f);
    }
  }
  std::fclose(f);
  std::printf("Wrote %s (%dx%d)\n", filename, w, h);
}

struct Candle {
  float x, open, high, low, close, halfWidth;
};

int main() {
  constexpr int W = 1024, H = 768;
  constexpr int NUM_CANDLES = 100;

  // 1. Create GL context
  std::unique_ptr<dc::GlContext> glCtx;
  bool isGlfw = false;
#ifdef DC_HAS_GLFW
  { auto g = std::make_unique<dc::GlfwContext>();
    if (g->init(W, H)) { glCtx = std::move(g); isGlfw = true; } }
#endif
#ifdef DC_HAS_OSMESA
  if (!glCtx) {
    auto m = std::make_unique<dc::OsMesaContext>();
    if (!m->init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }
    glCtx = std::move(m);
    std::printf("Using OSMesa (headless) — single frame\n");
  }
#endif
  if (!glCtx) { std::fprintf(stderr, "No GL context\n"); return 1; }

  // 2. Load font
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(1024);
  atlas.setGlyphPx(48);
  bool hasFont = false;
#ifdef FONT_PATH
  if (atlas.loadFontFile(FONT_PATH)) {
    atlas.ensureAscii();
    hasFont = true;
    std::printf("Font loaded\n");
  }
#endif

  // 3. Scene infrastructure
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  cp.setGlyphAtlas(&atlas);

  // Panes
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane1");
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"MACD"})"), "pane2");

  // Layers
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"BollFill"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Grid"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"Volume"})"), "l12");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":13,"paneId":1,"name":"Candles"})"), "l13");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":14,"paneId":1,"name":"Lines"})"), "l14");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1,"name":"TickLines"})"), "l15");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":16,"paneId":1,"name":"Labels"})"), "l16");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":17,"paneId":1,"name":"CrossLines"})"), "l17");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":18,"paneId":1,"name":"CrossLabels"})"), "l18");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":2,"name":"MacdHist"})"), "l20");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":2,"name":"MacdLines"})"), "l21");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":22,"paneId":2,"name":"MacdLabels"})"), "l22");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":23,"paneId":2,"name":"MacdCrossLines"})"), "l23");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":24,"paneId":2,"name":"MacdCrossLabels"})"), "l24");

  // Shared transforms
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "priceXform");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":51})"), "macdXform");

  // 4. Build recipes
  dc::LineRecipeConfig gridCfg;
  gridCfg.layerId = 11; gridCfg.name = "Grid"; gridCfg.createTransform = false;
  dc::LineRecipe gridRecipe(100, gridCfg);

  dc::AxisRecipeConfig axisCfg;
  axisCfg.paneId = 1; axisCfg.tickLayerId = 15; axisCfg.labelLayerId = 16;
  axisCfg.name = "PriceAxis"; axisCfg.yAxisClipX = 0.82f; axisCfg.xAxisClipY = -0.33f;
  dc::AxisRecipe axisRecipe(200, axisCfg);

  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 13; candleCfg.name = "OHLC"; candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(300, candleCfg);

  dc::SmaRecipeConfig smaCfg;
  smaCfg.layerId = 14; smaCfg.name = "SMA20"; smaCfg.createTransform = false; smaCfg.period = 20;
  dc::SmaRecipe smaRecipe(400, smaCfg);

  dc::BollingerRecipeConfig bbCfg;
  bbCfg.lineLayerId = 14; bbCfg.fillLayerId = 10; bbCfg.name = "BB";
  bbCfg.createTransform = false; bbCfg.period = 20; bbCfg.numStdDev = 2.0f;
  dc::BollingerRecipe bbRecipe(500, bbCfg);

  dc::AreaRecipeConfig areaCfg;
  areaCfg.layerId = 12; areaCfg.name = "Volume"; areaCfg.createTransform = false;
  dc::AreaRecipe areaRecipe(600, areaCfg);

  dc::MacdRecipeConfig macdCfg;
  macdCfg.lineLayerId = 21; macdCfg.histLayerId = 20; macdCfg.name = "MACD";
  macdCfg.createTransform = false;
  dc::MacdRecipe macdRecipe(700, macdCfg);

  dc::CrosshairRecipeConfig crossPriceCfg;
  crossPriceCfg.paneId = 1; crossPriceCfg.lineLayerId = 17;
  crossPriceCfg.labelLayerId = 18; crossPriceCfg.name = "CrossPrice";
  dc::CrosshairRecipe crossPriceRecipe(1000, crossPriceCfg);

  dc::CrosshairRecipeConfig crossMacdCfg;
  crossMacdCfg.paneId = 2; crossMacdCfg.lineLayerId = 23;
  crossMacdCfg.labelLayerId = 24; crossMacdCfg.name = "CrossMacd";
  dc::CrosshairRecipe crossMacdRecipe(1020, crossMacdCfg);

  dc::LevelLineRecipeConfig levelCfg;
  levelCfg.paneId = 1; levelCfg.lineLayerId = 17;
  levelCfg.labelLayerId = 18; levelCfg.name = "CurrentPrice";
  dc::LevelLineRecipe levelRecipe(1040, levelCfg);

  // Apply all recipes
  auto applyRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.createCommands) requireOk(cp.applyJsonText(cmd), name);
  };

  applyRecipe(gridRecipe.build(), "grid");
  applyRecipe(axisRecipe.build(), "axis");
  applyRecipe(candleRecipe.build(), "candle");
  applyRecipe(smaRecipe.build(), "sma");
  applyRecipe(bbRecipe.build(), "bb");
  applyRecipe(areaRecipe.build(), "area");
  applyRecipe(macdRecipe.build(), "macd");
  applyRecipe(crossPriceRecipe.build(), "crossPrice");
  applyRecipe(crossMacdRecipe.build(), "crossMacd");
  applyRecipe(levelRecipe.build(), "level");

  // Attach shared transforms to data-dependent items
  auto attachXform = [&](dc::Id diId, dc::Id xfId) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(diId) +
      R"(,"transformId":)" + std::to_string(xfId) + "}"), "attachXform");
  };
  attachXform(102, 50); // grid
  attachXform(302, 50); // candles

  // 5. Generate data
  std::vector<Candle> candles;
  float closePrices[NUM_CANDLES], xPos[NUM_CANDLES];
  float price = 100.0f;
  std::uint32_t seed = 42;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  float barW = 1.6f / static_cast<float>(NUM_CANDLES);
  float hw = barW * 0.35f;
  float priceMin = 1e9f, priceMax = -1e9f;

  for (int i = 0; i < NUM_CANDLES; i++) {
    float x = -0.85f + barW * (static_cast<float>(i) + 0.5f);
    float change = (rng() - 0.5f) * 4.0f;
    float open = price;
    float close = price + change;
    float high = std::fmax(open, close) + rng() * 2.0f;
    float low  = std::fmin(open, close) - rng() * 2.0f;
    price = close;

    Candle c;
    c.x = x; c.open = open; c.high = high; c.low = low; c.close = close; c.halfWidth = hw;
    candles.push_back(c);
    closePrices[i] = close;
    xPos[i] = x;
    priceMin = std::fmin(priceMin, low);
    priceMax = std::fmax(priceMax, high);
  }

  // Pane layout
  auto panes = dc::computePaneLayout({0.7f, 0.3f}, 0.05f, 0.05f);
  dc::PaneRegion pricePaneRegion = panes[0];
  dc::PaneRegion macdPaneRegion = panes[1];

  // Setup viewports
  dc::Viewport priceVP, macdVP;
  priceVP.setPixelViewport(W, H);
  priceVP.setClipRegion(pricePaneRegion);
  priceVP.setDataRange(
    static_cast<double>(xPos[0] - hw), static_cast<double>(xPos[NUM_CANDLES-1] + hw),
    static_cast<double>(priceMin), static_cast<double>(priceMax));

  macdVP.setPixelViewport(W, H);
  macdVP.setClipRegion(macdPaneRegion);
  // MACD range set after computation

  // InputMapper
  dc::InputMapper inputMapper;
  dc::InputMapperConfig imCfg;
  imCfg.linkXAxis = true;
  imCfg.zoomSensitivity = 0.15;
  inputMapper.setConfig(imCfg);
  inputMapper.setViewports({&priceVP, &macdVP});

  // Normalize candles
  auto normalizeCandles = [&](float pClipMin, float pClipMax) {
    std::vector<Candle> normCandles(NUM_CANDLES);
    for (int i = 0; i < NUM_CANDLES; i++) {
      auto& c = candles[static_cast<std::size_t>(i)];
      auto& nc = normCandles[static_cast<std::size_t>(i)];
      nc.x = c.x;
      nc.open = dc::normalizeToClip(c.open, priceMin, priceMax, pClipMin, pClipMax);
      nc.high = dc::normalizeToClip(c.high, priceMin, priceMax, pClipMin, pClipMax);
      nc.low  = dc::normalizeToClip(c.low, priceMin, priceMax, pClipMin, pClipMax);
      nc.close = dc::normalizeToClip(c.close, priceMin, priceMax, pClipMin, pClipMax);
      nc.halfWidth = hw;
    }
    return normCandles;
  };

  float pClipMin = pricePaneRegion.clipYMin;
  float pClipMax = pricePaneRegion.clipYMax;
  auto normCandles = normalizeCandles(pClipMin, pClipMax);

  // Compute indicators
  auto smaData = smaRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                     priceMin, priceMax, pClipMin, pClipMax);
  auto bbData = bbRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                   priceMin, priceMax, pClipMin, pClipMax);

  float mClipMin = macdPaneRegion.clipYMin;
  float mClipMax = macdPaneRegion.clipYMax;
  auto macdData = macdRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                       hw * 0.8f, mClipMin, mClipMax);

  // Set MACD viewport data range (approximate from macdData range)
  macdVP.setDataRange(
    static_cast<double>(xPos[0] - hw), static_cast<double>(xPos[NUM_CANDLES-1] + hw),
    -5.0, 5.0);

  // Volume area
  std::vector<float> volX(NUM_CANDLES), volY(NUM_CANDLES);
  for (int i = 0; i < NUM_CANDLES; i++) {
    volX[static_cast<std::size_t>(i)] = xPos[i];
    volY[static_cast<std::size_t>(i)] = pClipMin + rng() * 0.2f;
  }
  auto areaData = areaRecipe.compute(volX.data(), volY.data(), NUM_CANDLES, pClipMin);

  // Grid lines
  std::vector<float> gridVerts;
  for (int i = -4; i <= 4; i++) {
    float y = pClipMin + (pClipMax - pClipMin) * (static_cast<float>(i + 4) / 8.0f);
    gridVerts.push_back(-0.9f); gridVerts.push_back(y);
    gridVerts.push_back(0.85f); gridVerts.push_back(y);
  }

  // 6. Binary ingest (initial)
  std::vector<std::uint8_t> batch;
  ingestFloat(batch, 100, gridVerts);
  appendRecord(batch, 1, 300, 0, normCandles.data(),
               static_cast<std::uint32_t>(normCandles.size() * sizeof(Candle)));
  ingestFloat(batch, 400, smaData.lineVerts);
  ingestFloat(batch, 500, bbData.middleVerts);
  ingestFloat(batch, 503, bbData.upperVerts);
  ingestFloat(batch, 506, bbData.lowerVerts);
  ingestFloat(batch, 509, bbData.fillVerts);
  ingestFloat(batch, 600, areaData.triVerts);
  ingestFloat(batch, 700, macdData.macdLineVerts);
  ingestFloat(batch, 703, macdData.signalLineVerts);
  ingestFloat(batch, 706, macdData.posHistRects);
  ingestFloat(batch, 709, macdData.negHistRects);

  auto ir = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  // Set vertex counts
  setVC(cp, 101, static_cast<std::uint32_t>(gridVerts.size() / 2));
  setVC(cp, 301, static_cast<std::uint32_t>(NUM_CANDLES));
  if (smaData.vertexCount > 0) setVC(cp, 401, smaData.vertexCount);
  if (bbData.middleVC > 0) setVC(cp, 501, bbData.middleVC);
  if (bbData.upperVC > 0) setVC(cp, 504, bbData.upperVC);
  if (bbData.lowerVC > 0) setVC(cp, 507, bbData.lowerVC);
  if (bbData.fillVC > 0) setVC(cp, 510, bbData.fillVC);
  if (areaData.vertexCount > 0) setVC(cp, 601, areaData.vertexCount);
  if (macdData.macdVC > 0) setVC(cp, 701, macdData.macdVC);
  if (macdData.signalVC > 0) setVC(cp, 704, macdData.signalVC);
  if (macdData.posHistCount > 0) setVC(cp, 707, macdData.posHistCount);
  if (macdData.negHistCount > 0) setVC(cp, 710, macdData.negHistCount);

  // Set colors
  setColor(cp, 102, 0.15f, 0.15f, 0.25f, 1.0f);
  setColor(cp, 402, 1.0f, 0.8f, 0.0f, 1.0f);
  setColor(cp, 502, 0.4f, 0.6f, 1.0f, 1.0f);
  setColor(cp, 505, 0.3f, 0.5f, 0.8f, 0.6f);
  setColor(cp, 508, 0.3f, 0.5f, 0.8f, 0.6f);
  setColor(cp, 511, 0.15f, 0.25f, 0.45f, 0.15f);
  setColor(cp, 602, 0.2f, 0.5f, 0.8f, 0.25f);
  setColor(cp, 702, 0.0f, 0.9f, 0.9f, 1.0f);
  setColor(cp, 705, 1.0f, 0.5f, 0.0f, 1.0f);
  setColor(cp, 708, 0.0f, 0.8f, 0.0f, 0.7f);
  setColor(cp, 711, 0.8f, 0.0f, 0.0f, 0.7f);

  // Crosshair colors (white lines, light labels)
  setColor(cp, 1002, 0.8f, 0.8f, 0.8f, 0.5f); // price H-line
  setColor(cp, 1005, 0.8f, 0.8f, 0.8f, 0.5f); // price V-line
  setColor(cp, 1022, 0.8f, 0.8f, 0.8f, 0.5f); // macd H-line
  setColor(cp, 1025, 0.8f, 0.8f, 0.8f, 0.5f); // macd V-line
  setColor(cp, 1042, 1.0f, 0.4f, 0.0f, 0.8f); // level line: orange

  // Level line: current price
  float lastClose = closePrices[NUM_CANDLES - 1];
  if (hasFont) {
    char priceBuf[32];
    std::snprintf(priceBuf, sizeof(priceBuf), "%.2f", static_cast<double>(lastClose));
    auto levelData = levelRecipe.computeLevels(
        {{static_cast<double>(lastClose), priceBuf}},
        pricePaneRegion,
        static_cast<double>(priceMin), static_cast<double>(priceMax),
        atlas, 48.0f, 0.035f);

    if (!levelData.lineVerts.empty()) {
      std::vector<std::uint8_t> levelBatch;
      ingestFloat(levelBatch, 1040, levelData.lineVerts);
      ingestFloat(levelBatch, 1043, levelData.labelGlyphs);
      auto lr = ingest.processBatch(levelBatch.data(),
          static_cast<std::uint32_t>(levelBatch.size()));
      for (dc::Id id : lr.touchedBufferIds) {
        // Will be uploaded below
      }
      setVC(cp, 1041, levelData.lineVertexCount);
      setVC(cp, 1044, levelData.labelGlyphCount);
    }
  }

  // 7. GPU setup + renderer
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  if (!renderer.init()) { std::fprintf(stderr, "Renderer init failed\n"); return 1; }
  renderer.setGlyphAtlas(&atlas);

  // Upload all initial buffers
  auto uploadAll = [&]() {
    for (auto bid : scene.bufferIds()) {
      auto sz = ingest.getBufferSize(bid);
      if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
    }
    gpuBufs.uploadDirty();
  };

  // Apply initial transforms from viewports
  setTransformCmd(cp, 50, priceVP.computeTransformParams());
  setTransformCmd(cp, 51, macdVP.computeTransformParams());

  uploadAll();

  // For OSMesa: simulate a viewport offset then render a single frame
  if (!isGlfw) {
    // Simulate a slight pan for demo
    priceVP.pan(30.0, 0.0);
    macdVP.pan(30.0, 0.0);
    setTransformCmd(cp, 50, priceVP.computeTransformParams());
    setTransformCmd(cp, 51, macdVP.computeTransformParams());

    // Simulate crosshair at center of price pane
    if (hasFont) {
      double cx, cy;
      priceVP.pixelToClip(W/2, H/3, cx, cy);
      double dx, dy;
      priceVP.pixelToData(W/2, H/3, dx, dy);

      auto chData = crossPriceRecipe.computeCrosshairData(
          cx, cy, dx, dy, pricePaneRegion, atlas, 48.0f, 0.035f);
      if (chData.visible) {
        std::vector<std::uint8_t> chBatch;
        ingestFloat(chBatch, 1000, chData.hLineVerts);
        ingestFloat(chBatch, 1003, chData.vLineVerts);
        ingestFloat(chBatch, 1006, chData.priceLabelGlyphs);
        ingestFloat(chBatch, 1009, chData.timeLabelGlyphs);
        ingest.processBatch(chBatch.data(), static_cast<std::uint32_t>(chBatch.size()));
        setVC(cp, 1001, 2); // h-line: 2 verts
        setVC(cp, 1004, 2); // v-line: 2 verts
        setVC(cp, 1007, chData.priceLabelGC);
        setVC(cp, 1010, chData.timeLabelGC);
      }
    }

    uploadAll();
    dc::Stats stats = renderer.render(scene, gpuBufs, glCtx->width(), glCtx->height());
    std::printf("Rendered: %u draw calls\n", stats.drawCalls);

    auto pixels = glCtx->readPixels();
    writePPM("d5_7_interactive_chart.ppm", pixels, glCtx->width(), glCtx->height());
    std::printf("D5.7 interactive demo complete (OSMesa single frame)\n");
    return 0;
  }

#ifdef DC_HAS_GLFW
  // GLFW interactive loop
  auto* glfwCtx = static_cast<dc::GlfwContext*>(glCtx.get());
  std::printf("Interactive mode — drag to pan, scroll to zoom, Esc to quit\n");

  while (!glfwCtx->shouldClose()) {
    dc::InputState is = glfwCtx->pollInput();
    dc::ViewportInputState vis = is.toViewportInput();

    bool changed = inputMapper.processInput(vis);

    if (changed) {
      // Update transforms
      setTransformCmd(cp, 50, priceVP.computeTransformParams());
      setTransformCmd(cp, 51, macdVP.computeTransformParams());
    }

    // Update crosshair
    if (hasFont) {
      dc::Viewport* activeVP = inputMapper.activeViewport();
      dc::CrosshairRecipe* activeRecipe = nullptr;
      dc::PaneRegion activeRegion{};

      if (activeVP == &priceVP) {
        activeRecipe = &crossPriceRecipe;
        activeRegion = pricePaneRegion;
      } else if (activeVP == &macdVP) {
        activeRecipe = &crossMacdRecipe;
        activeRegion = macdPaneRegion;
      }

      // Compute crosshair for active pane
      for (auto* recipe : {&crossPriceRecipe, &crossMacdRecipe}) {
        dc::PaneRegion region = (recipe == &crossPriceRecipe) ? pricePaneRegion : macdPaneRegion;
        dc::Viewport* vp = (recipe == &crossPriceRecipe) ? &priceVP : &macdVP;
        dc::Id baseId = (recipe == &crossPriceRecipe) ? 1000 : 1020;

        double cx, cy;
        vp->pixelToClip(vis.cursorX, vis.cursorY, cx, cy);
        double dx, dy;
        vp->pixelToData(vis.cursorX, vis.cursorY, dx, dy);

        auto chData = recipe->computeCrosshairData(cx, cy, dx, dy, region, atlas, 48.0f, 0.035f);

        std::vector<std::uint8_t> chBatch;
        if (chData.visible) {
          ingestFloat(chBatch, static_cast<std::uint32_t>(baseId), chData.hLineVerts);
          ingestFloat(chBatch, static_cast<std::uint32_t>(baseId + 3), chData.vLineVerts);
          ingestFloat(chBatch, static_cast<std::uint32_t>(baseId + 6), chData.priceLabelGlyphs);
          ingestFloat(chBatch, static_cast<std::uint32_t>(baseId + 9), chData.timeLabelGlyphs);
          if (!chBatch.empty())
            ingest.processBatch(chBatch.data(), static_cast<std::uint32_t>(chBatch.size()));
          setVC(cp, static_cast<dc::Id>(baseId + 1), 2);
          setVC(cp, static_cast<dc::Id>(baseId + 4), 2);
          setVC(cp, static_cast<dc::Id>(baseId + 7), chData.priceLabelGC);
          setVC(cp, static_cast<dc::Id>(baseId + 10), chData.timeLabelGC);
        } else {
          setVC(cp, static_cast<dc::Id>(baseId + 1), 0);
          setVC(cp, static_cast<dc::Id>(baseId + 4), 0);
          setVC(cp, static_cast<dc::Id>(baseId + 7), 0);
          setVC(cp, static_cast<dc::Id>(baseId + 10), 0);
        }
      }
    }

    uploadAll();
    renderer.render(scene, gpuBufs, glfwCtx->width(), glfwCtx->height());
    glfwCtx->swapBuffers();
  }

  std::printf("D5.7 interactive demo complete\n");
#endif
  return 0;
}
