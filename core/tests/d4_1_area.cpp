// D4.1 — AreaRecipe test
// GL test: render filled area, verify non-black pixels in expected region.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/recipe/AreaRecipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

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

int main() {
  constexpr int W = 200, H = 200;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) { std::fprintf(stderr, "OSMesa init failed\n"); return 1; }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"L"})"), "layer");

  // Build AreaRecipe
  dc::AreaRecipeConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 2;
  cfg.name = "TestArea";
  cfg.createTransform = false;
  dc::AreaRecipe area(100, cfg);

  auto result = area.build();
  for (auto& cmd : result.createCommands) {
    requireOk(cp.applyJsonText(cmd), "area create");
  }

  requireTrue(scene.hasBuffer(100), "buffer 100 exists");
  requireTrue(scene.hasDrawItem(102), "drawItem 102 exists");

  // Compute area data: simple triangle shape
  float xData[] = {-0.8f, 0.0f, 0.8f};
  float yData[] = {-0.5f, 0.5f, -0.5f};
  auto areaData = area.compute(xData, yData, 3, -0.8f);
  requireTrue(areaData.vertexCount == 12, "2 segments × 6 verts = 12");
  requireTrue(areaData.triVerts.size() == 24, "12 verts × 2 floats = 24");

  // Set draw item color to green
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":102,"r":0.0,"g":0.8,"b":0.0,"a":1.0})"),
    "setColor");

  // Ingest data
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 100, 0, areaData.triVerts.data(),
               static_cast<std::uint32_t>(areaData.triVerts.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":)" +
    std::to_string(areaData.vertexCount) + "}"), "setVC");

  // Render
  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(100, ingest.getBufferData(100), ingest.getBufferSize(100));

  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats.drawCalls == 1, "1 draw call");

  // Read pixels and check for non-black in center region
  auto pixels = ctx.readPixels();
  int greenPixels = 0;
  int cx = W / 2, cy = H / 2;
  for (int dy = -20; dy <= 20; dy++) {
    for (int dx = -20; dx <= 20; dx++) {
      int x = cx + dx, y = cy + dy;
      if (x < 0 || x >= W || y < 0 || y >= H) continue;
      std::size_t idx = static_cast<std::size_t>((y * W + x) * 4);
      if (pixels[idx + 1] > 100) greenPixels++;
    }
  }
  std::printf("Green pixels in center: %d\n", greenPixels);
  requireTrue(greenPixels > 50, "non-black pixels in area region");

  // Dispose
  for (auto& cmd : result.disposeCommands) {
    requireOk(cp.applyJsonText(cmd), "area dispose");
  }
  requireTrue(!scene.hasDrawItem(102), "di disposed");

  std::printf("D4.1 area PASS\n");
  return 0;
}
