// D8.6 — Aggregation GL integration test (OSMesa)
// Tests: ChartSession with aggregation → zoom in/out → verify render at each tier.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/data/FakeDataSource.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
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

static int countNonBlack(const std::vector<std::uint8_t>& pixels, int W, int H) {
  int count = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx] > 5 || pixels[idx + 1] > 5 || pixels[idx + 2] > 5)
        count++;
    }
  }
  return count;
}

static void syncBuffersToGpu(dc::IngestProcessor& ingest, dc::Scene& scene,
                              dc::GpuBufferManager& gpuBufs,
                              const std::vector<dc::Id>& bufIds) {
  for (dc::Id bid : bufIds) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
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

  // 3. Create pane + layer + transform
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

  // 4. Viewport — start zoomed in
  dc::Viewport vp;
  vp.setPixelViewport(W, H);
  vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
  vp.setDataRange(-5.0, 30.0, 90.0, 110.0);

  // 5. ChartSession with aggregation enabled
  dc::ChartSessionConfig cfg;
  cfg.enableAggregation = true;
  cfg.aggregation.aggBufferIdOffset = 50000;

  dc::ChartSession session(cp, ingest);
  session.setConfig(cfg);
  session.setViewport(&vp);

  dc::LiveIngestLoopConfig loopCfg;
  loopCfg.autoScrollX = true;
  loopCfg.autoScaleY = true;
  session.loop().setConfig(loopCfg);

  // 6. Mount candle recipe
  auto hCandle = session.mount(
    std::make_unique<dc::CandleRecipe>(100,
      dc::CandleRecipeConfig{0, 10, "OHLC", false}), 50);

  // 7. GPU
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");

  // 8. Stream 100 candles at Raw tier (zoomed in)
  dc::FakeDataSourceConfig srcCfg;
  srcCfg.candleBufferId = 100;
  srcCfg.lineBufferId = 999; // unused
  srcCfg.tickIntervalMs = 10;
  srcCfg.candleIntervalMs = 20;
  srcCfg.startPrice = 100.0f;
  srcCfg.volatility = 0.5f;

  dc::FakeDataSource src(srcCfg);
  src.start();

  // Run frames to accumulate candles
  for (int frame = 0; frame < 30; frame++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto fr = session.update(src);
    if (fr.dataChanged) {
      syncBuffersToGpu(ingest, scene, gpuBufs, fr.touchedBufferIds);
    }
  }
  src.stop();
  auto lastFr = session.update(src);
  syncBuffersToGpu(ingest, scene, gpuBufs, lastFr.touchedBufferIds);

  auto rawCandleCount = ingest.getBufferSize(100) / 24;
  std::printf("  raw candles: %u\n", rawCandleCount);
  requireTrue(rawCandleCount >= 10, "have enough raw candles");

  // --- Test 1: Render at Raw tier ---
  // Sync all scene buffers
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats.drawCalls >= 1, "raw: draw calls >= 1");

  auto pixels = ctx.readPixels();
  int nonBlack = countNonBlack(pixels, W, H);
  requireTrue(nonBlack > 50, "raw: non-black pixels visible");
  std::printf("  raw render: %u draws, %d non-black\n", stats.drawCalls, nonBlack);

  // Verify geometry at raw buffer
  const dc::Geometry* g = scene.getGeometry(101);
  requireTrue(g != nullptr, "geometry exists");
  requireTrue(g->vertexBufferId == 100, "geometry → raw buffer at Raw tier");
  std::printf("  Raw tier render PASS\n");

  // --- Test 2: Zoom way out → tier switch ---
  vp.setDataRange(-5.0, 500.0, 50.0, 150.0); // ppdu = 400/505 ≈ 0.79 → Agg8x

  dc::FakeDataSource src2(srcCfg);
  src2.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto fr2 = session.update(src2);
  src2.stop();

  requireTrue(fr2.resolutionChanged, "tier changed on zoom out");
  std::printf("  resolution changed: tier switched\n");

  // --- Test 3: Render at aggregated tier ---
  g = scene.getGeometry(101);
  requireTrue(g->vertexBufferId == 50100, "geometry → agg buffer");
  std::printf("  agg vertex count: %u\n", g->vertexCount);

  // Sync agg buffers to GPU
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  for (dc::Id bid : fr2.touchedBufferIds) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  gpuBufs.uploadDirty();

  stats = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats.drawCalls >= 1, "agg: draw calls >= 1");

  pixels = ctx.readPixels();
  nonBlack = countNonBlack(pixels, W, H);
  requireTrue(nonBlack > 10, "agg: non-black pixels present");
  std::printf("  agg render: %u draws, %d non-black\n", stats.drawCalls, nonBlack);
  std::printf("  Agg tier render PASS\n");

  // --- Test 4: Zoom back in → Raw ---
  vp.setDataRange(-5.0, 30.0, 90.0, 110.0); // ppdu = 400/35 ≈ 11.4 → Raw

  dc::FakeDataSource src3(srcCfg);
  src3.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto fr3 = session.update(src3);
  src3.stop();

  requireTrue(fr3.resolutionChanged, "tier changed back to Raw");

  g = scene.getGeometry(101);
  requireTrue(g->vertexBufferId == 100, "geometry → raw buffer after zoom in");

  // Sync and render
  for (auto bid : scene.bufferIds()) {
    auto sz = ingest.getBufferSize(bid);
    if (sz > 0) gpuBufs.setCpuData(bid, ingest.getBufferData(bid), sz);
  }
  gpuBufs.uploadDirty();

  stats = renderer.render(scene, gpuBufs, W, H);
  pixels = ctx.readPixels();
  nonBlack = countNonBlack(pixels, W, H);
  requireTrue(nonBlack > 50, "raw again: candles visible");
  std::printf("  back to Raw: %u draws, %d non-black\n", stats.drawCalls, nonBlack);
  std::printf("  Zoom back in PASS\n");

  // --- Test 5: Verify vertex counts match tier expectations ---
  auto rawCount = ingest.getBufferSize(100) / 24;
  g = scene.getGeometry(101);
  requireTrue(g->vertexCount == rawCount, "raw VC matches buffer size");
  std::printf("  vertex counts verified (raw=%u)\n", rawCount);

  std::printf("\nD8.6 agg live PASS\n");
  return 0;
}
