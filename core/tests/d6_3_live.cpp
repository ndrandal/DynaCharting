// D6.3 — Live streaming integration test (GL, OSMesa)
// Tests: FakeDataSource → LiveIngestLoop → GPU render → verify visible output.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"

#include "dc/data/FakeDataSource.hpp"
#include "dc/data/LiveIngestLoop.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/viewport/Viewport.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

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

int main() {
  constexpr int W = 400, H = 300;

  // 1. GL context
  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }

  // 2. Scene
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Line"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

  // 3. Recipes
  dc::CandleRecipeConfig candleCfg;
  candleCfg.layerId = 10; candleCfg.name = "OHLC"; candleCfg.createTransform = false;
  dc::CandleRecipe candleRecipe(100, candleCfg);

  dc::LineRecipeConfig lineCfg;
  lineCfg.layerId = 11; lineCfg.name = "CloseLine"; lineCfg.createTransform = false;
  dc::LineRecipe lineRecipe(200, lineCfg);

  for (auto& cmd : candleRecipe.build().createCommands)
    requireOk(cp.applyJsonText(cmd), "candle");
  for (auto& cmd : lineRecipe.build().createCommands)
    requireOk(cp.applyJsonText(cmd), "line");

  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":102,"transformId":50})"), "attach");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":202,"transformId":50})"), "attach2");

  // Set close-line color
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemColor","drawItemId":202,"r":0.0,"g":0.8,"b":1.0,"a":1.0})");
    requireOk(cp.applyJsonText(buf), "lineColor");
  }

  // 4. Viewport
  dc::Viewport vp;
  vp.setPixelViewport(W, H);
  vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
  vp.setDataRange(-5.0, 30.0, 90.0, 110.0);
  double origXMax = vp.dataRange().xMax;

  // 5. Data source + loop
  dc::FakeDataSourceConfig srcCfg;
  srcCfg.candleBufferId = 100;
  srcCfg.lineBufferId = 200;
  srcCfg.tickIntervalMs = 20;
  srcCfg.candleIntervalMs = 100;
  srcCfg.startPrice = 100.0f;
  srcCfg.volatility = 0.5f;

  dc::FakeDataSource src(srcCfg);

  dc::LiveIngestLoop loop;
  dc::LiveIngestLoopConfig loopCfg;
  loopCfg.autoScrollX = true;
  loopCfg.autoScaleY = true;
  loop.setConfig(loopCfg);
  loop.addBinding({100, 101, 24});
  loop.addBinding({200, 201, 8});
  loop.setViewport(&vp);

  // 6. GPU
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");

  // 7. Start and stream
  src.start();

  for (int frame = 0; frame < 25; frame++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto touched = loop.consumeAndUpdate(src, ingest, cp);
    if (!touched.empty()) {
      for (dc::Id bid : touched) {
        auto sz = ingest.getBufferSize(bid);
        if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
      }
      setTransformCmd(cp, 50, vp.computeTransformParams());
    }
  }

  src.stop();

  // Drain remaining
  loop.consumeAndUpdate(src, ingest, cp);

  // --- Test 1: candle count > 0 ---
  auto candleSz = ingest.getBufferSize(100);
  std::uint32_t numCandles = candleSz / 24;
  requireTrue(numCandles > 0, "candle count > 0");
  std::printf("  candle count: %u\n", numCandles);

  // --- Test 2: vertex counts updated ---
  const auto* geom = scene.getGeometry(101);
  requireTrue(geom != nullptr, "candle geometry exists");
  requireTrue(geom->vertexCount == numCandles, "candle vertex count matches");

  const auto* lineGeom = scene.getGeometry(201);
  requireTrue(lineGeom != nullptr, "line geometry exists");
  auto lineSz = ingest.getBufferSize(200);
  requireTrue(lineGeom->vertexCount == lineSz / 8, "line vertex count matches");
  std::printf("  geometry VCs: candle=%u line=%u\n", geom->vertexCount, lineGeom->vertexCount);

  // --- Test 3: render produces visible candles ---
  // Final GPU sync + render
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  setTransformCmd(cp, 50, vp.computeTransformParams());
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats.drawCalls >= 1, "at least 1 draw call");

  auto pixels = ctx.readPixels();
  int nonBlack = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx] > 5 || pixels[idx + 1] > 5 || pixels[idx + 2] > 5)
        nonBlack++;
    }
  }
  requireTrue(nonBlack > 50, "candles visible (non-black pixels)");
  std::printf("  render: %u draw calls, %d non-black pixels\n",
              stats.drawCalls, nonBlack);

  // --- Test 4: auto-scroll adjusted viewport to track data ---
  if (numCandles > 2) {
    // Auto-scroll centers the viewport on the actual candle data range.
    // The last candle X should be near the right edge of the viewport.
    float lastX = static_cast<float>(numCandles - 1);
    requireTrue(vp.dataRange().xMax > static_cast<double>(lastX) - 1.0,
                "viewport tracks latest candle");
    std::printf("  viewport range: X=[%.1f, %.1f] Y=[%.1f, %.1f]\n",
                vp.dataRange().xMin, vp.dataRange().xMax,
                vp.dataRange().yMin, vp.dataRange().yMax);
  }

  // --- Test 5: clean shutdown ---
  requireTrue(!src.isRunning(), "source stopped");
  std::printf("  clean shutdown PASS\n");

  std::printf("\nD6.3 live PASS\n");
  return 0;
}
