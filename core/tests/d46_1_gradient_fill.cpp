// D46.1 — Gradient fill: setDrawItemGradient command + DrawItem field storage (linear)
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"

#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static bool near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) < eps;
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
  std::printf("=== D46.1 Gradient Fill Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Setup: create pane, layer, draw item
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"p"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"l"})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2,"name":"di"})");

  // Test 1: DrawItem defaults have no gradient
  {
    const dc::DrawItem* di = scene.getDrawItem(3);
    check(di != nullptr, "drawItem exists");
    check(di->gradientType == 0, "default gradientType = 0 (None)");
    check(near(di->gradientAngle, 0.0f), "default gradientAngle = 0");
    check(near(di->gradientRadius, 0.5f), "default gradientRadius = 0.5");
  }

  // Test 2: Set linear gradient
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 3,
      "type": "linear",
      "angle": 1.5708,
      "color0": {"r": 1.0, "g": 0.0, "b": 0.0, "a": 1.0},
      "color1": {"r": 0.0, "g": 0.0, "b": 1.0, "a": 1.0}
    })");
    check(r.ok, "setDrawItemGradient linear: ok");

    const dc::DrawItem* di = scene.getDrawItem(3);
    check(di->gradientType == 1, "gradientType = 1 (Linear)");
    check(near(di->gradientAngle, 1.5708f), "gradientAngle = pi/2");
    check(near(di->gradientColor0[0], 1.0f), "color0.r = 1");
    check(near(di->gradientColor0[1], 0.0f), "color0.g = 0");
    check(near(di->gradientColor0[2], 0.0f), "color0.b = 0");
    check(near(di->gradientColor0[3], 1.0f), "color0.a = 1");
    check(near(di->gradientColor1[0], 0.0f), "color1.r = 0");
    check(near(di->gradientColor1[1], 0.0f), "color1.g = 0");
    check(near(di->gradientColor1[2], 1.0f), "color1.b = 1");
    check(near(di->gradientColor1[3], 1.0f), "color1.a = 1");
  }

  // Test 3: Set gradient with center and radius (partial update)
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 3,
      "center": {"x": 0.3, "y": 0.7},
      "radius": 0.8
    })");
    check(r.ok, "setDrawItemGradient center+radius: ok");

    const dc::DrawItem* di = scene.getDrawItem(3);
    // type should still be linear from previous
    check(di->gradientType == 1, "gradientType still 1 after partial update");
    check(near(di->gradientCenter[0], 0.3f), "center.x = 0.3");
    check(near(di->gradientCenter[1], 0.7f), "center.y = 0.7");
    check(near(di->gradientRadius, 0.8f), "radius = 0.8");
  }

  // Test 4: Set gradient type to "none" clears type
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 3,
      "type": "none"
    })");
    check(r.ok, "setDrawItemGradient none: ok");

    const dc::DrawItem* di = scene.getDrawItem(3);
    check(di->gradientType == 0, "gradientType = 0 after setting none");
    // Other fields should remain unchanged
    check(near(di->gradientAngle, 1.5708f), "angle preserved after type=none");
  }

  // Test 5: Missing drawItemId -> error
  {
    auto r = cp.applyJsonText(R"({"cmd":"setDrawItemGradient","type":"linear"})");
    check(!r.ok, "missing drawItemId -> error");
    check(r.err.code == "BAD_COMMAND", "error code = BAD_COMMAND");
  }

  // Test 6: Non-existent drawItemId -> error
  {
    auto r = cp.applyJsonText(R"({"cmd":"setDrawItemGradient","drawItemId":999,"type":"linear"})");
    check(!r.ok, "non-existent drawItemId -> error");
    check(r.err.code == "MISSING_DRAWITEM", "error code = MISSING_DRAWITEM");
  }

  // Test 7: Partial color update (only some channels)
  {
    // First set to known state
    cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 3,
      "type": "linear",
      "color0": {"r": 0.5, "g": 0.5, "b": 0.5, "a": 0.5}
    })");

    // Then update only r channel
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 3,
      "color0": {"r": 0.9}
    })");
    check(r.ok, "partial color0 update: ok");

    const dc::DrawItem* di = scene.getDrawItem(3);
    check(near(di->gradientColor0[0], 0.9f), "color0.r updated to 0.9");
    check(near(di->gradientColor0[1], 0.5f), "color0.g unchanged at 0.5");
    check(near(di->gradientColor0[2], 0.5f), "color0.b unchanged at 0.5");
    check(near(di->gradientColor0[3], 0.5f), "color0.a unchanged at 0.5");
  }

  std::printf("=== D46.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
