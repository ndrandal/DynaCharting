// D2.7 — Live Candlestick Chart Demo
// Renders a fake candlestick chart with volume bars and grid lines.
// Uses GLFW if available (interactive pan/zoom), otherwise OSMesa (writes PPM).

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GlContext.hpp"

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

// ---- Fake OHLCV data generation ----

struct Candle {
  float x, open, high, low, close, halfWidth;
};

struct VolumeBar {
  float x0, y0, x1, y1; // rect4
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

  for (int i = 0; i < count; i++) {
    float x = -0.9f + barW * (static_cast<float>(i) + 0.5f);
    float change = (rng() - 0.5f) * 4.0f;
    float open = price;
    float close = price + change;
    float high = std::fmax(open, close) + rng() * 2.0f;
    float low  = std::fmin(open, close) - rng() * 2.0f;
    price = close;

    // Normalize prices to clip space [-0.8, 0.8]
    // We'll use a simple mapping: price range 80–120 → clip -0.8 to 0.8
    auto norm = [](float p) { return (p - 100.0f) / 25.0f; };

    Candle c;
    c.x = x;
    c.open = norm(open);
    c.high = norm(high);
    c.low  = norm(low);
    c.close = norm(close);
    c.halfWidth = hw;
    candles.push_back(c);

    // Volume bar (bottom of screen)
    float vol = rng() * 0.3f;
    VolumeBar vb;
    vb.x0 = x - hw;
    vb.y0 = -0.95f;
    vb.x1 = x + hw;
    vb.y1 = -0.95f + vol;
    volumes.push_back(vb);
  }
}

// Generate horizontal grid lines
static void generateGrid(std::vector<float>& gridVerts) {
  gridVerts.clear();
  for (int i = -4; i <= 4; i++) {
    float y = static_cast<float>(i) * 0.2f;
    gridVerts.push_back(-1.0f); gridVerts.push_back(y);
    gridVerts.push_back( 1.0f); gridVerts.push_back(y);
  }
}

// ---- PPM output for headless mode ----

static void writePPM(const char* filename, const std::vector<std::uint8_t>& pixels,
                      int w, int h) {
  FILE* f = std::fopen(filename, "wb");
  if (!f) {
    std::fprintf(stderr, "Cannot open %s for writing\n", filename);
    return;
  }
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  // OpenGL pixels are bottom-up; flip for PPM (top-down)
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
      std::fputc(pixels[idx + 0], f); // R
      std::fputc(pixels[idx + 1], f); // G
      std::fputc(pixels[idx + 2], f); // B
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
  bool useGlfw = true;
#else
  bool useGlfw = false;
#endif

#ifdef DC_HAS_GLFW
  if (useGlfw) {
    auto glfw = std::make_unique<dc::GlfwContext>();
    if (!glfw->init(W, H)) {
      std::fprintf(stderr, "GLFW init failed, falling back to OSMesa\n");
      useGlfw = false;
    } else {
      ctx = std::move(glfw);
    }
  }
#endif

#ifdef DC_HAS_OSMESA
  if (!ctx) {
    auto mesa = std::make_unique<dc::OsMesaContext>();
    if (!mesa->init(W, H)) {
      std::fprintf(stderr, "OSMesa init failed\n");
      return 1;
    }
    ctx = std::move(mesa);
    std::printf("Using OSMesa (headless)\n");
  }
#endif

  if (!ctx) {
    std::fprintf(stderr, "No GL context available\n");
    return 1;
  }

  // 2. Build scene via CommandProcessor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Pane (id=1)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"Chart"})"), "pane");

  // Grid layer (id=2)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"Grid"})"), "gridLayer");
  // Volume layer (id=3)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"Volume"})"), "volLayer");
  // Candle layer (id=4)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"Candles"})"), "candleLayer");

  // Shared transform (id=5)
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform"})"), "transform");

  // Grid buffer (id=6) + geometry (id=7) + drawItem (id=8)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "gridBuf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":6,"format":"pos2_clip","vertexCount":2})"),
    "gridGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"GridLines"})"), "gridDI");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":8,"pipeline":"line2d@1","geometryId":7})"), "gridBind");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":8,"transformId":5})"), "gridXform");

  // Volume buffer (id=9) + geometry (id=10) + drawItem (id=11)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "volBuf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":9,"format":"rect4","vertexCount":1})"),
    "volGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":3,"name":"VolBars"})"), "volDI");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":11,"pipeline":"instancedRect@1","geometryId":10})"), "volBind");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":11,"transformId":5})"), "volXform");

  // Candle buffer (id=12) + geometry (id=13) + drawItem (id=14)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "candleBuf");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":12,"format":"candle6","vertexCount":1})"),
    "candleGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":4,"name":"Candles"})"), "candleDI");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":14,"pipeline":"instancedCandle@1","geometryId":13})"), "candleBind");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":14,"transformId":5})"), "candleXform");

  // 3. Generate fake data
  std::vector<Candle> candles;
  std::vector<VolumeBar> volumes;
  std::vector<float> gridVerts;

  generateCandles(NUM_CANDLES, candles, volumes);
  generateGrid(gridVerts);

  // 4. Binary ingest
  dc::IngestProcessor ingest;
  std::vector<std::uint8_t> batch;

  // Grid lines
  appendRecord(batch, 1, 6, 0, gridVerts.data(),
               static_cast<std::uint32_t>(gridVerts.size() * sizeof(float)));
  // Volume bars
  appendRecord(batch, 1, 9, 0, volumes.data(),
               static_cast<std::uint32_t>(volumes.size() * sizeof(VolumeBar)));
  // Candles
  appendRecord(batch, 1, 12, 0, candles.data(),
               static_cast<std::uint32_t>(candles.size() * sizeof(Candle)));

  dc::IngestResult ir = ingest.processBatch(batch.data(),
                                             static_cast<std::uint32_t>(batch.size()));
  std::printf("Ingested: %u payload bytes, %zu touched buffers\n",
              ir.payloadBytes, ir.touchedBufferIds.size());

  // Update geometry vertex counts
  auto* gridGeom = const_cast<dc::Geometry*>(scene.getGeometry(7));
  gridGeom->vertexCount = static_cast<std::uint32_t>(gridVerts.size() / 2);
  auto* volGeom = const_cast<dc::Geometry*>(scene.getGeometry(10));
  volGeom->vertexCount = static_cast<std::uint32_t>(volumes.size());
  auto* candleGeom = const_cast<dc::Geometry*>(scene.getGeometry(13));
  candleGeom->vertexCount = static_cast<std::uint32_t>(candles.size());

  // 5. Upload to GPU
  dc::GpuBufferManager gpuBufs;
  for (dc::Id id : ir.touchedBufferIds) {
    gpuBufs.setCpuData(id, ingest.getBufferData(id), ingest.getBufferSize(id));
  }

  dc::Renderer renderer;
  if (!renderer.init()) {
    std::fprintf(stderr, "Renderer init failed\n");
    return 1;
  }

  gpuBufs.uploadDirty();

  // 6. Render
  float tx = 0.0f, ty = 0.0f, sx = 1.0f, sy = 1.0f;

#ifdef DC_HAS_GLFW
  if (useGlfw) {
    auto* glfwCtx = static_cast<dc::GlfwContext*>(ctx.get());
    std::printf("GLFW render loop — drag to pan, scroll to zoom, close window to exit\n");

    while (!glfwCtx->shouldClose()) {
      dc::InputState input = glfwCtx->pollInput();

      // Apply pan/zoom
      if (input.panDx != 0 || input.panDy != 0) {
        tx += static_cast<float>(input.panDx) * 2.0f / static_cast<float>(glfwCtx->width());
        ty -= static_cast<float>(input.panDy) * 2.0f / static_cast<float>(glfwCtx->height());
      }
      if (input.zoomDelta != 0) {
        float factor = 1.0f + static_cast<float>(input.zoomDelta) * 0.1f;
        sx *= factor;
        sy *= factor;
      }

      // Update transform
      cp.applyJsonText(
        std::string(R"({"cmd":"setTransform","id":5,"tx":)") + std::to_string(tx) +
        R"(,"ty":)" + std::to_string(ty) +
        R"(,"sx":)" + std::to_string(sx) +
        R"(,"sy":)" + std::to_string(sy) + "}");

      dc::Stats stats = renderer.render(scene, gpuBufs,
                                         glfwCtx->width(), glfwCtx->height());
      glfwCtx->swapBuffers();
      (void)stats;
    }
  } else
#endif
  {
    // OSMesa: single render, write PPM
    dc::Stats stats = renderer.render(scene, gpuBufs, ctx->width(), ctx->height());
    ctx->swapBuffers();
    std::printf("Rendered: %u draw calls\n", stats.drawCalls);

    auto pixels = ctx->readPixels();
    writePPM("d2_7_candle_chart.ppm", pixels, ctx->width(), ctx->height());
  }

  std::printf("D2.7 demo complete\n");
  return 0;
}
