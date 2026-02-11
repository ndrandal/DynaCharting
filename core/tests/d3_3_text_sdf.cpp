// D3.3 — textSDF@1 pipeline test
// Renders a few characters via SDF text and verifies non-black pixels.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/OsMesaContext.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
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

static bool isNonBlack(const std::vector<std::uint8_t>& px, int w, int x, int y) {
  std::size_t idx = static_cast<std::size_t>((y * w + x) * 4);
  return px[idx] > 0 || px[idx+1] > 0 || px[idx+2] > 0;
}

int main() {
  constexpr int W = 128;
  constexpr int H = 64;

  // GL context
  dc::OsMesaContext ctx;
  requireTrue(ctx.init(W, H), "OSMesa init");

  // Load font + ensure glyphs
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  requireTrue(atlas.loadFontFile(FONT_PATH), "font load");
  atlas.ensureAscii();

  // Scene setup
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  cp.setGlyphAtlas(&atlas);

  auto ok = [](const dc::CmdResult& r, const char* c) {
    if (!r.ok) { std::fprintf(stderr, "FAIL [%s]: %s\n", c, r.err.message.c_str()); std::exit(1); }
  };

  ok(cp.applyJsonText(R"({"cmd":"createPane","name":"Test"})"), "pane");      // id=1
  ok(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"Text"})"), "layer"); // id=2
  ok(cp.applyJsonText(R"({"cmd":"createTransform"})"), "xform");               // id=3
  ok(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":0})"), "buf");     // id=4
  ok(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":4,"format":"glyph8","vertexCount":1})"),
    "geom");  // id=5
  ok(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"hello"})"), "di"); // id=6
  ok(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":6,"pipeline":"textSDF@1","geometryId":5})"), "bind");
  ok(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":6,"transformId":3})"), "xform");

  // Build glyph instance data for "Hi" centered on screen
  // Each instance: x0 y0 x1 y1 u0 v0 u1 v1 (8 floats = 32 bytes)
  const char* text = "Hi";
  float fontSize = 0.5f; // in clip-space units
  float cursorX = -0.4f;
  float baselineY = 0.0f;

  std::vector<float> instances;
  int glyphCount = 0;

  for (const char* p = text; *p; p++) {
    const dc::GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += (g->advance / 48.0f) * fontSize;
      continue;
    }

    float scale = fontSize / 48.0f;
    float x0 = cursorX + g->bearingX * scale;
    float y1 = baselineY + g->bearingY * scale;  // top of glyph
    float y0 = y1 - g->h * scale;                // bottom of glyph
    float x1 = x0 + g->w * scale;

    instances.push_back(x0);
    instances.push_back(y0);
    instances.push_back(x1);
    instances.push_back(y1);
    instances.push_back(g->u0);
    instances.push_back(g->v0);
    instances.push_back(g->u1);
    instances.push_back(g->v1);
    glyphCount++;

    cursorX += g->advance * scale;
  }

  std::printf("Built %d glyph instances\n", glyphCount);
  requireTrue(glyphCount == 2, "2 visible glyphs for 'Hi'");

  // Ingest glyph data
  dc::IngestProcessor ingest;
  std::vector<std::uint8_t> batch;
  appendRecord(batch, 1, 4, 0, instances.data(),
               static_cast<std::uint32_t>(instances.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));

  // Update geometry vertex count
  ok(cp.applyJsonText(
    std::string(R"({"cmd":"setGeometryVertexCount","geometryId":5,"vertexCount":)") +
    std::to_string(glyphCount) + "}"), "setVC");

  // Upload to GPU
  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(4, ingest.getBufferData(4), ingest.getBufferSize(4));

  dc::Renderer renderer;
  requireTrue(renderer.init(), "renderer init");
  renderer.setGlyphAtlas(&atlas);
  gpuBufs.uploadDirty();

  // Render
  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  std::printf("Draw calls: %u\n", stats.drawCalls);
  requireTrue(stats.drawCalls == 1, "1 draw call for text");

  // Read pixels and check for non-black
  auto pixels = ctx.readPixels();
  int nonBlack = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (isNonBlack(pixels, W, x, y)) nonBlack++;
    }
  }
  std::printf("Non-black pixels: %d\n", nonBlack);
  requireTrue(nonBlack > 20, "text rendered visible pixels");

  std::printf("\nD3.3 textSDF PASS\n");
  return 0;
}
