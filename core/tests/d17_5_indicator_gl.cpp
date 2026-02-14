// D17.5 — Indicator GL integration test (OSMesa)
// Renders RSI + Stochastic recipes in a headless OSMesa context.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/recipe/RSIRecipe.hpp"
#include "dc/recipe/StochasticRecipe.hpp"
#include "dc/math/Indicators.hpp"
#include "dc/viewport/Viewport.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
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

int main() {
  constexpr int W = 200;
  constexpr int H = 200;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Create pane + layer
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Indicators"})"), "createLayer");

  // Shared transform for viewport mapping
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");

  // --- Generate synthetic OHLC data ---
  constexpr int N = 40;
  float closes[N], highs[N], lows[N], xCoords[N];
  float price = 100.0f;
  std::uint32_t seed = 123;
  auto rng = [&]() -> float {
    seed = seed * 1103515245u + 12345u;
    return static_cast<float>((seed >> 16) & 0x7FFF) / 32767.0f;
  };

  for (int i = 0; i < N; i++) {
    float open = price;
    float delta = (rng() - 0.5f) * 4.0f;
    price += delta;
    float close = price;
    float high = (open > close ? open : close) + rng() * 2.0f;
    float low  = (open < close ? open : close) - rng() * 2.0f;

    closes[i] = close;
    highs[i] = high;
    lows[i] = low;
    xCoords[i] = static_cast<float>(i);
  }

  // --- RSI Recipe ---
  dc::RSIRecipeConfig rsiCfg;
  rsiCfg.paneId = 1;
  rsiCfg.layerId = 10;
  rsiCfg.name = "RSI";
  rsiCfg.showRefLines = true;

  dc::RSIRecipe rsiRecipe(100, rsiCfg);
  auto rsiBuild = rsiRecipe.build();
  for (auto& cmd : rsiBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "rsi create");

  // Attach transform to RSI drawItems
  for (dc::Id diId : rsiRecipe.drawItemIds()) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(diId) +
      R"(,"transformId":50})"), "attach rsi xform");
  }

  // Compute RSI data
  auto rsiData = rsiRecipe.computeRSI(closes, N, xCoords);
  std::printf("  RSI segments: %u\n", rsiData.segmentCount);
  requireTrue(rsiData.segmentCount > 0, "RSI has segments");

  // Compute ref lines
  auto rsiRef = rsiRecipe.computeRefLines(0.0f, static_cast<float>(N - 1));

  // Upload RSI line data
  ingest.ensureBuffer(rsiRecipe.lineBufferId());
  ingest.setBufferData(rsiRecipe.lineBufferId(),
    reinterpret_cast<const std::uint8_t*>(rsiData.lineSegments.data()),
    static_cast<std::uint32_t>(rsiData.lineSegments.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(rsiRecipe.lineGeomId()).c_str(), rsiData.segmentCount);
    requireOk(cp.applyJsonText(buf), "setRsiLineVC");
  }

  // Upload RSI ref line data
  ingest.ensureBuffer(rsiRecipe.refBufferId());
  ingest.setBufferData(rsiRecipe.refBufferId(),
    reinterpret_cast<const std::uint8_t*>(rsiRef.lineSegments.data()),
    static_cast<std::uint32_t>(rsiRef.lineSegments.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(rsiRecipe.refGeomId()).c_str(), rsiRef.segmentCount);
    requireOk(cp.applyJsonText(buf), "setRsiRefVC");
  }

  // --- Stochastic Recipe ---
  dc::StochasticRecipeConfig stochCfg;
  stochCfg.paneId = 1;
  stochCfg.layerId = 10;
  stochCfg.name = "Stoch";
  stochCfg.showRefLines = true;

  dc::StochasticRecipe stochRecipe(200, stochCfg);
  auto stochBuild = stochRecipe.build();
  for (auto& cmd : stochBuild.createCommands)
    requireOk(cp.applyJsonText(cmd), "stoch create");

  // Attach transform to Stochastic drawItems
  for (dc::Id diId : stochRecipe.drawItemIds()) {
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":)" + std::to_string(diId) +
      R"(,"transformId":50})"), "attach stoch xform");
  }

  // Compute Stochastic data
  auto stochData = stochRecipe.computeStochastic(highs, lows, closes, N, xCoords);
  std::printf("  Stoch %%K segments: %u, %%D segments: %u\n",
              stochData.kCount, stochData.dCount);
  requireTrue(stochData.kCount > 0, "Stoch has %K segments");
  requireTrue(stochData.dCount > 0, "Stoch has %D segments");

  // Compute stochastic ref lines
  auto stochRef = stochRecipe.computeRefLines(0.0f, static_cast<float>(N - 1));

  // Upload %K data
  ingest.ensureBuffer(stochRecipe.kBufferId());
  ingest.setBufferData(stochRecipe.kBufferId(),
    reinterpret_cast<const std::uint8_t*>(stochData.kSegments.data()),
    static_cast<std::uint32_t>(stochData.kSegments.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(stochRecipe.kGeomId()).c_str(), stochData.kCount);
    requireOk(cp.applyJsonText(buf), "setStochKVC");
  }

  // Upload %D data
  ingest.ensureBuffer(stochRecipe.dBufferId());
  ingest.setBufferData(stochRecipe.dBufferId(),
    reinterpret_cast<const std::uint8_t*>(stochData.dSegments.data()),
    static_cast<std::uint32_t>(stochData.dSegments.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(stochRecipe.dGeomId()).c_str(), stochData.dCount);
    requireOk(cp.applyJsonText(buf), "setStochDVC");
  }

  // Upload stochastic ref line data
  ingest.ensureBuffer(stochRecipe.refBufferId());
  ingest.setBufferData(stochRecipe.refBufferId(),
    reinterpret_cast<const std::uint8_t*>(stochRef.lineSegments.data()),
    static_cast<std::uint32_t>(stochRef.lineSegments.size() * sizeof(float)));
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setGeometryVertexCount","geometryId":%s,"vertexCount":%u})",
      std::to_string(stochRecipe.refGeomId()).c_str(), stochRef.segmentCount);
    requireOk(cp.applyJsonText(buf), "setStochRefVC");
  }

  // --- Set viewport transform ---
  // X range: [0, N-1] data indices, Y range: [0, 100] indicator values
  dc::Viewport vp;
  vp.setDataRange(0.0f, static_cast<float>(N - 1), 0.0f, 100.0f);
  vp.setPixelViewport(W, H);
  auto tp = vp.computeTransformParams();
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":%.9g,"tx":%.9g,"ty":%.9g})",
      static_cast<double>(tp.sx), static_cast<double>(tp.sy),
      static_cast<double>(tp.tx), static_cast<double>(tp.ty));
    requireOk(cp.applyJsonText(buf), "setTransform");
  }

  // --- Render ---
  dc::GpuBufferManager gpuBufs;

  auto syncBuf = [&](dc::Id bufId) {
    const auto* data = ingest.getBufferData(bufId);
    auto size = ingest.getBufferSize(bufId);
    if (data && size > 0) {
      gpuBufs.setCpuData(bufId, data, size);
    }
  };

  // Sync all buffers
  syncBuf(rsiRecipe.lineBufferId());
  syncBuf(rsiRecipe.refBufferId());
  syncBuf(stochRecipe.kBufferId());
  syncBuf(stochRecipe.dBufferId());
  syncBuf(stochRecipe.refBufferId());

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  gpuBufs.uploadDirty();

  auto stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  std::printf("  draw calls: %u, culled: %u\n", stats.drawCalls, stats.culledDrawCalls);

  // Expect at least 5 draw calls: RSI line, RSI ref, %K, %D, stoch ref
  requireTrue(stats.drawCalls >= 5, "at least 5 draw calls");

  // Verify pixels: should have non-black content
  auto pixels = ctx.readPixels();
  int nonBlack = 0;
  for (int i = 0; i < W * H; i++) {
    std::size_t idx = static_cast<std::size_t>(i * 4);
    if (pixels[idx] > 10 || pixels[idx + 1] > 10 || pixels[idx + 2] > 10)
      nonBlack++;
  }
  requireTrue(nonBlack > 50, "significant non-black content");
  std::printf("  non-black pixels: %d\n", nonBlack);

  std::printf("D17.5 indicator_gl: ALL PASS\n");
  return 0;
}
