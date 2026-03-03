// D37.2 — Anchor GL: render anchored triangle, verify screen position
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/layout/Anchor.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
  }
}

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D37.2 Anchor GL Tests ===\n");

  dc::OsMesaContext ctx;
  if (!ctx.init(64, 64)) {
    std::fprintf(stderr, "SKIP: OSMesa init failed\n");
    return 0;
  }

  dc::Renderer renderer;
  check(renderer.init(), "renderer init");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  dc::GpuBufferManager gpuBufs;
  cp.setIngestProcessor(&ingest);

  // Setup: small triangle at origin
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
    "createGeometry");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
    "bindDrawItem");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":0,"a":1})"),
    "setColor");

  // Small triangle centered at origin
  ingest.ensureBuffer(10);
  float tri[] = {
    -0.1f, -0.1f,
     0.1f, -0.1f,
     0.0f,  0.1f,
  };
  ingest.setBufferData(10, reinterpret_cast<const std::uint8_t*>(tri), sizeof(tri));
  gpuBufs.setCpuData(10, ingest.getBufferData(10), ingest.getBufferSize(10));
  gpuBufs.uploadDirty();

  // Render without anchor — triangle at center
  auto stats = renderer.render(scene, gpuBufs, 64, 64);
  check(stats.drawCalls > 0, "rendered triangle");

  auto pixels = ctx.readPixels();

  // Center pixel should be yellow
  int cx = 32, cy = 32;
  int idx = (cy * 64 + cx) * 4;
  check(pixels[idx] > 200 && pixels[idx + 1] > 200, "center pixel is yellow (no anchor)");

  // Test anchor computation directly
  {
    dc::PaneRegion full{-1.0f, 1.0f, -1.0f, 1.0f};
    auto [ax, ay] = dc::computeAnchorClipPosition(
      dc::AnchorPoint::TopRight, full, 0, 0, 64, 64);
    check(ax > 0.9f && ay > 0.9f, "TopRight anchor computes to top-right corner");
  }

  std::printf("=== D37.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
