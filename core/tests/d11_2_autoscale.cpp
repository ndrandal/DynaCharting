// D11.2 — Auto-Scale Y test
// Tests: candle data visible range → tight Y bounds + margin,
// viewport with no visible data → returns false,
// includeZero → Y range extends to 0.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/viewport/AutoScale.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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
  // --- Test 1: Candle data, viewport X=[2,7] → Y bounds from candles 2-7 + margin ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"candle6","vertexCount":10})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"instancedCandle@1","geometryId":30})"),
      "bindDI");

    // 10 candles at x=0..9, prices in range 95-105
    float candles[60]; // 10 * 6 floats
    for (int i = 0; i < 10; i++) {
      float cx = static_cast<float>(i);
      float open = 98.0f + static_cast<float>(i % 3);
      float close = open + 1.0f;
      float high = 100.0f + static_cast<float>(i);   // 100..109
      float low = 95.0f + static_cast<float>(i % 5);  // 95..99
      float hw = 0.3f;
      candles[i * 6 + 0] = cx;
      candles[i * 6 + 1] = open;
      candles[i * 6 + 2] = high;
      candles[i * 6 + 3] = low;
      candles[i * 6 + 4] = close;
      candles[i * 6 + 5] = hw;
    }
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

    dc::Viewport vp;
    vp.setDataRange(2, 7, 0, 200); // only candles 2-7 visible
    vp.setPixelViewport(400, 300);

    dc::AutoScale as;
    dc::AutoScaleConfig cfg;
    cfg.marginFraction = 0.05f;
    as.setConfig(cfg);

    double yMin, yMax;
    bool ok = as.computeYRange({40}, scene, ingest, vp, yMin, yMax);
    requireTrue(ok, "computeYRange returns true");

    // Find expected low/high for candles 2-7
    float eLow = 1e30f, eHigh = -1e30f;
    for (int i = 2; i <= 7; i++) {
      float h = candles[i * 6 + 2];
      float l = candles[i * 6 + 3];
      if (l < eLow) eLow = l;
      if (h > eHigh) eHigh = h;
    }
    double span = eHigh - eLow;
    double margin = span * 0.05;

    std::printf("  expected low=%.1f high=%.1f, got yMin=%.2f yMax=%.2f\n",
                eLow, eHigh, yMin, yMax);
    requireTrue(std::fabs(yMin - (eLow - margin)) < 0.1, "yMin ≈ eLow - margin");
    requireTrue(std::fabs(yMax - (eHigh + margin)) < 0.1, "yMax ≈ eHigh + margin");

    std::printf("  Test 1 (candle Y range + margin) PASS\n");
  }

  // --- Test 2: Viewport X=[100,200] (no visible data) → returns false ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"candle6","vertexCount":2})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"instancedCandle@1","geometryId":30})"),
      "bindDI");

    float candles[] = {
      5.0f, 100.0f, 105.0f, 95.0f, 102.0f, 0.3f,
      6.0f, 101.0f, 106.0f, 96.0f, 103.0f, 0.3f,
    };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(candles), sizeof(candles));

    dc::Viewport vp;
    vp.setDataRange(100, 200, 0, 200);
    vp.setPixelViewport(400, 300);

    dc::AutoScale as;
    double yMin, yMax;
    bool ok = as.computeYRange({40}, scene, ingest, vp, yMin, yMax);
    requireTrue(!ok, "no visible data → returns false");

    std::printf("  Test 2 (no visible data → false) PASS\n");
  }

  // --- Test 3: includeZero → Y range extends to 0 ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":0})"), "createBuffer");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":2})"),
      "createGeometry");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"line2d@1","geometryId":30})"),
      "bindDI");

    // Two points: (5, 50) and (10, 60) — all positive
    float verts[] = { 5.0f, 50.0f, 10.0f, 60.0f };
    ingest.ensureBuffer(20);
    ingest.setBufferData(20, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

    dc::Viewport vp;
    vp.setDataRange(0, 20, 0, 100);
    vp.setPixelViewport(400, 300);

    dc::AutoScale as;
    dc::AutoScaleConfig cfg;
    cfg.marginFraction = 0.05f;
    cfg.includeZero = true;
    as.setConfig(cfg);

    double yMin, yMax;
    bool ok = as.computeYRange({40}, scene, ingest, vp, yMin, yMax);
    requireTrue(ok, "includeZero returns true");
    requireTrue(yMin <= 0.0, "includeZero: yMin <= 0");
    requireTrue(yMax > 60.0, "includeZero: yMax > 60");

    std::printf("  Test 3 (includeZero → extends to 0) PASS\n");
  }

  std::printf("D11.2 autoscale: ALL PASS\n");
  return 0;
}
