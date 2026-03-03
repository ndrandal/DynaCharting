// D52.1 — LodManager: 3 LOD levels, verify geometry swaps at zoom thresholds
#include "dc/data/LodManager.hpp"
#include "dc/scene/Scene.hpp"

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
  std::printf("=== D52.1 LodManager Tests ===\n");

  dc::Scene scene;

  // Create pane, layer, and draw item
  dc::Pane pane;
  pane.id = 1;
  scene.addPane(pane);

  dc::Layer layer;
  layer.id = 10;
  layer.paneId = 1;
  scene.addLayer(layer);

  // Create 3 geometry resources for LOD levels
  dc::Buffer buf;
  buf.id = 100;
  buf.byteLength = 1024;
  scene.addBuffer(buf);

  dc::Geometry geoHigh;
  geoHigh.id = 200;
  geoHigh.vertexBufferId = 100;
  geoHigh.vertexCount = 1000;
  scene.addGeometry(geoHigh);

  dc::Geometry geoMed;
  geoMed.id = 201;
  geoMed.vertexBufferId = 100;
  geoMed.vertexCount = 500;
  scene.addGeometry(geoMed);

  dc::Geometry geoLow;
  geoLow.id = 202;
  geoLow.vertexBufferId = 100;
  geoLow.vertexCount = 100;
  scene.addGeometry(geoLow);

  dc::DrawItem di;
  di.id = 300;
  di.layerId = 10;
  di.pipeline = "triSolid@1";
  di.geometryId = 202; // start at low
  scene.addDrawItem(di);

  // Configure LodManager
  dc::LodManager lod;
  dc::LodManagerConfig cfg;
  cfg.hysteresis = 0.0f; // no hysteresis for basic tests
  lod.setConfig(cfg);

  dc::LodGroup group;
  group.drawItemId = 300;
  // Levels sorted by threshold descending
  group.levels = {
    {10.0f, 200},  // high detail: >= 10 px/unit
    {5.0f,  201},  // medium detail: >= 5 px/unit
    {0.0f,  202}   // low detail: >= 0 px/unit (always matches)
  };
  lod.setGroup(group);

  // Test 1: Very zoomed in (20 px/unit) → high detail
  {
    lod.update(20.0f, scene);
    check(lod.currentLevel(300) == 0, "20 px/unit → level 0 (high detail)");
    check(scene.getDrawItem(300)->geometryId == 200, "geometryId = 200 (high)");
  }

  // Test 2: Medium zoom (7 px/unit) → medium detail
  {
    lod.update(7.0f, scene);
    check(lod.currentLevel(300) == 1, "7 px/unit → level 1 (medium detail)");
    check(scene.getDrawItem(300)->geometryId == 201, "geometryId = 201 (medium)");
  }

  // Test 3: Zoomed out (2 px/unit) → low detail
  {
    lod.update(2.0f, scene);
    check(lod.currentLevel(300) == 2, "2 px/unit → level 2 (low detail)");
    check(scene.getDrawItem(300)->geometryId == 202, "geometryId = 202 (low)");
  }

  // Test 4: Exactly at threshold boundary (10.0) → picks high
  {
    lod.update(10.0f, scene);
    check(lod.currentLevel(300) == 0, "exactly 10 px/unit → level 0");
    check(scene.getDrawItem(300)->geometryId == 200, "geometryId = 200 at boundary");
  }

  // Test 5: Exactly at medium threshold (5.0) → picks medium
  {
    lod.update(5.0f, scene);
    check(lod.currentLevel(300) == 1, "exactly 5 px/unit → level 1");
    check(scene.getDrawItem(300)->geometryId == 201, "geometryId = 201 at medium boundary");
  }

  // Test 6: Zero px/unit → low detail
  {
    lod.update(0.0f, scene);
    check(lod.currentLevel(300) == 2, "0 px/unit → level 2 (low)");
  }

  // Test 7: Remove group
  {
    lod.removeGroup(300);
    check(lod.currentLevel(300) == -1, "removed group returns -1");
  }

  // Test 8: Unknown drawItem returns -1
  {
    check(lod.currentLevel(999) == -1, "unknown drawItem returns -1");
  }

  // Test 9: Multiple groups
  {
    dc::DrawItem di2;
    di2.id = 301;
    di2.layerId = 10;
    di2.pipeline = "triSolid@1";
    di2.geometryId = 202;
    scene.addDrawItem(di2);

    dc::LodGroup g1;
    g1.drawItemId = 300;
    g1.levels = {{10.0f, 200}, {0.0f, 202}};

    dc::LodGroup g2;
    g2.drawItemId = 301;
    g2.levels = {{20.0f, 200}, {0.0f, 202}};

    lod.setGroup(g1);
    lod.setGroup(g2);

    lod.update(15.0f, scene);
    check(lod.currentLevel(300) == 0, "group1: 15 >= 10 → level 0");
    check(lod.currentLevel(301) == 1, "group2: 15 < 20 → level 1");
    check(scene.getDrawItem(300)->geometryId == 200, "group1 got high geo");
    check(scene.getDrawItem(301)->geometryId == 202, "group2 got low geo");
  }

  std::printf("=== D52.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
