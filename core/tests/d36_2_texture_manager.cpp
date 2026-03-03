// D36.2 — TextureManager GL: load/remove/bind lifecycle
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/TextureManager.hpp"

#include <cstdio>
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
  std::printf("=== D36.2 TextureManager Lifecycle Tests ===\n");

  dc::OsMesaContext ctx;
  if (!ctx.init(32, 32)) {
    std::fprintf(stderr, "SKIP: OSMesa init failed\n");
    return 0;
  }

  // Test 1: Single pixel texture
  {
    dc::TextureManager mgr;
    std::uint8_t pixel[] = {255, 0, 0, 255}; // red
    dc::TextureId id = mgr.load(pixel, 1, 1);
    check(id > 0, "1x1 texture loaded");
    check(mgr.getGlTexture(id) > 0, "has GL texture");
    mgr.remove(id);
    check(mgr.count() == 0, "removed 1x1 texture");
  }

  // Test 2: Large texture
  {
    dc::TextureManager mgr;
    std::vector<std::uint8_t> large(256 * 256 * 4, 128);
    dc::TextureId id = mgr.load(large.data(), 256, 256);
    check(id > 0, "256x256 texture loaded");
    mgr.bind(id, 0);
    check(true, "bind large texture");
    mgr.remove(id);
  }

  // Test 3: Multiple binds to different units
  {
    dc::TextureManager mgr;
    std::uint8_t r[] = {255, 0, 0, 255};
    std::uint8_t g[] = {0, 255, 0, 255};
    dc::TextureId t1 = mgr.load(r, 1, 1);
    dc::TextureId t2 = mgr.load(g, 1, 1);
    mgr.bind(t1, 0);
    mgr.bind(t2, 1);
    check(true, "bind to units 0 and 1");
    mgr.remove(t1);
    mgr.remove(t2);
  }

  // Test 4: IDs are monotonically increasing
  {
    dc::TextureManager mgr;
    std::uint8_t px[] = {0, 0, 0, 255};
    dc::TextureId id1 = mgr.load(px, 1, 1);
    dc::TextureId id2 = mgr.load(px, 1, 1);
    dc::TextureId id3 = mgr.load(px, 1, 1);
    check(id2 == id1 + 1, "IDs are sequential");
    check(id3 == id2 + 1, "IDs are sequential (2)");

    // Remove middle, add new
    mgr.remove(id2);
    dc::TextureId id4 = mgr.load(px, 1, 1);
    check(id4 == id3 + 1, "new ID after remove continues sequence");
  }

  // Test 5: Destructor cleans up
  {
    dc::TextureManager mgr;
    std::uint8_t px[] = {0, 0, 0, 255};
    mgr.load(px, 1, 1);
    mgr.load(px, 1, 1);
    mgr.load(px, 1, 1);
    // Destructor runs here — should not crash
  }
  check(true, "destructor cleanup succeeds");

  // Test 6: Bind non-existent texture is safe
  {
    dc::TextureManager mgr;
    mgr.bind(999, 0); // should not crash
    check(true, "bind non-existent is safe");
  }

  std::printf("=== D36.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
