// D41.2 — GPU pick on texturedQuad@1, verify DrawItem ID
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
  std::printf("=== D41.2 Textured Quad GPU Pick ===\n");

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
    redPixels[i * 4 + 0] = 255;
    redPixels[i * 4 + 1] = 0;
    redPixels[i * 4 + 2] = 0;
    redPixels[i * 4 + 3] = 255;
  }
  dc::TextureId texId = texMgr.load(redPixels, 2, 2);
  check(texId > 0, "texture loaded");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // -- Test 1: Pick a full-screen texturedQuad, verify drawItemId == 5 --
  {
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

    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":5,"color":[1.0,1.0,1.0,1.0]})"),
      "setDrawItemColor");

    // Upload full-screen quad vertex data
    float verts[] = {-1.0f, -1.0f, 1.0f, 1.0f};
    ingest.ensureBuffer(3);
    ingest.setBufferData(3, reinterpret_cast<const uint8_t*>(verts), sizeof(verts));

    dc::GpuBufferManager gpuBufs;
    const auto* cpuData = ingest.getBufferData(3);
    auto cpuSize = ingest.getBufferSize(3);
    if (cpuData && cpuSize > 0) {
      gpuBufs.setCpuData(3, cpuData, cpuSize);
    }
    gpuBufs.uploadDirty();

    dc::Renderer renderer;
    check(renderer.init(), "renderer init");
    renderer.setTextureManager(&texMgr);

    // Pick center — should return drawItemId = 5
    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    std::printf("  pick center: drawItemId = %u\n", static_cast<unsigned>(result.drawItemId));
    check(result.drawItemId == 5, "pick center: drawItemId == 5");

    // Pick corner — full-screen quad, should also return 5
    auto result2 = renderer.renderPick(scene, gpuBufs, W, H, 2, 2);
    std::printf("  pick corner: drawItemId = %u\n", static_cast<unsigned>(result2.drawItemId));
    check(result2.drawItemId == 5, "pick corner: drawItemId == 5 (full-screen)");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":1})"), "cleanup pane");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":3})"), "cleanup buffer");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":4})"), "cleanup geometry");
  }

  // -- Test 2: Pick a partial-screen texturedQuad, verify background outside --
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":10})"), "createPane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":10})"), "createLayer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":0})"), "createBuffer2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":13,"vertexBufferId":12,"vertexCount":1,"format":"pos2_uv4"})"),
      "createGeometry2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createDrawItem","id":14,"layerId":11})"), "createDrawItem2");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":14,"pipeline":"texturedQuad@1","geometryId":13})"),
      "bindDrawItem2");

    {
      char buf[128];
      std::snprintf(buf, sizeof(buf),
        R"({"cmd":"setDrawItemTexture","drawItemId":14,"textureId":%u})",
        static_cast<unsigned>(texId));
      requireOk(cp.applyJsonText(buf), "setDrawItemTexture2");
    }

    // Upload a small quad in the center (clip-space -0.5 to 0.5)
    float verts[] = {-0.5f, -0.5f, 0.5f, 0.5f};
    ingest.ensureBuffer(12);
    ingest.setBufferData(12, reinterpret_cast<const uint8_t*>(verts), sizeof(verts));

    dc::GpuBufferManager gpuBufs;
    const auto* cpuData = ingest.getBufferData(12);
    auto cpuSize = ingest.getBufferSize(12);
    if (cpuData && cpuSize > 0) {
      gpuBufs.setCpuData(12, cpuData, cpuSize);
    }
    gpuBufs.uploadDirty();

    dc::Renderer renderer;
    check(renderer.init(), "renderer init (test 2)");
    renderer.setTextureManager(&texMgr);

    // Pick center — should find the quad
    auto result = renderer.renderPick(scene, gpuBufs, W, H, W / 2, H / 2);
    std::printf("  pick center (partial): drawItemId = %u\n",
                static_cast<unsigned>(result.drawItemId));
    check(result.drawItemId == 14, "pick center: drawItemId == 14");

    // Pick corner — outside the quad, should be background
    auto result2 = renderer.renderPick(scene, gpuBufs, W, H, 2, 2);
    std::printf("  pick corner (partial): drawItemId = %u\n",
                static_cast<unsigned>(result2.drawItemId));
    check(result2.drawItemId == 0, "pick corner: background (drawItemId == 0)");

    // Clean up
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":10})"), "cleanup pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":12})"), "cleanup buffer2");
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":13})"), "cleanup geometry2");
  }

  std::printf("=== D41.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
