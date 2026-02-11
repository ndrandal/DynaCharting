// D3.5 — Recipe-Driven Chart Demo
// Builds a complete chart scene using recipes: grid lines, candles, volume
// bars, and text labels — all through the recipe system.
// Renders to PPM via OSMesa (or GLFW window if available).

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

// ---- Binary batch helpers ----

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

// ---- Fake OHLCV data ----

struct Candle {
  float x, open, high, low, close, halfWidth;
};

struct VolumeBar {
  float x0, y0, x1, y1;
};

static void generateCandles(int count, std::vector<Candle>& candles,
                             std::vector<VolumeBar>& volumes) {
  candles.clear();
  volumes.clear();
  float price = 100.0f;
  std::uint32_t seed = 42;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };
  float barW = 1.8f / static_cast<float>(count);
  float hw = barW * 0.35f;
  auto norm = [](float p) { return (p - 100.0f) / 25.0f; };

  for (int i = 0; i < count; i++) {
    float x = -0.9f + barW * (static_cast<float>(i) + 0.5f);
    float change = (rng() - 0.5f) * 4.0f;
    float open = price;
    float close = price + change;
    float high = std::fmax(open, close) + rng() * 2.0f;
    float low  = std::fmin(open, close) - rng() * 2.0f;
    price = close;

    Candle c;
    c.x = x; c.open = norm(open); c.high = norm(high);
    c.low = norm(low); c.close = norm(close); c.halfWidth = hw;
    candles.push_back(c);

    float vol = rng() * 0.3f;
    VolumeBar vb;
    vb.x0 = x - hw; vb.y0 = -0.95f; vb.x1 = x + hw; vb.y1 = -0.95f + vol;
    volumes.push_back(vb);
  }
}

static void generateGrid(std::vector<float>& gridVerts) {
  gridVerts.clear();
  for (int i = -4; i <= 4; i++) {
    float y = static_cast<float>(i) * 0.2f;
    gridVerts.push_back(-1.0f); gridVerts.push_back(y);
    gridVerts.push_back( 1.0f); gridVerts.push_back(y);
  }
}

// Build glyph instance data for a text string at given position
static void buildTextInstances(const dc::GlyphAtlas& atlas, const char* text,
                                float startX, float baselineY, float fontSize,
                                float glyphPx,
                                std::vector<float>& out, int& glyphCount) {
  float cursorX = startX;
  float scale = fontSize / glyphPx;

  for (const char* p = text; *p; p++) {
    const dc::GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += g->advance * scale;
      continue;
    }
    float x0 = cursorX + g->bearingX * scale;
    float y1 = baselineY + g->bearingY * scale;  // top of glyph (above baseline)
    float y0 = y1 - g->h * scale;                // bottom of glyph
    float x1 = x0 + g->w * scale;
    out.push_back(x0); out.push_back(y0); out.push_back(x1); out.push_back(y1);
    out.push_back(g->u0); out.push_back(g->v0); out.push_back(g->u1); out.push_back(g->v1);
    glyphCount++;
    cursorX += g->advance * scale;
  }
}

// ---- PPM output ----

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

int main() {
  constexpr int W = 800;
  constexpr int H = 600;
  constexpr int NUM_CANDLES = 50;

  // 1. Create GL context
  std::unique_ptr<dc::GlContext> ctx;
#ifdef DC_HAS_GLFW
  {
    auto glfw = std::make_unique<dc::GlfwContext>();
    if (glfw->init(W, H)) ctx = std::move(glfw);
  }
#endif
#ifdef DC_HAS_OSMESA
  if (!ctx) {
    auto mesa = std::make_unique<dc::OsMesaContext>();
    if (!mesa->init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }
    ctx = std::move(mesa);
    std::printf("Using OSMesa (headless)\n");
  }
#endif
  if (!ctx) { std::fprintf(stderr, "No GL context\n"); return 1; }

  // 2. Load font + ensure ASCII glyphs
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(1024);
  atlas.setGlyphPx(48);
#ifdef FONT_PATH
  if (!atlas.loadFontFile(FONT_PATH)) {
    std::fprintf(stderr, "Warning: Could not load font, text will not render\n");
  } else {
    atlas.ensureAscii();
    std::printf("Font loaded, ASCII glyphs ready\n");
  }
#endif

  // 3. Scene infrastructure via CommandProcessor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  cp.setGlyphAtlas(&atlas);

  // Pane (id=1)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Chart"})"), "pane");
  // Layers (ids 2,3,4,5)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"Grid"})"), "gridLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":3,"paneId":1,"name":"Volume"})"), "volLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":4,"paneId":1,"name":"Candles"})"), "candleLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":5,"paneId":1,"name":"Labels"})"), "labelLayer");

  // Shared transform for pan/zoom (id=6)
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":6})"), "sharedXform");

  // 4. Build recipes using deterministic IDs
  // Grid recipe (idBase=100) — line2d for horizontal grid lines
  dc::LineRecipeConfig gridCfg;
  gridCfg.layerId = 2;
  gridCfg.name = "Grid";
  gridCfg.createTransform = false; // use shared transform
  dc::LineRecipe gridRecipe(100, gridCfg);

  // Volume recipe (idBase=200) — we use a special rect recipe manually
  // (no RectRecipe yet, so we create buffer/geom/di manually via commands)

  // Candle recipe (idBase=300)
  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 4;
  candleCfg.name = "OHLC";
  candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(300, candleCfg);

  // Title text recipe (idBase=400)
  dc::TextRecipeConfig titleCfg;
  titleCfg.layerId = 5;
  titleCfg.name = "Title";
  titleCfg.createTransform = false;
  dc::TextRecipe titleRecipe(400, titleCfg);

  // Price label text recipe (idBase=500)
  dc::TextRecipeConfig priceCfg;
  priceCfg.layerId = 5;
  priceCfg.name = "PriceLabel";
  priceCfg.createTransform = false;
  dc::TextRecipe priceRecipe(500, priceCfg);

  // Apply recipe create commands
  auto applyRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.createCommands) {
      requireOk(cp.applyJsonText(cmd), name);
    }
  };

  applyRecipe(gridRecipe.build(), "grid");
  applyRecipe(candleRecipe.build(), "candle");
  applyRecipe(titleRecipe.build(), "title");
  applyRecipe(priceRecipe.build(), "price");

  // Attach shared transform to grid and candle draw items
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":102,"transformId":6})"), "gridXform");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":302,"transformId":6})"), "candleXform");

  // Volume bars (manual — no recipe for instancedRect yet)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":200,"byteLength":0})"), "volBuf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":201,"vertexBufferId":200,"format":"rect4","vertexCount":1})"),
    "volGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":202,"layerId":3,"name":"VolBars"})"), "volDI");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":202,"pipeline":"instancedRect@1","geometryId":201})"), "volBind");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":202,"transformId":6})"), "volXform");

  // 5. Generate data
  std::vector<Candle> candles;
  std::vector<VolumeBar> volumes;
  std::vector<float> gridVerts;
  generateCandles(NUM_CANDLES, candles, volumes);
  generateGrid(gridVerts);

  // Build text instance data
  std::vector<float> titleInstances;
  int titleGlyphCount = 0;
  buildTextInstances(atlas, "DynaCharting - Recipe Demo",
                     -0.85f, 0.92f, 0.06f, 48.0f,
                     titleInstances, titleGlyphCount);

  std::vector<float> priceInstances;
  int priceGlyphCount = 0;
  buildTextInstances(atlas, "$100.00",
                     0.6f, 0.85f, 0.05f, 48.0f,
                     priceInstances, priceGlyphCount);

  std::printf("Title: %d glyphs, Price: %d glyphs\n", titleGlyphCount, priceGlyphCount);

  // 6. Binary ingest
  std::vector<std::uint8_t> batch;

  // Grid lines → buffer 100
  appendRecord(batch, 1, 100, 0, gridVerts.data(),
               static_cast<std::uint32_t>(gridVerts.size() * sizeof(float)));
  // Volume bars → buffer 200
  appendRecord(batch, 1, 200, 0, volumes.data(),
               static_cast<std::uint32_t>(volumes.size() * sizeof(VolumeBar)));
  // Candles → buffer 300
  appendRecord(batch, 1, 300, 0, candles.data(),
               static_cast<std::uint32_t>(candles.size() * sizeof(Candle)));
  // Title text → buffer 400
  if (!titleInstances.empty()) {
    appendRecord(batch, 1, 400, 0, titleInstances.data(),
                 static_cast<std::uint32_t>(titleInstances.size() * sizeof(float)));
  }
  // Price label → buffer 500
  if (!priceInstances.empty()) {
    appendRecord(batch, 1, 500, 0, priceInstances.data(),
                 static_cast<std::uint32_t>(priceInstances.size() * sizeof(float)));
  }

  dc::IngestResult ir = ingest.processBatch(batch.data(),
                                             static_cast<std::uint32_t>(batch.size()));
  std::printf("Ingested: %u payload bytes, %zu touched buffers\n",
              ir.payloadBytes, ir.touchedBufferIds.size());

  // Update geometry vertex counts via commands
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":)" +
    std::to_string(gridVerts.size() / 2) + "}"), "gridVC");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":201,"vertexCount":)" +
    std::to_string(volumes.size()) + "}"), "volVC");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":301,"vertexCount":)" +
    std::to_string(candles.size()) + "}"), "candleVC");
  if (titleGlyphCount > 0) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryVertexCount","geometryId":401,"vertexCount":)" +
      std::to_string(titleGlyphCount) + "}"), "titleVC");
  }
  if (priceGlyphCount > 0) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryVertexCount","geometryId":501,"vertexCount":)" +
      std::to_string(priceGlyphCount) + "}"), "priceVC");
  }

  // 7. Upload to GPU
  dc::GpuBufferManager gpuBufs;
  for (dc::Id id : ir.touchedBufferIds) {
    gpuBufs.setCpuData(id, ingest.getBufferData(id), ingest.getBufferSize(id));
  }

  dc::Renderer renderer;
  if (!renderer.init()) { std::fprintf(stderr, "Renderer init failed\n"); return 1; }
  renderer.setGlyphAtlas(&atlas);
  gpuBufs.uploadDirty();

  // 8. Render
  dc::Stats stats = renderer.render(scene, gpuBufs, ctx->width(), ctx->height());
  ctx->swapBuffers();
  std::printf("Rendered: %u draw calls\n", stats.drawCalls);

  auto pixels = ctx->readPixels();
  writePPM("d3_5_recipe_chart.ppm", pixels, ctx->width(), ctx->height());

  // 9. Dispose all recipes
  auto disposeRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.disposeCommands) {
      requireOk(cp.applyJsonText(cmd), name);
    }
  };

  disposeRecipe(priceRecipe.build(), "dispose price");
  disposeRecipe(titleRecipe.build(), "dispose title");
  disposeRecipe(candleRecipe.build(), "dispose candle");
  disposeRecipe(gridRecipe.build(), "dispose grid");

  std::printf("Recipes disposed. Scene drawItems remaining: %zu\n",
              scene.drawItemIds().size());

  std::printf("D3.5 recipe demo complete\n");
  return 0;
}
