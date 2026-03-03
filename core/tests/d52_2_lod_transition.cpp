// D52.2 — LodManager hysteresis: verify level doesn't thrash near threshold boundary
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
  std::printf("=== D52.2 LodManager Hysteresis Tests ===\n");

  dc::Scene scene;

  dc::Pane pane;
  pane.id = 1;
  scene.addPane(pane);

  dc::Layer layer;
  layer.id = 10;
  layer.paneId = 1;
  scene.addLayer(layer);

  dc::Buffer buf;
  buf.id = 100;
  buf.byteLength = 1024;
  scene.addBuffer(buf);

  dc::Geometry geoHigh;
  geoHigh.id = 200;
  geoHigh.vertexBufferId = 100;
  geoHigh.vertexCount = 1000;
  scene.addGeometry(geoHigh);

  dc::Geometry geoLow;
  geoLow.id = 201;
  geoLow.vertexBufferId = 100;
  geoLow.vertexCount = 100;
  scene.addGeometry(geoLow);

  dc::DrawItem di;
  di.id = 300;
  di.layerId = 10;
  di.pipeline = "triSolid@1";
  di.geometryId = 201;
  scene.addDrawItem(di);

  dc::LodManager lod;
  dc::LodManagerConfig cfg;
  cfg.hysteresis = 0.1f; // 10% hysteresis
  lod.setConfig(cfg);

  // Two levels: high (>= 10 px/unit) and low (>= 0)
  dc::LodGroup group;
  group.drawItemId = 300;
  group.levels = {
    {10.0f, 200},  // high detail
    {0.0f,  201}   // low detail
  };
  lod.setGroup(group);

  // Test 1: Start well below threshold → low detail
  {
    lod.update(5.0f, scene);
    check(lod.currentLevel(300) == 1, "5 px/unit → low detail (level 1)");
    check(scene.getDrawItem(300)->geometryId == 201, "geometryId = 201 (low)");
  }

  // Test 2: Rise to exactly the threshold (10.0) — hysteresis should prevent switch
  // To switch UP to high detail, need >= 10.0 * (1 - 0.1) = 9.0
  // Actually, we're going from low to high (bestLevel=0 < current=1),
  // so we need pixelsPerDataUnit >= targetThreshold * (1 - hysteresis) = 10.0 * 0.9 = 9.0
  // At 10.0 we're above 9.0, so it SHOULD switch.
  {
    lod.update(10.0f, scene);
    check(lod.currentLevel(300) == 0, "10 px/unit → switches to high (above 9.0 threshold)");
    check(scene.getDrawItem(300)->geometryId == 200, "geometryId = 200 (high)");
  }

  // Test 3: Now at high detail, drop slightly below threshold
  // To switch DOWN from high to low, we need the drop check:
  // currentThreshold = 10.0, dropRequired = 10.0 * (1 - 0.1) = 9.0
  // At 9.5 px/unit: bestLevel would be 1 (below 10.0), but
  // 9.5 > dropRequired (9.0), so hysteresis keeps us at high
  {
    lod.update(9.5f, scene);
    check(lod.currentLevel(300) == 0, "9.5 px/unit: hysteresis keeps high detail");
    check(scene.getDrawItem(300)->geometryId == 200, "geometryId stays 200");
  }

  // Test 4: Drop well below — should switch to low
  {
    lod.update(8.0f, scene);
    check(lod.currentLevel(300) == 1, "8.0 px/unit: below hysteresis band → low detail");
    check(scene.getDrawItem(300)->geometryId == 201, "geometryId = 201 (low)");
  }

  // Test 5: Rapid oscillation near boundary — should not thrash
  {
    // Reset to a known state: go well above threshold first
    lod.update(20.0f, scene);
    check(lod.currentLevel(300) == 0, "reset: 20 px/unit → high detail");

    int switchCount = 0;
    int prevLevel = lod.currentLevel(300);

    // Oscillate between 9.2 and 10.2 (within hysteresis band around 10.0)
    float values[] = {9.2f, 10.2f, 9.3f, 10.1f, 9.4f, 10.0f, 9.5f, 9.8f};
    for (float v : values) {
      lod.update(v, scene);
      int cur = lod.currentLevel(300);
      if (cur != prevLevel) ++switchCount;
      prevLevel = cur;
    }

    check(switchCount <= 2, "hysteresis prevents excessive thrashing near boundary");
  }

  // Test 6: Zero hysteresis — immediate switching
  {
    dc::LodManagerConfig noCfg;
    noCfg.hysteresis = 0.0f;
    lod.setConfig(noCfg);

    lod.update(20.0f, scene); // start high
    check(lod.currentLevel(300) == 0, "no hysteresis: start high");

    lod.update(9.9f, scene);  // just below threshold → should switch immediately
    check(lod.currentLevel(300) == 1, "no hysteresis: 9.9 → immediate switch to low");

    lod.update(10.0f, scene); // at threshold → should switch back
    check(lod.currentLevel(300) == 0, "no hysteresis: 10.0 → immediate switch to high");
  }

  // Test 7: Three levels with hysteresis
  {
    dc::LodManagerConfig cfg3;
    cfg3.hysteresis = 0.1f;
    lod.setConfig(cfg3);

    dc::Geometry geoMed;
    geoMed.id = 202;
    geoMed.vertexBufferId = 100;
    geoMed.vertexCount = 500;
    scene.addGeometry(geoMed);

    dc::LodGroup g3;
    g3.drawItemId = 300;
    g3.levels = {
      {20.0f, 200},  // high: >= 20
      {10.0f, 202},  // medium: >= 10
      {0.0f,  201}   // low: always
    };
    lod.setGroup(g3);

    lod.update(25.0f, scene);
    check(lod.currentLevel(300) == 0, "3-level: 25 → high");

    lod.update(15.0f, scene);
    check(lod.currentLevel(300) == 1, "3-level: 15 → medium");

    lod.update(3.0f, scene);
    check(lod.currentLevel(300) == 2, "3-level: 3 → low");

    lod.update(25.0f, scene);
    check(lod.currentLevel(300) == 0, "3-level: back to 25 → high");
  }

  std::printf("=== D52.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
