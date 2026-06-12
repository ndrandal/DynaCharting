// ENC-606 (P1.15) — regression guard for the setDrawItemStyle PRIMARY-color key
// wiring. The ENC-606 render proof drives the SMA line's color through
// setDrawItemStyle using the explicit colorR/colorG/colorB/colorA keys (the same
// naming convention as the candle colorUp*/colorDown* keys). cmdSetDrawItemStyle
// originally read the primary color ONLY from the short r/g/b/a keys, so a caller
// using colorR/colorG/colorB left di.color at the default white and the line
// rendered white instead of amber. This asserts BOTH key forms reach di.color,
// plus that colorUp*/colorDown* still land on di.colorUp/.colorDown.
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Types.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
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

static bool eqf(float a, float b) { return std::fabs(a - b) < 1e-5f; }

static void apply(dc::CommandProcessor& cp, const char* json) {
  auto r = cp.applyJsonText(json);
  if (!r.ok) {
    std::fprintf(stderr, "apply failed: %s -> %s / %s\n", json,
                 r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

int main() {
  std::printf("=== ENC-606 setDrawItemStyle color-key wiring ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  apply(cp, R"({"cmd":"createPane","id":1})");
  apply(cp, R"({"cmd":"createLayer","id":2,"paneId":1})");
  apply(cp, R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  apply(cp, R"({"cmd":"createDrawItem","id":4,"layerId":2})");

  // Primary color via the EXPLICIT colorR/colorG/colorB/colorA keys (the form the
  // ENC-606 render proof uses for the amber SMA line, #ffb300).
  apply(cp,
        R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
        R"("colorR":1.0,"colorG":0.701961,"colorB":0.0,"colorA":1.0})");
  const dc::DrawItem* di3 = scene.getDrawItem(3);
  check(di3 && eqf(di3->color[0], 1.0f) && eqf(di3->color[1], 0.701961f) &&
            eqf(di3->color[2], 0.0f) && eqf(di3->color[3], 1.0f),
        "colorR/colorG/colorB/colorA reach di.color (#ffb300 amber)");

  // The short r/g/b/a key form must STILL work (back-compat).
  apply(cp,
        R"({"cmd":"setDrawItemStyle","drawItemId":4,)"
        R"("r":0.25,"g":0.5,"b":0.75,"a":0.5})");
  const dc::DrawItem* di4 = scene.getDrawItem(4);
  check(di4 && eqf(di4->color[0], 0.25f) && eqf(di4->color[1], 0.5f) &&
            eqf(di4->color[2], 0.75f) && eqf(di4->color[3], 0.5f),
        "short r/g/b/a keys still reach di.color (back-compat)");

  // Candle up/down keys land on di.colorUp / di.colorDown (#26a69a / #ef5350).
  apply(cp,
        R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
        R"("colorUpR":0.149,"colorUpG":0.651,"colorUpB":0.604,"colorUpA":1.0,)"
        R"("colorDownR":0.937,"colorDownG":0.325,"colorDownB":0.314,"colorDownA":1.0})");
  check(di3 && eqf(di3->colorUp[1], 0.651f) && eqf(di3->colorDown[0], 0.937f),
        "colorUp*/colorDown* reach di.colorUp/.colorDown (candle up/down)");

  // Setting only the up/down colors must NOT clobber the previously-set primary.
  check(di3 && eqf(di3->color[0], 1.0f) && eqf(di3->color[2], 0.0f),
        "primary color survives a candle-only style merge");

  std::printf("=== ENC-606 color-key: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
