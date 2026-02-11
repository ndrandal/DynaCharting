#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

// Helper: build a binary ingest record
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
  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  // 1. Build scene via CommandProcessor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})"),
    "createGeometry");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Tri"})"), "di");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":4})"), "bind");

  // 2. Build binary batch with triangle vertex data via IngestProcessor
  float verts[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
  };
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1 /*OP_APPEND*/, 3, 0, verts, sizeof(verts));

  dc::IngestProcessor ingest;
  dc::IngestResult result = ingest.processBatch(batch.data(),
                                                 static_cast<std::uint32_t>(batch.size()));

  std::printf("processBatch: touchedBuffers=%zu, payloadBytes=%u, droppedBytes=%u\n",
              result.touchedBufferIds.size(), result.payloadBytes, result.droppedBytes);
  requireTrue(result.touchedBufferIds.size() == 1, "1 touched buffer");
  requireTrue(result.payloadBytes == sizeof(verts), "correct payload bytes");
  requireTrue(ingest.getBufferSize(3) == sizeof(verts), "buffer size matches");

  // 3. Upload to GPU and render
  ingest.syncBufferLengths(scene);

  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(3, ingest.getBufferData(3), ingest.getBufferSize(3));
  gpuBufs.uploadDirty();

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  auto pixels = ctx.readPixels();
  auto px = [&](int x, int y) -> const std::uint8_t* {
    return &pixels[static_cast<std::size_t>((y * W + x) * 4)];
  };

  requireTrue(stats.drawCalls == 1, "drawCalls==1");
  const auto* center = px(W/2, H/2);
  std::printf("Center: R=%u G=%u B=%u\n", center[0], center[1], center[2]);
  requireTrue(center[0] > 200, "center is red");

  // 4. updateRange: modify first vertex to move it
  float newVert[] = { -0.9f, -0.9f };
  batch.clear();
  appendRecord(batch, 2 /*OP_UPDATE_RANGE*/, 3, 0, newVert, sizeof(newVert));

  result = ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  requireTrue(result.payloadBytes == sizeof(newVert), "updateRange payload");

  // Re-upload and re-render
  gpuBufs.setCpuData(3, ingest.getBufferData(3), ingest.getBufferSize(3));
  gpuBufs.uploadDirty();

  stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  pixels = ctx.readPixels();

  // Verify updateRange modified buffer data correctly
  const auto* raw = ingest.getBufferData(3);
  float readBack[2];
  std::memcpy(readBack, raw, sizeof(readBack));
  std::printf("Updated v0: %.1f, %.1f\n", readBack[0], readBack[1]);
  requireTrue(readBack[0] < -0.8f, "v0.x updated to -0.9");
  requireTrue(readBack[1] < -0.8f, "v0.y updated to -0.9");

  // Triangle centroid after update ≈ (-0.13, -0.3) → pixel (28, 22)
  const auto* centroid = px(28, 22);
  std::printf("Centroid after update: R=%u G=%u B=%u\n", centroid[0], centroid[1], centroid[2]);
  requireTrue(centroid[0] > 200, "centroid red after updateRange");

  std::printf("\nD2.4 ingest PASS\n");
  return 0;
}
