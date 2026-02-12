// D8.4 — AggregationManager + setGeometryBuffer test (pure C++, no GL)
// Tests: tier switching, geometry rebinding, agg buffer recomputation.

#include "dc/data/AggregationManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static void appendCandles(dc::IngestProcessor& ingest, dc::Id bufferId,
                           const std::vector<Candle6>& candles) {
  // Build binary ingest batch: [1B op=1][4B bufferId][4B offset=0][4B payloadLen][payload]
  std::uint32_t payloadLen = static_cast<std::uint32_t>(candles.size() * sizeof(Candle6));
  std::vector<std::uint8_t> batch(13 + payloadLen);

  batch[0] = 1; // OP_APPEND
  std::uint32_t bid = static_cast<std::uint32_t>(bufferId);
  std::memcpy(&batch[1], &bid, 4);
  std::uint32_t zero = 0;
  std::memcpy(&batch[5], &zero, 4);
  std::memcpy(&batch[9], &payloadLen, 4);
  std::memcpy(&batch[13], candles.data(), payloadLen);

  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

int main() {
  // Setup: scene with pane, layer, buffer, geometry, drawItem for candles
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  auto ok = [](const dc::CmdResult& r, const char* ctx) {
    if (!r.ok) {
      std::fprintf(stderr, "FAIL [%s]: %s %s\n", ctx, r.err.code.c_str(), r.err.message.c_str());
      std::exit(1);
    }
  };

  ok(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  ok(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "layer");
  ok(cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})"), "raw buf");
  ok(cp.applyJsonText(R"({"cmd":"createBuffer","id":50100,"byteLength":0})"), "agg buf");
  ok(cp.applyJsonText(R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"candle6","vertexCount":1})"), "geom");
  ok(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10,"name":"candle"})"), "di");
  ok(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":102,"pipeline":"instancedCandle@1","geometryId":101})"), "bind");

  ingest.ensureBuffer(100);
  ingest.ensureBuffer(50100);

  // Create 20 raw candles
  std::vector<Candle6> candles20;
  for (int i = 0; i < 20; ++i) {
    candles20.push_back({
      static_cast<float>(i), 100.0f + i, 105.0f + i,
      95.0f + i, 102.0f + i, 0.4f
    });
  }
  appendCandles(ingest, 100, candles20);
  ingest.syncBufferLengths(scene);

  requireTrue(ingest.getBufferSize(100) == 20 * 24, "raw buffer has 20 candles");

  // --- Test 1: At Raw tier, onRawDataChanged is no-op ---
  {
    dc::AggregationManager mgr;
    dc::AggregationManagerConfig cfg;
    mgr.setConfig(cfg);
    mgr.addBinding({100, 50100, 101});

    auto modified = mgr.onRawDataChanged({100}, ingest);
    requireTrue(modified.empty(), "Raw tier: onRawDataChanged is no-op");
    requireTrue(mgr.currentTier() == dc::ResolutionTier::Raw, "starts at Raw");

    std::printf("  Raw no-op PASS\n");

    // --- Test 2: onViewportChanged(ppdu=4) → Agg2x ---
    auto modVp = mgr.onViewportChanged(4.0, ingest, cp);
    requireTrue(mgr.currentTier() == dc::ResolutionTier::Agg2x, "ppdu=4 → Agg2x");
    requireTrue(!modVp.empty(), "agg buffers modified");

    // Verify agg buffer has 10 candles (20/2)
    std::uint32_t aggSize = ingest.getBufferSize(50100);
    requireTrue(aggSize == 10 * 24, "agg buffer has 10 candles");

    // Verify geometry rebound to agg buffer
    const dc::Geometry* g = scene.getGeometry(101);
    requireTrue(g != nullptr, "geometry exists");
    requireTrue(g->vertexBufferId == 50100, "geometry → agg buffer");
    requireTrue(g->vertexCount == 10, "geometry vertexCount = 10");

    std::printf("  Agg2x switch PASS\n");

    // --- Test 3: Add 4 more raw candles, onRawDataChanged recomputes ---
    std::vector<Candle6> candles4;
    for (int i = 20; i < 24; ++i) {
      candles4.push_back({
        static_cast<float>(i), 100.0f, 110.0f, 90.0f, 105.0f, 0.4f
      });
    }
    appendCandles(ingest, 100, candles4);
    ingest.syncBufferLengths(scene);

    requireTrue(ingest.getBufferSize(100) == 24 * 24, "raw buffer now 24 candles");

    auto modRaw = mgr.onRawDataChanged({100}, ingest);
    requireTrue(!modRaw.empty(), "agg buffer recomputed");

    aggSize = ingest.getBufferSize(50100);
    requireTrue(aggSize == 12 * 24, "agg buffer has 12 candles (24/2)");

    std::printf("  Recompute PASS\n");

    // --- Test 4: Zoom in → Raw ---
    auto modIn = mgr.onViewportChanged(20.0, ingest, cp);
    requireTrue(mgr.currentTier() == dc::ResolutionTier::Raw, "ppdu=20 → Raw");

    const dc::Geometry* g2 = scene.getGeometry(101);
    requireTrue(g2->vertexBufferId == 100, "geometry → raw buffer");

    std::printf("  Back to Raw PASS\n");
  }

  std::printf("\nD8.4 aggregation PASS\n");
  return 0;
}
