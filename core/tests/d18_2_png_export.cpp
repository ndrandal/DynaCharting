// D18.2 — PNG export test

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
#include <cstring>
#include <string>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  constexpr int W = 100, H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("D18.2 png_export: SKIPPED (no OSMesa)\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // --- Set up a red triangle (same as D18.1) ---
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

  // --- Test 1: writePNG produces a file ---
  std::printf("  Test 1: writePNG\n");
  std::string pngPath = "d18_2_test_output.png";
  requireTrue(dc::writePNG(pngPath, pixels.data(), W, H), "writePNG returned true");
  std::printf("    PNG written to %s\n", pngPath.c_str());

  // --- Test 2: Verify PNG signature (first 8 bytes) ---
  std::printf("  Test 2: PNG signature\n");
  {
    FILE* f = std::fopen(pngPath.c_str(), "rb");
    requireTrue(f != nullptr, "PNG file exists");

    std::uint8_t sig[8];
    std::size_t nread = std::fread(sig, 1, 8, f);
    requireTrue(nread == 8, "Read 8 signature bytes");

    const std::uint8_t expected[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    requireTrue(std::memcmp(sig, expected, 8) == 0, "PNG signature matches");
    std::printf("    Signature OK\n");

    // Check file size is reasonable
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fclose(f);
    requireTrue(size > 100, "PNG file has content (> 100 bytes)");
    std::printf("    PNG file size: %ld bytes\n", size);
  }

  // --- Test 3: writePNGFlipped produces a file with correct signature ---
  std::printf("  Test 3: writePNGFlipped\n");
  std::string pngFlipPath = "d18_2_test_output_flipped.png";
  requireTrue(dc::writePNGFlipped(pngFlipPath, pixels.data(), W, H), "writePNGFlipped returned true");
  std::printf("    Flipped PNG written to %s\n", pngFlipPath.c_str());

  {
    FILE* f = std::fopen(pngFlipPath.c_str(), "rb");
    requireTrue(f != nullptr, "Flipped PNG file exists");

    std::uint8_t sig[8];
    std::size_t nread = std::fread(sig, 1, 8, f);
    requireTrue(nread == 8, "Read 8 signature bytes (flipped)");

    const std::uint8_t expected[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    requireTrue(std::memcmp(sig, expected, 8) == 0, "Flipped PNG signature matches");

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fclose(f);
    requireTrue(size > 100, "Flipped PNG file has content");
    std::printf("    Flipped PNG file size: %ld bytes\n", size);
  }

  // --- Test 4: Verify IHDR chunk follows signature ---
  std::printf("  Test 4: IHDR chunk\n");
  {
    FILE* f = std::fopen(pngPath.c_str(), "rb");
    requireTrue(f != nullptr, "PNG file open for IHDR check");

    // Skip 8-byte signature
    std::fseek(f, 8, SEEK_SET);

    // Read chunk length (4B BE) + type (4B)
    std::uint8_t header[8];
    std::size_t nread = std::fread(header, 1, 8, f);
    requireTrue(nread == 8, "Read IHDR chunk header");

    // Chunk length should be 13
    std::uint32_t chunkLen = (static_cast<std::uint32_t>(header[0]) << 24) |
                              (static_cast<std::uint32_t>(header[1]) << 16) |
                              (static_cast<std::uint32_t>(header[2]) << 8) |
                              static_cast<std::uint32_t>(header[3]);
    requireTrue(chunkLen == 13, "IHDR data length is 13");

    // Type should be "IHDR"
    requireTrue(header[4] == 'I' && header[5] == 'H' &&
                header[6] == 'D' && header[7] == 'R', "Chunk type is IHDR");

    // Read IHDR data (13 bytes)
    std::uint8_t ihdr[13];
    nread = std::fread(ihdr, 1, 13, f);
    requireTrue(nread == 13, "Read IHDR data");

    // Width (4B BE)
    std::uint32_t w = (static_cast<std::uint32_t>(ihdr[0]) << 24) |
                      (static_cast<std::uint32_t>(ihdr[1]) << 16) |
                      (static_cast<std::uint32_t>(ihdr[2]) << 8) |
                      static_cast<std::uint32_t>(ihdr[3]);
    requireTrue(w == static_cast<std::uint32_t>(W), "IHDR width matches");

    // Height (4B BE)
    std::uint32_t h = (static_cast<std::uint32_t>(ihdr[4]) << 24) |
                      (static_cast<std::uint32_t>(ihdr[5]) << 16) |
                      (static_cast<std::uint32_t>(ihdr[6]) << 8) |
                      static_cast<std::uint32_t>(ihdr[7]);
    requireTrue(h == static_cast<std::uint32_t>(H), "IHDR height matches");

    // Bit depth = 8, color type = 2 (RGB)
    requireTrue(ihdr[8] == 8, "IHDR bit depth is 8");
    requireTrue(ihdr[9] == 2, "IHDR color type is 2 (RGB)");

    std::fclose(f);
    std::printf("    IHDR: %ux%u, depth=%u, colorType=%u OK\n", w, h, ihdr[8], ihdr[9]);
  }

  // --- Test 5: Normal and flipped PNGs differ ---
  std::printf("  Test 5: Normal vs flipped differ\n");
  {
    FILE* f1 = std::fopen(pngPath.c_str(), "rb");
    FILE* f2 = std::fopen(pngFlipPath.c_str(), "rb");
    requireTrue(f1 && f2, "Both PNG files open");

    std::fseek(f1, 0, SEEK_END);
    std::fseek(f2, 0, SEEK_END);
    long size1 = std::ftell(f1);
    long size2 = std::ftell(f2);

    // Both files should be the same size (same dimensions, stored blocks)
    requireTrue(size1 == size2, "Normal and flipped have same file size");

    // Read both files and compare — they should differ (the triangle is asymmetric)
    std::fseek(f1, 0, SEEK_SET);
    std::fseek(f2, 0, SEEK_SET);
    std::vector<std::uint8_t> buf1(static_cast<std::size_t>(size1));
    std::vector<std::uint8_t> buf2(static_cast<std::size_t>(size2));
    std::fread(buf1.data(), 1, buf1.size(), f1);
    std::fread(buf2.data(), 1, buf2.size(), f2);
    std::fclose(f1);
    std::fclose(f2);

    bool identical = (buf1 == buf2);
    requireTrue(!identical, "Normal and flipped PNGs are NOT identical");
    std::printf("    Files differ as expected\n");
  }

  // --- Clean up test files ---
  std::remove(pngPath.c_str());
  std::remove(pngFlipPath.c_str());

  std::printf("D18.2 png_export: ALL PASS\n");
  return 0;
}
