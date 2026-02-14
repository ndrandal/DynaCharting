// D18.1 — PPM export test

#include "dc/export/ChartSnapshot.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  constexpr int W = 100, H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("D18.1 export: SKIPPED (no OSMesa)\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Simple red triangle
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"pos2_clip","vertexCount":3})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":102,"pipeline":"triSolid@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":102,"r":1,"g":0,"b":0,"a":1})");

  float verts[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
  ingest.ensureBuffer(100);
  ingest.setBufferData(100, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(100, ingest.getBufferData(100), ingest.getBufferSize(100));

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  gpuBufs.uploadDirty();
  renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  auto pixels = ctx.readPixels();

  // Write PPM (not flipped)
  std::string path = "d18_test_output.ppm";
  requireTrue(dc::writePPM(path, pixels.data(), W, H), "writePPM");
  std::printf("  PPM written to %s\n", path.c_str());

  // Write flipped PPM
  std::string pathFlipped = "d18_test_output_flipped.ppm";
  requireTrue(dc::writePPMFlipped(pathFlipped, pixels.data(), W, H), "writePPMFlipped");
  std::printf("  Flipped PPM written to %s\n", pathFlipped.c_str());

  // Verify file exists and has content
  FILE* f = std::fopen(path.c_str(), "rb");
  requireTrue(f != nullptr, "PPM file exists");
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fclose(f);
  requireTrue(size > 100, "PPM file has content");
  std::printf("  PPM file size: %ld bytes\n", size);

  // Clean up test files
  std::remove(path.c_str());
  std::remove(pathFlipped.c_str());

  std::printf("D18.1 export: ALL PASS\n");
  return 0;
}
