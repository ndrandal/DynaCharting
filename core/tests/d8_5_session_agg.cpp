// D8.5 — ChartSession aggregation integration test (pure C++, no GL)
// Tests: session with enableAggregation, tier switches via update(), smart retention.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
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

// candle6: x, open, high, low, close, halfWidth (6 floats = 24 bytes)
struct Candle6 {
  float x, open, high, low, close, halfWidth;
};

static std::vector<std::uint8_t> makeBatch(dc::Id bufferId,
                                             const std::vector<Candle6>& candles) {
  std::uint32_t payloadLen = static_cast<std::uint32_t>(candles.size() * sizeof(Candle6));
  std::vector<std::uint8_t> batch(13 + payloadLen);
  batch[0] = 1; // OP_APPEND
  std::uint32_t bid = static_cast<std::uint32_t>(bufferId);
  std::memcpy(&batch[1], &bid, 4);
  std::uint32_t zero = 0;
  std::memcpy(&batch[5], &zero, 4);
  std::memcpy(&batch[9], &payloadLen, 4);
  std::memcpy(&batch[13], candles.data(), payloadLen);
  return batch;
}

// Simple data source that feeds a batch once
class OneShotSource : public dc::DataSource {
public:
  void setBatch(std::vector<std::uint8_t> data) { data_ = std::move(data); }

  void start() override { ready_ = true; }
  void stop() override { ready_ = false; }
  bool isRunning() const override { return ready_; }
  bool poll(std::vector<std::uint8_t>& batch) override {
    if (!ready_ || data_.empty()) return false;
    batch = std::move(data_);
    data_.clear();
    return true;
  }
private:
  std::vector<std::uint8_t> data_;
  bool ready_{false};
};

int main() {
  // --- Test 1-5: Full session aggregation lifecycle ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    auto ok = [](const dc::CmdResult& r, const char* ctx) {
      if (!r.ok) {
        std::fprintf(stderr, "CMD FAIL [%s]: %s %s\n", ctx,
                     r.err.code.c_str(), r.err.message.c_str());
        std::exit(1);
      }
    };

    ok(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    ok(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "layer");
    ok(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

    dc::ChartSessionConfig cfg;
    cfg.enableAggregation = true;
    cfg.aggregation.aggBufferIdOffset = 50000;

    dc::ChartSession session(cp, ingest);
    session.setConfig(cfg);

    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    // Zoomed in: data range 0..40 → ppdu = 800/40 = 20
    vp.setDataRange(0.0, 40.0, 80.0, 120.0);
    session.setViewport(&vp);

    // Mount candle recipe (idBase=100: buf=100, geom=101, di=102)
    auto hCandle = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "OHLC", false}), 50);

    requireTrue(session.isMounted(hCandle), "candle mounted");

    // Verify agg buffer was created (100 + 50000 = 50100)
    requireTrue(scene.hasBuffer(50100), "agg buffer created in scene");

    // Feed 40 candles
    std::vector<Candle6> candles40;
    for (int i = 0; i < 40; ++i) {
      candles40.push_back({
        static_cast<float>(i), 100.0f + i, 105.0f + i,
        95.0f + i, 102.0f + i, 0.4f
      });
    }

    OneShotSource src;
    src.setBatch(makeBatch(100, candles40));
    src.start();

    auto fr = session.update(src);
    requireTrue(fr.dataChanged, "data changed after feeding");

    // Test 2: Zoomed in (ppdu=20) → Raw tier
    requireTrue(!fr.resolutionChanged, "no resolution change (still Raw)");

    const dc::Geometry* g = scene.getGeometry(101);
    requireTrue(g != nullptr, "geometry exists");
    requireTrue(g->vertexBufferId == 100, "geometry → raw buffer (zoomed in)");

    std::printf("  Zoomed in Raw PASS\n");

    // Test 3: Zoom out → ppdu < 6 → Agg2x
    vp.setDataRange(0.0, 200.0, 80.0, 120.0); // ppdu = 800/200 = 4

    OneShotSource emptySrc;
    emptySrc.start();
    auto fr2 = session.update(emptySrc);

    requireTrue(fr2.resolutionChanged, "resolution changed on zoom out");

    g = scene.getGeometry(101);
    requireTrue(g->vertexBufferId == 50100, "geometry → agg buffer (zoomed out)");

    // Verify agg buffer has 20 candles (40/2)
    std::uint32_t aggSize = ingest.getBufferSize(50100);
    requireTrue(aggSize == 20 * 24, "agg buffer = 20 candles");

    std::printf("  Zoom out Agg2x PASS\n");

    // Test 4: Feed 10 more candles → agg buffer updated
    std::vector<Candle6> candles10;
    for (int i = 40; i < 50; ++i) {
      candles10.push_back({
        static_cast<float>(i), 100.0f, 110.0f, 90.0f, 105.0f, 0.4f
      });
    }

    OneShotSource src3;
    src3.setBatch(makeBatch(100, candles10));
    src3.start();
    auto fr3 = session.update(src3);

    requireTrue(fr3.dataChanged, "new data arrived");
    aggSize = ingest.getBufferSize(50100);
    requireTrue(aggSize == 25 * 24, "agg buffer = 25 candles (50/2)");

    std::printf("  Feed more data PASS\n");

    // Test 5: Zoom back in → Raw
    vp.setDataRange(0.0, 40.0, 80.0, 120.0); // ppdu = 20

    OneShotSource emptySrc2;
    emptySrc2.start();
    auto fr4 = session.update(emptySrc2);

    requireTrue(fr4.resolutionChanged, "resolution changed on zoom in");
    g = scene.getGeometry(101);
    requireTrue(g->vertexBufferId == 100, "geometry → raw buffer (zoomed back in)");

    std::printf("  Zoom back in PASS\n");
  }

  // --- Test 6: Smart retention ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    auto ok = [](const dc::CmdResult& r, const char* ctx) {
      if (!r.ok) {
        std::fprintf(stderr, "CMD FAIL [%s]: %s %s\n", ctx,
                     r.err.code.c_str(), r.err.message.c_str());
        std::exit(1);
      }
    };

    ok(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    ok(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "layer");

    dc::ChartSessionConfig cfg;
    cfg.enableSmartRetention = true;
    cfg.smartRetention.retentionMultiplier = 3.0f;
    cfg.smartRetention.minRetention = 64 * 1024;
    cfg.smartRetention.maxRetention = 8 * 1024 * 1024;

    dc::ChartSession session(cp, ingest);
    session.setConfig(cfg);

    dc::Viewport vp;
    vp.setPixelViewport(800, 600);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 20.0, 80.0, 120.0); // visibleDataWidth = 20
    session.setViewport(&vp);

    auto hCandle = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "c", false}));

    // Trigger smart retention via update
    OneShotSource src;
    src.start();
    session.update(src);

    // Expected: 20 × 24 × 3 = 1440. Clamped to min 64KB.
    std::uint32_t maxB = ingest.getMaxBytes(100);
    requireTrue(maxB == 64 * 1024, "smart retention clamped to minRetention (64KB)");

    // Zoom way out: visibleDataWidth = 100000
    vp.setDataRange(0.0, 100000.0, 80.0, 120.0);
    OneShotSource src2;
    src2.start();
    session.update(src2);

    // Expected: 100000 × 24 × 3 = 7,200,000. Under max 8MB.
    maxB = ingest.getMaxBytes(100);
    requireTrue(maxB == 7200000, "smart retention = 7.2M");

    // Extreme zoom out: visibleDataWidth = 1000000
    vp.setDataRange(0.0, 1000000.0, 80.0, 120.0);
    OneShotSource src3;
    src3.start();
    session.update(src3);

    maxB = ingest.getMaxBytes(100);
    requireTrue(maxB == 8 * 1024 * 1024, "smart retention clamped to maxRetention (8MB)");

    std::printf("  Smart retention PASS\n");
  }

  std::printf("\nD8.5 session aggregation PASS\n");
  return 0;
}
