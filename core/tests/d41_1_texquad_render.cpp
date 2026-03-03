// D41.1 — Render a solid red texture on a texturedQuad@1, verify red pixels
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/TextureManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
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
  std::printf("=== D41.1 Textured Quad Render ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("OSMesa not available — skipping\n");
    return 0;
  }

  // Create a 2x2 solid red texture (RGBA)
  dc::TextureManager texMgr;
  std::uint8_t redPixels[2 * 2 * 4];
  for (int i = 0; i < 4; i++) {
    redPixels[i * 4 + 0] = 255;  // R
    redPixels[i * 4 + 1] = 0;    // G
    redPixels[i * 4 + 2] = 0;    // B
    redPixels[i * 4 + 3] = 255;  // A
  }
  dc::TextureId texId = texMgr.load(redPixels, 2, 2);
  check(texId > 0, "texture loaded");

  // Set up scene via CommandProcessor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":3,"byteLength":0})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":4,"vertexBufferId":3,"vertexCount":1,"format":"pos2_uv4"})"),
    "createGeometry");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "createDrawItem");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"texturedQuad@1","geometryId":4})"),
    "bindDrawItem");

  // Set drawItem textureId
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemTexture","drawItemId":5,"textureId":%u})",
      static_cast<unsigned>(texId));
    requireOk(cp.applyJsonText(buf), "setDrawItemTexture");
  }

  // Set drawItem color to white so texture shows through unmodified
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":5,"color":[1.0,1.0,1.0,1.0]})"),
    "setDrawItemColor");

  // Upload vertex data: full-screen quad (x0,y0, x1,y1) = (-1,-1, 1,1)
  // pos2_uv4 format: 4 floats per instance
  float verts[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  ingest.ensureBuffer(3);
  ingest.setBufferData(3, reinterpret_cast<const uint8_t*>(verts), sizeof(verts));

  // Sync to GPU
  dc::GpuBufferManager gpuBufs;
  const auto* cpuData = ingest.getBufferData(3);
  auto cpuSize = ingest.getBufferSize(3);
  if (cpuData && cpuSize > 0) {
    gpuBufs.setCpuData(3, cpuData, cpuSize);
  }
  gpuBufs.uploadDirty();

  // Init renderer with texture manager
  dc::Renderer renderer;
  check(renderer.init(), "renderer init");
  renderer.setTextureManager(&texMgr);

  // Render
  auto stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  check(stats.drawCalls >= 1, "at least 1 draw call");

  // Read pixels and verify center is red
  auto pixels = ctx.readPixels();
  check(!pixels.empty(), "readPixels non-empty");

  // Check center pixel (32, 32) — note: readPixels is bottom-up in OSMesa
  int cx = W / 2;
  int cy = H / 2;
  std::size_t idx = static_cast<std::size_t>((cy * W + cx) * 4);
  std::uint8_t r = pixels[idx + 0];
  std::uint8_t g = pixels[idx + 1];
  std::uint8_t b = pixels[idx + 2];
  std::printf("  center pixel: R=%u G=%u B=%u\n", r, g, b);
  check(r > 200, "center R > 200");
  check(g < 50, "center G < 50");
  check(b < 50, "center B < 50");

  // Also check a corner pixel to make sure the quad covers the whole viewport
  std::size_t cornerIdx = static_cast<std::size_t>((1 * W + 1) * 4);
  std::uint8_t cr = pixels[cornerIdx + 0];
  std::uint8_t cg = pixels[cornerIdx + 1];
  std::uint8_t cb = pixels[cornerIdx + 2];
  std::printf("  corner pixel: R=%u G=%u B=%u\n", cr, cg, cb);
  check(cr > 200, "corner R > 200");
  check(cg < 50, "corner G < 50");
  check(cb < 50, "corner B < 50");

  std::printf("=== D41.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
