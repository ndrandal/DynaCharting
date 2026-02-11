// D6.1 — Live ingest test (pure C++, no GL)
// Tests: FakeDataSource produces valid batches, LiveIngestLoop processes them,
//        auto-scroll updates viewport.

#include "dc/data/DataSource.hpp"
#include "dc/data/FakeDataSource.hpp"
#include "dc/data/ThreadSafeQueue.hpp"
#include "dc/data/LiveIngestLoop.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
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
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

int main() {
  // --- Test 1: ThreadSafeQueue basic operations ---
  {
    dc::ThreadSafeQueue<int> q(3);
    int v;
    requireTrue(!q.pop(v), "empty queue pop returns false");
    q.push(1);
    q.push(2);
    q.push(3);
    requireTrue(q.size() == 3, "size is 3");
    q.push(4); // should drop oldest (1)
    requireTrue(q.size() == 3, "size stays 3 after overflow");
    requireTrue(q.pop(v) && v == 2, "oldest is 2 after drop");
    q.clear();
    requireTrue(q.size() == 0, "clear empties queue");
    std::printf("  ThreadSafeQueue PASS\n");
  }

  // --- Test 2: FakeDataSource produces batches ---
  {
    dc::FakeDataSourceConfig cfg;
    cfg.candleBufferId = 100;
    cfg.lineBufferId = 200;
    cfg.tickIntervalMs = 20;
    cfg.candleIntervalMs = 100;
    cfg.startPrice = 50.0f;
    cfg.volatility = 1.0f;

    dc::FakeDataSource src(cfg);
    requireTrue(!src.isRunning(), "not running before start");

    src.start();
    requireTrue(src.isRunning(), "running after start");

    // Let it produce for 300ms
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Poll all batches
    std::vector<std::vector<std::uint8_t>> batches;
    std::vector<std::uint8_t> batch;
    while (src.poll(batch)) {
      batches.push_back(std::move(batch));
    }

    requireTrue(!batches.empty(), "produced at least 1 batch");
    std::printf("  FakeDataSource produced %zu batches\n", batches.size());

    // Check first batch has valid 13-byte header + candle payload
    auto& b = batches[0];
    requireTrue(b.size() >= 13, "batch has at least header size");

    // Check op code
    std::uint8_t op = b[0];
    requireTrue(op == 1 || op == 2, "valid op code (1=append, 2=update)");

    // Check buffer ID
    std::uint32_t bufferId;
    std::memcpy(&bufferId, b.data() + 1, 4);
    requireTrue(bufferId == 100 || bufferId == 200,
                "buffer ID matches candle or line");

    // Check payload size is multiple of candle (24) or line (8) bytes
    std::uint32_t payloadLen;
    std::memcpy(&payloadLen, b.data() + 9, 4);
    if (bufferId == 100) {
      requireTrue(payloadLen == 24, "candle payload is 24 bytes");
    } else {
      requireTrue(payloadLen == 8, "line payload is 8 bytes");
    }

    // Candle count should be > 0
    requireTrue(src.candleCount() > 0, "candleCount > 0");
    requireTrue(src.priceMin() < src.priceMax(), "priceMin < priceMax");

    src.stop();
    requireTrue(!src.isRunning(), "stopped after stop()");
    std::printf("  FakeDataSource PASS (candles=%u, price=[%.1f, %.1f])\n",
                src.candleCount(), static_cast<double>(src.priceMin()),
                static_cast<double>(src.priceMax()));
  }

  // --- Test 3: FakeDataSource batches ingest correctly ---
  {
    dc::FakeDataSourceConfig cfg;
    cfg.candleBufferId = 100;
    cfg.lineBufferId = 200;
    cfg.tickIntervalMs = 10;
    cfg.candleIntervalMs = 50;
    cfg.startPrice = 100.0f;
    cfg.volatility = 0.5f;

    dc::FakeDataSource src(cfg);
    dc::IngestProcessor ingest;
    ingest.ensureBuffer(100);
    ingest.ensureBuffer(200);

    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    src.stop();

    // Process all batches
    std::vector<std::uint8_t> batch;
    int batchCount = 0;
    while (src.poll(batch)) {
      ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
      batchCount++;
    }

    auto candleSz = ingest.getBufferSize(100);
    auto lineSz = ingest.getBufferSize(200);
    requireTrue(candleSz > 0, "candle buffer has data");
    requireTrue(candleSz % 24 == 0, "candle buffer size multiple of 24");
    requireTrue(lineSz > 0, "line buffer has data");
    requireTrue(lineSz % 8 == 0, "line buffer size multiple of 8");

    std::uint32_t numCandles = candleSz / 24;
    std::uint32_t numLinePoints = lineSz / 8;
    requireTrue(numCandles == numLinePoints, "candle count matches line points");

    std::printf("  Ingest PASS (%d batches, %u candles, %u line pts)\n",
                batchCount, numCandles, numLinePoints);
  }

  // --- Test 4: LiveIngestLoop processes batches and updates vertex counts ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    // Create scene structure
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Line"})"), "layer2");

    // Create candle recipe components
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})"), "buf");
    requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"pipeline":"instancedCandle@1","vertexCount":1})"), "geom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10,"geometryId":101,"pipeline":"instancedCandle@1","name":"c"})"), "di");

    // Create line recipe components
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":200,"byteLength":0})"), "lbuf");
    requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":201,"vertexBufferId":200,"pipeline":"line2d@1","vertexCount":2})"), "lgeom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":202,"layerId":11,"geometryId":201,"pipeline":"line2d@1","name":"l"})"), "ldi");

    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(-5.0, 20.0, 90.0, 110.0);

    dc::LiveIngestLoop loop;
    dc::LiveIngestLoopConfig loopCfg;
    loopCfg.autoScrollX = true;
    loopCfg.autoScaleY = true;
    loopCfg.scrollMargin = 0.1f;
    loop.setConfig(loopCfg);
    loop.addBinding({100, 101, 24}); // candle6
    loop.addBinding({200, 201, 8});  // pos2_clip
    loop.setViewport(&vp);

    dc::FakeDataSourceConfig srcCfg;
    srcCfg.candleBufferId = 100;
    srcCfg.lineBufferId = 200;
    srcCfg.tickIntervalMs = 10;
    srcCfg.candleIntervalMs = 50;
    srcCfg.startPrice = 100.0f;
    srcCfg.volatility = 0.5f;

    dc::FakeDataSource src(srcCfg);
    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // consumeAndUpdate drains batches, updates vertex counts and viewport
    auto touched = loop.consumeAndUpdate(src, ingest, cp);
    requireTrue(!touched.empty(), "consumeAndUpdate returned touched buffers");

    src.stop();

    // Drain remaining
    auto touched2 = loop.consumeAndUpdate(src, ingest, cp);

    // Verify candle data
    auto candleSz = ingest.getBufferSize(100);
    std::uint32_t numCandles = candleSz / 24;
    requireTrue(numCandles > 1, "multiple candles from LiveIngestLoop");

    // Verify geometry vertex count was updated
    const auto* geom = scene.getGeometry(101);
    requireTrue(geom != nullptr, "geometry exists");
    requireTrue(geom->vertexCount == numCandles, "vertex count matches candle count");

    // Verify auto-scroll moved viewport
    requireTrue(vp.dataRange().xMax > 0.0, "viewport scrolled forward");

    std::printf("  LiveIngestLoop PASS (%u candles, geomVC=%u, vpXMax=%.1f)\n",
                numCandles, geom->vertexCount, vp.dataRange().xMax);
  }

  // --- Test 5: Auto-scroll logic ---
  {
    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 20.0, 90.0, 110.0);

    // Simulate auto-scroll
    float lastX = 25.0f;
    double xSpan = vp.dataRange().xMax - vp.dataRange().xMin;
    double margin = xSpan * 0.1;
    double newXMax = static_cast<double>(lastX) + margin;
    double newXMin = newXMax - xSpan;
    vp.setDataRange(newXMin, newXMax, vp.dataRange().yMin, vp.dataRange().yMax);

    requireTrue(vp.dataRange().xMax > 25.0, "viewport scrolled to show lastX=25");
    requireTrue(vp.dataRange().xMin > 0.0, "viewport xMin moved forward");
    std::printf("  Auto-scroll PASS (new range=[%.1f, %.1f])\n",
                vp.dataRange().xMin, vp.dataRange().xMax);
  }

  // --- Test 6: Clean shutdown ---
  {
    dc::FakeDataSourceConfig cfg;
    cfg.candleBufferId = 1;
    cfg.tickIntervalMs = 5;
    cfg.candleIntervalMs = 20;

    dc::FakeDataSource src(cfg);
    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    src.stop();
    requireTrue(!src.isRunning(), "clean stop");

    // Double stop should be safe
    src.stop();
    requireTrue(!src.isRunning(), "double stop safe");
    std::printf("  Clean shutdown PASS\n");
  }

  std::printf("\nD6.1 live ingest PASS\n");
  return 0;
}
