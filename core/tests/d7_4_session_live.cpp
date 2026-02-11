// D7.4 — ChartSession live integration test (GL, OSMesa)
// Tests: ChartSession with FakeDataSource → GL render → verify visible output.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/data/FakeDataSource.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/viewport/Viewport.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
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

int main() {
  constexpr int W = 400, H = 300;

  // 1. GL context
  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }

  // 2. Scene + processors
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // 3. Create pane + layers (these are NOT managed by ChartSession)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Line"})"), "l11");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

  // 4. Viewport
  dc::Viewport vp;
  vp.setPixelViewport(W, H);
  vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
  vp.setDataRange(-5.0, 30.0, 90.0, 110.0);

  // 5. ChartSession
  dc::ChartSession session(cp, ingest);
  dc::LiveIngestLoopConfig loopCfg;
  loopCfg.autoScrollX = true;
  loopCfg.autoScaleY = true;
  session.loop().setConfig(loopCfg);
  session.setViewport(&vp);

  // 6. Mount recipes via session
  auto hCandle = session.mount(
    std::make_unique<dc::CandleRecipe>(100,
      dc::CandleRecipeConfig{0, 10, "OHLC", false}), 50);

  auto hLine = session.mount(
    std::make_unique<dc::LineRecipe>(200,
      dc::LineRecipeConfig{0, 11, "CloseLine", false}), 50);

  // Set close-line color
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":202,"r":0.0,"g":0.8,"b":1.0,"a":1.0})"),
    "lineColor");

  // 7. GPU
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");

  // 8. Data source
  dc::FakeDataSourceConfig srcCfg;
  srcCfg.candleBufferId = 100;
  srcCfg.lineBufferId = 200;
  srcCfg.tickIntervalMs = 20;
  srcCfg.candleIntervalMs = 100;
  srcCfg.startPrice = 100.0f;
  srcCfg.volatility = 0.5f;

  dc::FakeDataSource src(srcCfg);
  src.start();

  // 9. Run 20 frames using session.update()
  for (int frame = 0; frame < 20; frame++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    auto fr = session.update(src);
    if (fr.dataChanged) {
      for (dc::Id bid : fr.touchedBufferIds) {
        auto sz = ingest.getBufferSize(bid);
        if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
      }
    }
  }

  src.stop();

  // Drain remaining
  auto lastFr = session.update(src);
  for (dc::Id bid : lastFr.touchedBufferIds) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }

  // --- Test 1: candle count > 0 ---
  auto candleSz = ingest.getBufferSize(100);
  std::uint32_t numCandles = candleSz / 24;
  requireTrue(numCandles > 0, "candle count > 0");
  std::printf("  candle count: %u\n", numCandles);

  // --- Test 2: geometry vertex counts match ---
  const auto* geom = scene.getGeometry(101);
  requireTrue(geom != nullptr, "candle geometry exists");
  requireTrue(geom->vertexCount == numCandles, "candle VC matches");

  const auto* lineGeom = scene.getGeometry(201);
  requireTrue(lineGeom != nullptr, "line geometry exists");
  auto lineSz = ingest.getBufferSize(200);
  requireTrue(lineGeom->vertexCount == lineSz / 8, "line VC matches");
  std::printf("  geometry VCs: candle=%u line=%u\n", geom->vertexCount, lineGeom->vertexCount);

  // --- Test 3: render produces visible output ---
  // Final GPU sync
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
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

  // --- Test 4: transform was synced ---
  const auto* xf = scene.getTransform(50);
  requireTrue(xf != nullptr, "transform exists");
  auto tp = vp.computeTransformParams();
  requireTrue(std::fabs(xf->params.sx - tp.sx) < 1e-5f &&
              std::fabs(xf->params.sy - tp.sy) < 1e-5f,
              "transform params match viewport");
  std::printf("  transform synced: sx=%.4f sy=%.4f\n",
              static_cast<double>(tp.sx), static_cast<double>(tp.sy));

  // --- Test 5: unmountAll cleans up ---
  session.unmountAll();
  requireTrue(scene.getDrawItem(102) == nullptr, "candle DI removed");
  requireTrue(scene.getDrawItem(202) == nullptr, "line DI removed");
  requireTrue(scene.getGeometry(101) == nullptr, "candle geom removed");
  requireTrue(scene.getGeometry(201) == nullptr, "line geom removed");
  std::printf("  unmountAll cleanup PASS\n");

  std::printf("\nD7.4 session live PASS\n");
  return 0;
}
