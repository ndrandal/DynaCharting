// D44.1 — AnnotationRenderer: text generation from AnnotationStore
// Needs font file for glyph atlas.
#include "dc/metadata/AnnotationRenderer.hpp"
#include "dc/metadata/AnnotationStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"

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
  std::printf("=== D44.1 AnnotationRenderer Tests ===\n");

  // Load font
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

  // Setup scene, registry, command processor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);
  cp.setGlyphAtlas(&atlas);

  // Create pane and layer for annotations
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Main"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  // Setup annotation store
  dc::AnnotationStore store;
  store.set(100, "series", "Open", "line");
  store.set(200, "series", "Close", "line");
  store.set(300, "indicator", "RSI", "70.5");

  // Test 1: Create renderer with proper config
  dc::AnnotationRenderer renderer;
  renderer.setAnnotationStore(&store);
  renderer.setGlyphAtlas(&atlas);
  renderer.setCommandProcessor(&cp);
  renderer.setIngestProcessor(&ingest);

  dc::AnnotationRendererConfig cfg;
  cfg.paneId = 1;
  cfg.layerId = 2;
  cfg.baseBufferId = 1000;
  cfg.baseGeomId = 2000;
  cfg.baseDrawItemId = 3000;
  cfg.fontSize = 14.0f;
  cfg.color[0] = 1.0f; cfg.color[1] = 1.0f; cfg.color[2] = 0.0f; cfg.color[3] = 1.0f;
  renderer.setConfig(cfg);

  // Test 2: Update creates resources
  renderer.update(scene, 400, 300);
  check(renderer.renderedCount() > 0, "renderedCount > 0 after update");
  check(renderer.renderedCount() == 3, "renderedCount == 3 for 3 annotations");

  // Test 3: Scene has created draw items
  check(scene.hasDrawItem(3000), "drawItem 3000 exists");
  check(scene.hasDrawItem(3001), "drawItem 3001 exists");
  check(scene.hasDrawItem(3002), "drawItem 3002 exists");

  // Test 4: Draw items are textSDF pipeline
  const dc::DrawItem* di = scene.getDrawItem(3000);
  check(di != nullptr && di->pipeline == "textSDF@1", "pipeline is textSDF@1");

  // Test 5: Draw items have correct layer
  check(di != nullptr && di->layerId == 2, "layerId is 2");

  // Test 6: Geometries exist
  check(scene.hasGeometry(2000), "geometry 2000 exists");
  check(scene.hasGeometry(2001), "geometry 2001 exists");
  check(scene.hasGeometry(2002), "geometry 2002 exists");

  // Test 7: Geometry has glyph8 format
  const dc::Geometry* geo = scene.getGeometry(2000);
  check(geo != nullptr && geo->format == dc::VertexFormat::Glyph8,
        "geometry format is glyph8");

  // Test 8: Geometry has non-zero vertex count
  check(geo != nullptr && geo->vertexCount > 0, "geometry vertexCount > 0");

  // Test 9: Buffers exist
  check(scene.hasBuffer(1000), "buffer 1000 exists");
  check(scene.hasBuffer(1001), "buffer 1001 exists");
  check(scene.hasBuffer(1002), "buffer 1002 exists");

  // Test 10: Dispose cleans up
  renderer.dispose();
  check(renderer.renderedCount() == 0, "renderedCount == 0 after dispose");
  check(!scene.hasDrawItem(3000), "drawItem 3000 gone after dispose");
  check(!scene.hasGeometry(2000), "geometry 2000 gone after dispose");
  check(!scene.hasBuffer(1000), "buffer 1000 gone after dispose");

  // Test 11: Update after dispose works again
  renderer.update(scene, 400, 300);
  check(renderer.renderedCount() == 3, "re-created after dispose");

  // Test 12: Update with empty store
  renderer.dispose();
  dc::AnnotationStore emptyStore;
  renderer.setAnnotationStore(&emptyStore);
  renderer.update(scene, 400, 300);
  check(renderer.renderedCount() == 0, "renderedCount == 0 with empty store");

  std::printf("=== D44.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
