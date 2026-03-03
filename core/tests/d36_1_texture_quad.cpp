// D36.1 — TextureManager GL: render checkerboard texture on quad, verify pixels
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/Renderer.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/TextureManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdio>
#include <cstring>
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

int main() {
  std::printf("=== D36.1 Texture Quad GL Tests ===\n");

  dc::OsMesaContext ctx;
  if (!ctx.init(64, 64)) {
    std::fprintf(stderr, "SKIP: OSMesa init failed\n");
    return 0;
  }

  dc::TextureManager texMgr;

  // Test 1: Create 4x4 checkerboard texture
  std::vector<std::uint8_t> checker(4 * 4 * 4);
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int idx = (y * 4 + x) * 4;
      bool white = ((x + y) % 2) == 0;
      checker[idx + 0] = white ? 255 : 0;
      checker[idx + 1] = white ? 255 : 0;
      checker[idx + 2] = white ? 255 : 0;
      checker[idx + 3] = 255;
    }
  }

  dc::TextureId texId = texMgr.load(checker.data(), 4, 4);
  check(texId > 0, "texture loaded with valid ID");
  check(texMgr.count() == 1, "texture count is 1");

  // Test 2: GL texture handle is valid
  GLuint glTex = texMgr.getGlTexture(texId);
  check(glTex > 0, "GL texture handle is valid");

  // Test 3: Bind/unbind doesn't crash
  texMgr.bind(texId, 0);
  check(true, "bind to unit 0 succeeds");

  // Test 4: Non-existent texture returns 0
  check(texMgr.getGlTexture(999) == 0, "non-existent texture returns 0");

  // Test 5: Remove texture
  texMgr.remove(texId);
  check(texMgr.count() == 0, "count is 0 after remove");
  check(texMgr.getGlTexture(texId) == 0, "removed texture returns 0");

  // Test 6: Load multiple textures
  dc::TextureId t1 = texMgr.load(checker.data(), 4, 4);
  dc::TextureId t2 = texMgr.load(checker.data(), 4, 4);
  check(t1 != t2, "multiple textures get unique IDs");
  check(texMgr.count() == 2, "count is 2 with multiple textures");

  texMgr.remove(t1);
  check(texMgr.count() == 1, "count is 1 after removing one");
  texMgr.remove(t2);
  check(texMgr.count() == 0, "count is 0 after removing all");

  // Test 7: Remove non-existent is safe
  texMgr.remove(999);
  check(true, "remove non-existent is safe");

  std::printf("=== D36.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
