// D6.3 — Live Streaming Demo
// GLFW: continuous render loop with live candle updates, auto-scroll, crosshair
// OSMesa fallback: run FakeDataSource for ~1s, render final frame, write PPM
// Use --ws ws://host:port to use WebSocket data source (if DC_HAS_WEBSOCKET)

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GlContext.hpp"

#include "dc/data/FakeDataSource.hpp"
#include "dc/data/LiveIngestLoop.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/recipe/CrosshairRecipe.hpp"
#include "dc/recipe/LevelLineRecipe.hpp"
#include "dc/layout/PaneLayout.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/InputMapper.hpp"

#ifdef DC_HAS_WEBSOCKET
#include "dc/data/WebSocketDataSource.hpp"
#endif

#ifdef DC_HAS_GLFW
#include "dc/gl/GlfwContext.hpp"
#endif
#ifdef DC_HAS_OSMESA
#include "dc/gl/OsMesaContext.hpp"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// ---- Helpers ----

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

static void setTransformCmd(dc::CommandProcessor& cp, dc::Id xfId,
                             const dc::TransformParams& tp) {
  char buf[256];
  std::snprintf(buf, sizeof(buf),
    R"({"cmd":"setTransform","id":%llu,"tx":%.6f,"ty":%.6f,"sx":%.6f,"sy":%.6f})",
    static_cast<unsigned long long>(xfId),
    static_cast<double>(tp.tx), static_cast<double>(tp.ty),
    static_cast<double>(tp.sx), static_cast<double>(tp.sy));
  requireOk(cp.applyJsonText(buf), "setTransform");
}

static void writeU32LE(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

static void appendRecord(std::vector<std::uint8_t>& batch,
                          std::uint8_t op, std::uint32_t bufferId,
                          std::uint32_t offset, const void* payload,
                          std::uint32_t len) {
  batch.push_back(op);
  writeU32LE(batch, bufferId);
  writeU32LE(batch, offset);
  writeU32LE(batch, len);
  const auto* p = static_cast<const std::uint8_t*>(payload);
  batch.insert(batch.end(), p, p + len);
}

static void ingestFloat(std::vector<std::uint8_t>& batch, std::uint32_t bid,
                          const std::vector<float>& v) {
  if (!v.empty())
    appendRecord(batch, 1, bid, 0, v.data(),
                 static_cast<std::uint32_t>(v.size() * sizeof(float)));
}

static void setVC(dc::CommandProcessor& cp, dc::Id geomId, std::uint32_t vc) {
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":)" + std::to_string(geomId) +
    R"(,"vertexCount":)" + std::to_string(vc) + "}"), "setVC");
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

// ---- ID Allocation ----
// 1: Pane
// 10-12: Layers (candles, close-line, overlays)
// 50: Transform
// 100-103: CandleRecipe (buffer, geom, drawItem, transform)
// 200-203: LineRecipe (close price line)
// 1000-1011: CrosshairRecipe
// 1040-1045: LevelLineRecipe (current price)

int main(int argc, char* argv[]) {
  constexpr int W = 1024, H = 768;

  // Parse args
  std::string wsUrl;
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--ws" && i + 1 < argc) {
      wsUrl = argv[++i];
    }
  }

  // 1. Create GL context
  std::unique_ptr<dc::GlContext> glCtx;
  bool isGlfw = false;
#ifdef DC_HAS_GLFW
  {
    auto g = std::make_unique<dc::GlfwContext>();
    if (g->init(W, H)) { glCtx = std::move(g); isGlfw = true; }
  }
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

  // Pane
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");

  // Layers
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"CloseLine"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":12,"paneId":1,"name":"CrossLines"})"), "l12");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":13,"paneId":1,"name":"CrossLabels"})"), "l13");

  // Shared transform
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

  // 4. Build recipes
  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 10; candleCfg.name = "OHLC"; candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(100, candleCfg);

  dc::LineRecipeConfig lineCfg;
  lineCfg.layerId = 11; lineCfg.name = "CloseLine"; lineCfg.createTransform = false;
  dc::LineRecipe lineRecipe(200, lineCfg);

  dc::CrosshairRecipeConfig crossCfg;
  crossCfg.paneId = 1; crossCfg.lineLayerId = 12;
  crossCfg.labelLayerId = 13; crossCfg.name = "Cross";
  dc::CrosshairRecipe crossRecipe(1000, crossCfg);

  dc::LevelLineRecipeConfig levelCfg;
  levelCfg.paneId = 1; levelCfg.lineLayerId = 12;
  levelCfg.labelLayerId = 13; levelCfg.name = "CurPrice";
  dc::LevelLineRecipe levelRecipe(1040, levelCfg);

  // Apply recipes
  auto applyRecipe = [&](const dc::RecipeBuildResult& r, const char* name) {
    for (auto& cmd : r.createCommands) requireOk(cp.applyJsonText(cmd), name);
  };
  applyRecipe(candleRecipe.build(), "candle");
  applyRecipe(lineRecipe.build(), "line");
  applyRecipe(crossRecipe.build(), "cross");
  applyRecipe(levelRecipe.build(), "level");

  // Attach shared transform to candle + line draw items
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":102,"transformId":50})"), "attachCandle");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":202,"transformId":50})"), "attachLine");

  // Set colors
  setColor(cp, 202, 0.0f, 0.8f, 1.0f, 1.0f);  // close-price line: cyan
  setColor(cp, 1002, 0.8f, 0.8f, 0.8f, 0.5f);  // crosshair H-line
  setColor(cp, 1005, 0.8f, 0.8f, 0.8f, 0.5f);  // crosshair V-line
  setColor(cp, 1042, 1.0f, 0.4f, 0.0f, 0.8f);  // level line: orange

  // 5. Setup viewport
  dc::PaneRegion paneRegion{-1.0f, 1.0f, -1.0f, 1.0f};
  dc::Viewport vp;
  vp.setPixelViewport(W, H);
  vp.setClipRegion(paneRegion);
  vp.setDataRange(-5.0, 50.0, 90.0, 110.0); // initial range

  dc::InputMapper inputMapper;
  dc::InputMapperConfig imCfg;
  imCfg.linkXAxis = false;
  imCfg.zoomSensitivity = 0.15;
  inputMapper.setConfig(imCfg);
  inputMapper.setViewports({&vp});

  // 6. Setup data source
  std::unique_ptr<dc::DataSource> dataSource;

#ifdef DC_HAS_WEBSOCKET
  if (!wsUrl.empty()) {
    dc::WebSocketDataSourceConfig wsCfg;
    wsCfg.url = wsUrl;
    wsCfg.reconnectIntervalMs = 3000;
    dataSource = std::make_unique<dc::WebSocketDataSource>(wsCfg);
    std::printf("Using WebSocket data source: %s\n", wsUrl.c_str());
  }
#endif

  if (!dataSource) {
    dc::FakeDataSourceConfig fakeCfg;
    fakeCfg.candleBufferId = 100; // matches candle recipe buffer
    fakeCfg.lineBufferId = 200;   // matches line recipe buffer
    fakeCfg.tickIntervalMs = 50;
    fakeCfg.candleIntervalMs = 1000;
    fakeCfg.startPrice = 100.0f;
    fakeCfg.volatility = 0.5f;
    fakeCfg.maxCandles = 200;
    dataSource = std::make_unique<dc::FakeDataSource>(fakeCfg);
    std::printf("Using FakeDataSource\n");
  }

  // 7. Setup LiveIngestLoop
  dc::LiveIngestLoop loop;
  dc::LiveIngestLoopConfig loopCfg;
  loopCfg.autoScrollX = true;
  loopCfg.autoScaleY = true;
  loopCfg.scrollMargin = 0.1f;
  loop.setConfig(loopCfg);
  loop.addBinding({100, 101, 24}); // candle6: buffer→geometry
  loop.addBinding({200, 201, 8});  // pos2_clip: buffer→geometry
  loop.setViewport(&vp);

  // 8. GPU setup
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  if (!renderer.init()) { std::fprintf(stderr, "Renderer init failed\n"); return 1; }
  renderer.setGlyphAtlas(&atlas);

  auto syncGpu = [&](const std::vector<dc::Id>& touchedIds) {
    for (dc::Id bid : touchedIds) {
      auto sz = ingest.getBufferSize(bid);
      if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
    }
    // Also sync any other buffers that might have data (crosshair/level)
    for (auto bid : scene.bufferIds()) {
      auto sz = ingest.getBufferSize(bid);
      if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
    }
    gpuBufs.uploadDirty();
  };

  // 9. Start data source
  dataSource->start();

  if (!isGlfw) {
    // OSMesa: run for ~1s, render final frame
    std::printf("Streaming for 1 second...\n");
    for (int frame = 0; frame < 20; frame++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      auto touched = loop.consumeAndUpdate(*dataSource, ingest, cp);
      if (!touched.empty()) {
        setTransformCmd(cp, 50, vp.computeTransformParams());
        syncGpu(touched);
      }
    }
    dataSource->stop();

    // Final render
    setTransformCmd(cp, 50, vp.computeTransformParams());
    syncGpu({});
    dc::Stats stats = renderer.render(scene, gpuBufs, glCtx->width(), glCtx->height());
    std::printf("Rendered: %u draw calls\n", stats.drawCalls);

    auto pixels = glCtx->readPixels();
    writePPM("d6_3_live_chart.ppm", pixels, glCtx->width(), glCtx->height());

    // Count candles
    auto candleSz = ingest.getBufferSize(100);
    std::printf("Final candle count: %u\n", candleSz / 24);
    std::printf("D6.3 live demo complete (OSMesa)\n");
    return 0;
  }

#ifdef DC_HAS_GLFW
  // GLFW interactive loop
  auto* glfwCtx = static_cast<dc::GlfwContext*>(glCtx.get());
  std::printf("Live chart — streaming data. Drag to pan, scroll to zoom, Esc to quit\n");

  while (!glfwCtx->shouldClose()) {
    dc::InputState is = glfwCtx->pollInput();
    dc::ViewportInputState vis = is.toViewportInput();

    // Input processing
    bool inputChanged = inputMapper.processInput(vis);

    // Consume data
    auto touched = loop.consumeAndUpdate(*dataSource, ingest, cp);
    bool dataChanged = !touched.empty();

    if (inputChanged || dataChanged) {
      setTransformCmd(cp, 50, vp.computeTransformParams());
    }

    // Update crosshair
    if (hasFont) {
      double cx, cy, dx, dy;
      vp.pixelToClip(vis.cursorX, vis.cursorY, cx, cy);
      vp.pixelToData(vis.cursorX, vis.cursorY, dx, dy);

      auto chData = crossRecipe.computeCrosshairData(
          cx, cy, dx, dy, paneRegion, atlas, 48.0f, 0.035f);

      if (chData.visible) {
        std::vector<std::uint8_t> chBatch;
        ingestFloat(chBatch, 1000, chData.hLineVerts);
        ingestFloat(chBatch, 1003, chData.vLineVerts);
        ingestFloat(chBatch, 1006, chData.priceLabelGlyphs);
        ingestFloat(chBatch, 1009, chData.timeLabelGlyphs);
        if (!chBatch.empty())
          ingest.processBatch(chBatch.data(), static_cast<std::uint32_t>(chBatch.size()));
        setVC(cp, 1001, 2);
        setVC(cp, 1004, 2);
        setVC(cp, 1007, chData.priceLabelGC);
        setVC(cp, 1010, chData.timeLabelGC);
      } else {
        setVC(cp, 1001, 0);
        setVC(cp, 1004, 0);
        setVC(cp, 1007, 0);
        setVC(cp, 1010, 0);
      }

      // Level line: current price
      auto candleSz = ingest.getBufferSize(100);
      if (candleSz >= 24) {
        std::uint32_t numCandles = candleSz / 24;
        const auto* data = ingest.getBufferData(100);
        float lastClose;
        std::memcpy(&lastClose, data + (numCandles - 1) * 24 + 16, sizeof(float));

        char priceBuf[32];
        std::snprintf(priceBuf, sizeof(priceBuf), "%.2f", static_cast<double>(lastClose));
        auto levelData = levelRecipe.computeLevels(
            {{static_cast<double>(lastClose), priceBuf}},
            paneRegion,
            vp.dataRange().yMin, vp.dataRange().yMax,
            atlas, 48.0f, 0.035f);

        if (!levelData.lineVerts.empty()) {
          std::vector<std::uint8_t> lvlBatch;
          ingestFloat(lvlBatch, 1040, levelData.lineVerts);
          ingestFloat(lvlBatch, 1043, levelData.labelGlyphs);
          ingest.processBatch(lvlBatch.data(), static_cast<std::uint32_t>(lvlBatch.size()));
          setVC(cp, 1041, levelData.lineVertexCount);
          setVC(cp, 1044, levelData.labelGlyphCount);
        }
      }
    }

    syncGpu({});
    renderer.render(scene, gpuBufs, glfwCtx->width(), glfwCtx->height());
    glfwCtx->swapBuffers();
  }

  dataSource->stop();
  std::printf("D6.3 live demo complete\n");
#endif

  return 0;
}
