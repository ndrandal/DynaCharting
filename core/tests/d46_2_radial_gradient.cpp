// D46.2 — Radial gradient: verify command + DrawItem field storage for radial type
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
  std::printf("=== D46.2 Radial Gradient Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Setup
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"p"})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1,"name":"l"})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":10,"layerId":2,"name":"radialItem"})");

  // Test 1: Set radial gradient with full params
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 10,
      "type": "radial",
      "color0": {"r": 1.0, "g": 1.0, "b": 0.0, "a": 1.0},
      "color1": {"r": 0.0, "g": 0.0, "b": 0.0, "a": 0.0},
      "center": {"x": 0.5, "y": 0.5},
      "radius": 0.75
    })");
    check(r.ok, "setDrawItemGradient radial: ok");

    const dc::DrawItem* di = scene.getDrawItem(10);
    check(di != nullptr, "drawItem 10 exists");
    check(di->gradientType == 2, "gradientType = 2 (Radial)");
    check(near(di->gradientColor0[0], 1.0f), "color0.r = 1");
    check(near(di->gradientColor0[1], 1.0f), "color0.g = 1");
    check(near(di->gradientColor0[2], 0.0f), "color0.b = 0");
    check(near(di->gradientColor0[3], 1.0f), "color0.a = 1");
    check(near(di->gradientColor1[0], 0.0f), "color1.r = 0");
    check(near(di->gradientColor1[1], 0.0f), "color1.g = 0");
    check(near(di->gradientColor1[2], 0.0f), "color1.b = 0");
    check(near(di->gradientColor1[3], 0.0f), "color1.a = 0");
    check(near(di->gradientCenter[0], 0.5f), "center.x = 0.5");
    check(near(di->gradientCenter[1], 0.5f), "center.y = 0.5");
    check(near(di->gradientRadius, 0.75f), "radius = 0.75");
  }

  // Test 2: Change radial to linear preserves colors
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 10,
      "type": "linear",
      "angle": 0.7854
    })");
    check(r.ok, "switch radial -> linear: ok");

    const dc::DrawItem* di = scene.getDrawItem(10);
    check(di->gradientType == 1, "gradientType = 1 (Linear)");
    check(near(di->gradientAngle, 0.7854f), "angle = pi/4");
    // Colors should be preserved from previous
    check(near(di->gradientColor0[0], 1.0f), "color0.r preserved");
    check(near(di->gradientColor0[1], 1.0f), "color0.g preserved");
    check(near(di->gradientRadius, 0.75f), "radius preserved");
  }

  // Test 3: Off-center radial
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 10,
      "type": "radial",
      "center": {"x": 0.2, "y": 0.8},
      "radius": 0.3
    })");
    check(r.ok, "off-center radial: ok");

    const dc::DrawItem* di = scene.getDrawItem(10);
    check(di->gradientType == 2, "gradientType = 2 (Radial)");
    check(near(di->gradientCenter[0], 0.2f), "center.x = 0.2");
    check(near(di->gradientCenter[1], 0.8f), "center.y = 0.8");
    check(near(di->gradientRadius, 0.3f), "radius = 0.3");
  }

  // Test 4: Multiple drawItems can have independent gradients
  {
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":11,"layerId":2,"name":"item2"})");

    cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 11,
      "type": "linear",
      "angle": 3.14159,
      "color0": {"r": 0.0, "g": 1.0, "b": 0.0, "a": 1.0},
      "color1": {"r": 1.0, "g": 0.0, "b": 1.0, "a": 1.0}
    })");

    const dc::DrawItem* di10 = scene.getDrawItem(10);
    const dc::DrawItem* di11 = scene.getDrawItem(11);

    check(di10->gradientType == 2, "item 10 still radial");
    check(di11->gradientType == 1, "item 11 is linear");
    check(near(di11->gradientAngle, 3.14159f, 1e-4f), "item 11 angle = pi");
    check(near(di11->gradientColor0[1], 1.0f), "item 11 color0.g = 1");
    check(!near(di10->gradientColor0[0], di11->gradientColor0[0]), "items have different color0.r");
  }

  // Test 5: Frame atomicity with gradient
  {
    cp.applyJsonText(R"({"cmd":"beginFrame","frameId":1})");
    cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 10,
      "type": "linear",
      "angle": 0.0
    })");

    // Before commit, active scene should still have radial
    // (the active scene is not updated until commit)
    // After commit:
    cp.applyJsonText(R"({"cmd":"commitFrame","frameId":1})");

    const dc::DrawItem* di = scene.getDrawItem(10);
    check(di->gradientType == 1, "after commit: gradientType = 1 (Linear)");
    check(near(di->gradientAngle, 0.0f), "after commit: angle = 0");
  }

  // Test 6: Zero-value gradient radius is valid
  {
    auto r = cp.applyJsonText(R"({
      "cmd": "setDrawItemGradient",
      "drawItemId": 10,
      "radius": 0.0
    })");
    check(r.ok, "zero radius: ok");

    const dc::DrawItem* di = scene.getDrawItem(10);
    check(near(di->gradientRadius, 0.0f), "radius = 0.0");
  }

  std::printf("=== D46.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
