// D4.6 — Full-Featured Multi-Pane Chart Demo
// Builds a complete chart with: price pane (candles, SMA, Bollinger, volume, axes)
// and MACD pane (MACD line, signal, histogram, axis). Renders to PPM.

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
#include "dc/recipe/TextRecipe.hpp"
#include "dc/recipe/AreaRecipe.hpp"
#include "dc/recipe/SmaRecipe.hpp"
#include "dc/recipe/AxisRecipe.hpp"
#include "dc/recipe/BollingerRecipe.hpp"
#include "dc/recipe/MacdRecipe.hpp"
#include "dc/math/Normalize.hpp"
#include "dc/layout/PaneLayout.hpp"

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

// Build text glyph instances
static void buildTextInstances(const dc::GlyphAtlas& atlas, const char* text,
                                float startX, float baselineY, float fontSize,
                                float glyphPx,
                                std::vector<float>& out, int& glyphCount) {
  float cursorX = startX;
  float scale = fontSize / glyphPx;
  for (const char* p = text; *p; p++) {
    const dc::GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) { cursorX += g->advance * scale; continue; }
    float x0 = cursorX + g->bearingX * scale;
    float y1 = baselineY + g->bearingY * scale;
    float y0 = y1 - g->h * scale;
    float x1 = x0 + g->w * scale;
    out.push_back(x0); out.push_back(y0); out.push_back(x1); out.push_back(y1);
    out.push_back(g->u0); out.push_back(g->v0); out.push_back(g->u1); out.push_back(g->v1);
    glyphCount++;
    cursorX += g->advance * scale;
  }
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

// ---- Candle data ----

struct Candle {
  float x, open, high, low, close, halfWidth;
};

int main() {
  constexpr int W = 1024;
  constexpr int H = 768;
  constexpr int NUM_CANDLES = 100;

  // 1. Create GL context
  std::unique_ptr<dc::GlContext> glCtx;
#ifdef DC_HAS_GLFW
  { auto g = std::make_unique<dc::GlfwContext>(); if (g->init(W, H)) glCtx = std::move(g); }
#endif
#ifdef DC_HAS_OSMESA
  if (!glCtx) {
    auto m = std::make_unique<dc::OsMesaContext>();
    if (!m->init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }
    glCtx = std::move(m);
    std::printf("Using OSMesa (headless)\n");
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

  // Layers (ordered for correct draw order)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"BollFill"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Grid"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"Volume"})"), "l12");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":13,"paneId":1,"name":"Candles"})"), "l13");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":14,"paneId":1,"name":"Lines"})"), "l14");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":15,"paneId":1,"name":"TickLines"})"), "l15");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":16,"paneId":1,"name":"Labels"})"), "l16");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":2,"name":"MacdHist"})"), "l20");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":2,"name":"MacdLines"})"), "l21");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":22,"paneId":2,"name":"MacdLabels"})"), "l22");

  // Shared transforms
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "priceXform");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":51})"), "macdXform");

  // 4. Build all recipes
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

  dc::AxisRecipeConfig macdAxisCfg;
  macdAxisCfg.paneId = 2; macdAxisCfg.tickLayerId = 21; macdAxisCfg.labelLayerId = 22;
  macdAxisCfg.name = "MacdAxis"; macdAxisCfg.yAxisClipX = 0.82f; macdAxisCfg.xAxisClipY = -0.98f;
  dc::AxisRecipe macdAxisRecipe(800, macdAxisCfg);

  dc::TextRecipeConfig titleCfg;
  titleCfg.layerId = 16; titleCfg.name = "Title"; titleCfg.createTransform = false;
  dc::TextRecipe titleRecipe(900, titleCfg);

  dc::TextRecipeConfig legendCfg;
  legendCfg.layerId = 16; legendCfg.name = "Legend"; legendCfg.createTransform = false;
  dc::TextRecipe legendRecipe(910, legendCfg);

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
  applyRecipe(macdAxisRecipe.build(), "macdAxis");
  applyRecipe(titleRecipe.build(), "title");
  applyRecipe(legendRecipe.build(), "legend");

  // Attach shared transforms
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

  // Price pane clip region
  float pClipMin = -0.35f, pClipMax = 0.88f;

  // Normalize candles
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

  // Compute indicators
  auto smaData = smaRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                     priceMin, priceMax, pClipMin, pClipMax);
  auto bbData = bbRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                   priceMin, priceMax, pClipMin, pClipMax);

  float mClipMin = -1.0f, mClipMax = -0.42f;
  auto macdData = macdRecipe.compute(closePrices, xPos, NUM_CANDLES,
                                       hw * 0.8f, mClipMin, mClipMax);

  // Volume area (random volumes)
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

  // Title + legend text
  std::vector<float> titleInstances, legendInstances;
  int titleGC = 0, legendGC = 0;
  if (hasFont) {
    buildTextInstances(atlas, "DynaCharting D4.6 Demo",
                       -0.85f, 0.93f, 0.05f, 48.0f, titleInstances, titleGC);
    buildTextInstances(atlas, "SMA(20)  BB(20,2)  MACD(12,26,9)",
                       -0.85f, 0.87f, 0.03f, 48.0f, legendInstances, legendGC);
  }

  // 6. Binary ingest
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

  if (!titleInstances.empty())
    appendRecord(batch, 1, 900, 0, titleInstances.data(),
                 static_cast<std::uint32_t>(titleInstances.size() * sizeof(float)));
  if (!legendInstances.empty())
    appendRecord(batch, 1, 910, 0, legendInstances.data(),
                 static_cast<std::uint32_t>(legendInstances.size() * sizeof(float)));

  auto ir = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  std::printf("Ingested: %u payload bytes, %zu buffers\n",
              ir.payloadBytes, ir.touchedBufferIds.size());

  // 7. Set geometry vertex counts
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
  if (titleGC > 0) setVC(cp, 901, static_cast<std::uint32_t>(titleGC));
  if (legendGC > 0) setVC(cp, 911, static_cast<std::uint32_t>(legendGC));

  // 8. Set colors
  setColor(cp, 102, 0.15f, 0.15f, 0.25f, 1.0f); // grid
  setColor(cp, 402, 1.0f, 0.8f, 0.0f, 1.0f);    // SMA yellow
  setColor(cp, 502, 0.4f, 0.6f, 1.0f, 1.0f);    // BB middle blue
  setColor(cp, 505, 0.3f, 0.5f, 0.8f, 0.6f);    // BB upper
  setColor(cp, 508, 0.3f, 0.5f, 0.8f, 0.6f);    // BB lower
  setColor(cp, 511, 0.15f, 0.25f, 0.45f, 0.15f); // BB fill
  setColor(cp, 602, 0.2f, 0.5f, 0.8f, 0.25f);   // volume
  setColor(cp, 702, 0.0f, 0.9f, 0.9f, 1.0f);    // MACD cyan
  setColor(cp, 705, 1.0f, 0.5f, 0.0f, 1.0f);    // signal orange
  setColor(cp, 708, 0.0f, 0.8f, 0.0f, 0.7f);    // pos hist green
  setColor(cp, 711, 0.8f, 0.0f, 0.0f, 0.7f);    // neg hist red

  // 9. Upload + render
  dc::GpuBufferManager gpuBufs;
  for (dc::Id id : ir.touchedBufferIds) {
    gpuBufs.setCpuData(id, ingest.getBufferData(id), ingest.getBufferSize(id));
  }

  dc::Renderer renderer;
  if (!renderer.init()) { std::fprintf(stderr, "Renderer init failed\n"); return 1; }
  renderer.setGlyphAtlas(&atlas);
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, glCtx->width(), glCtx->height());
  glCtx->swapBuffers();
  std::printf("Rendered: %u draw calls\n", stats.drawCalls);

  auto pixels = glCtx->readPixels();
  writePPM("d4_6_full_chart.ppm", pixels, glCtx->width(), glCtx->height());

  // 10. Dispose all recipes
  auto disposeRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.disposeCommands) requireOk(cp.applyJsonText(cmd), name);
  };

  disposeRecipe(legendRecipe.build(), "legend");
  disposeRecipe(titleRecipe.build(), "title");
  disposeRecipe(macdAxisRecipe.build(), "macdAxis");
  disposeRecipe(macdRecipe.build(), "macd");
  disposeRecipe(areaRecipe.build(), "area");
  disposeRecipe(bbRecipe.build(), "bb");
  disposeRecipe(smaRecipe.build(), "sma");
  disposeRecipe(candleRecipe.build(), "candle");
  disposeRecipe(axisRecipe.build(), "axis");
  disposeRecipe(gridRecipe.build(), "grid");

  std::printf("All recipes disposed. Remaining drawItems: %zu\n",
              scene.drawItemIds().size());
  std::printf("D4.6 full demo complete\n");
  return 0;
}
