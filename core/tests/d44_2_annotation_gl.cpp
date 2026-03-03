// D44.2 — AnnotationRenderer GL: render annotations and verify text pixels
#include "dc/metadata/AnnotationRenderer.hpp"
#include "dc/metadata/AnnotationStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdio>

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
  std::printf("=== D44.2 AnnotationRenderer GL Tests ===\n");

  constexpr int W = 200;
  constexpr int H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("SKIP: OSMesa not available\n");
    return 0;
  }

  dc::GlyphAtlas atlas;
#ifdef FONT_PATH
  bool fontOk = atlas.loadFontFile(FONT_PATH);
#else
  bool fontOk = false;
#endif
  if (!fontOk) {
    std::printf("SKIP: Font not available\n");
    return 0;
  }
  atlas.ensureAscii();

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  cp.setGlyphAtlas(&atlas);

  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;
  if (!renderer.init()) {
    std::fprintf(stderr, "Renderer init failed\n");
    return 1;
  }
  renderer.setGlyphAtlas(&atlas);

  // Create pane and layer
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Main"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  // Create annotation store with a label
  dc::AnnotationStore store;
  store.set(10, "label", "Hello", "World");

  // Setup annotation renderer
  dc::AnnotationRenderer annRenderer;
  annRenderer.setAnnotationStore(&store);
  annRenderer.setGlyphAtlas(&atlas);
  annRenderer.setCommandProcessor(&cp);
  annRenderer.setIngestProcessor(&ingest);

  dc::AnnotationRendererConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 2;
  cfg.baseBufferId = 500;
  cfg.baseGeomId = 600;
  cfg.baseDrawItemId = 700;
  cfg.fontSize = 20.0f;
  cfg.color[0] = 1.0f; cfg.color[1] = 1.0f; cfg.color[2] = 1.0f; cfg.color[3] = 1.0f;
  annRenderer.setConfig(cfg);

  // Generate annotation resources
  annRenderer.update(scene, W, H);
  check(annRenderer.renderedCount() == 1, "rendered 1 annotation");

  // Sync ingest buffer data to GPU
  for (auto id : scene.bufferIds()) {
    auto data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) {
      gpuBufs.setCpuData(id, data, size);
    }
  }
  gpuBufs.uploadDirty();

  // Render
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  renderer.render(scene, gpuBufs, W, H);

  auto fb = ctx.readPixels();

  // Test: At least some pixels should be non-black (text was rendered)
  int nonBlackPixels = 0;
  for (int i = 0; i < W * H; i++) {
    int idx = i * 4;
    if (fb[idx] > 30 || fb[idx + 1] > 30 || fb[idx + 2] > 30) {
      nonBlackPixels++;
    }
  }
  check(nonBlackPixels > 10, "text pixels rendered (non-black pixels > 10)");

  // Test: Top area should have text (annotation is positioned near top)
  int topNonBlack = 0;
  for (int y = H / 2; y < H; y++) {
    for (int x = 0; x < W; x++) {
      int idx = (y * W + x) * 4;
      if (fb[idx] > 30 || fb[idx + 1] > 30 || fb[idx + 2] > 30) {
        topNonBlack++;
      }
    }
  }
  check(topNonBlack > 5, "text pixels in top half");

  // Test: After dispose, re-render is clean
  annRenderer.dispose();
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  renderer.render(scene, gpuBufs, W, H);
  auto fb2 = ctx.readPixels();

  int afterDispose = 0;
  for (int i = 0; i < W * H; i++) {
    int idx = i * 4;
    if (fb2[idx] > 30 || fb2[idx + 1] > 30 || fb2[idx + 2] > 30) {
      afterDispose++;
    }
  }
  check(afterDispose == 0, "no text pixels after dispose");

  std::printf("=== D44.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
