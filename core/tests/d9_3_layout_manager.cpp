// D9.3 — LayoutManager test (pure C++, no GL)
// Tests: two-pane layout, three-pane, resizeDivider, minFraction clamping,
//        dividerClipY, applyLayout issues setPaneRegion commands.

#include "dc/layout/LayoutManager.hpp"
#include "dc/layout/PaneLayout.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Two panes {0.7, 0.3} ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    cfg.minFraction = 0.1f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.7f}, {2, 0.3f}});

    auto expected = dc::computePaneLayout({0.7f, 0.3f}, 0.05f, 0.05f);
    const auto& regions = lm.regions();

    requireTrue(regions.size() == 2, "two regions");
    requireClose(regions[0].clipYMax, expected[0].clipYMax, 1e-5f, "pane0 clipYMax");
    requireClose(regions[0].clipYMin, expected[0].clipYMin, 1e-5f, "pane0 clipYMin");
    requireClose(regions[1].clipYMax, expected[1].clipYMax, 1e-5f, "pane1 clipYMax");
    requireClose(regions[1].clipYMin, expected[1].clipYMin, 1e-5f, "pane1 clipYMin");
    requireTrue(lm.dividerCount() == 1, "1 divider");

    std::printf("  Test 1 (two panes) PASS\n");
  }

  // --- Test 2: Three panes ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.5f}, {2, 0.3f}, {3, 0.2f}});

    auto expected = dc::computePaneLayout({0.5f, 0.3f, 0.2f}, 0.05f, 0.05f);
    const auto& regions = lm.regions();

    requireTrue(regions.size() == 3, "three regions");
    for (std::size_t i = 0; i < 3; i++) {
      requireClose(regions[i].clipYMax, expected[i].clipYMax, 1e-5f, "pane clipYMax");
      requireClose(regions[i].clipYMin, expected[i].clipYMin, 1e-5f, "pane clipYMin");
    }
    requireTrue(lm.dividerCount() == 2, "2 dividers");

    std::printf("  Test 2 (three panes) PASS\n");
  }

  // --- Test 3: resizeDivider(0, 0.1) ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    cfg.minFraction = 0.1f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.7f}, {2, 0.3f}});
    float frac1Before = lm.getFraction(1);
    float frac2Before = lm.getFraction(2);

    lm.resizeDivider(0, 0.1f);

    float frac1After = lm.getFraction(1);
    float frac2After = lm.getFraction(2);

    requireClose(frac1After, frac1Before + 0.1f, 1e-5f, "pane1 grew by 0.1");
    requireClose(frac2After, frac2Before - 0.1f, 1e-5f, "pane2 shrank by 0.1");

    std::printf("  Test 3 (resizeDivider) PASS\n");
  }

  // --- Test 4: Resize clamped by minFraction ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    cfg.minFraction = 0.1f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.5f}, {2, 0.5f}});

    // Try to grow pane1 by 0.9 — should be clamped so pane2 doesn't go below minFraction
    lm.resizeDivider(0, 0.9f);

    float frac2 = lm.getFraction(2);
    requireTrue(frac2 >= cfg.minFraction * 1.0f - 1e-5f, "pane2 >= minFraction after clamp");

    std::printf("  Test 4 (resize clamped) PASS\n");
  }

  // --- Test 5: dividerClipY returns expected position ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.5f}, {2, 0.5f}});

    const auto& regions = lm.regions();
    float expectedMid = (regions[0].clipYMin + regions[1].clipYMax) / 2.0f;
    requireClose(lm.dividerClipY(0), expectedMid, 1e-5f, "dividerClipY midpoint");

    std::printf("  Test 5 (dividerClipY) PASS\n");
  }

  // --- Test 6: applyLayout issues setPaneRegion commands ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    // Create panes in scene first
    cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P1"})");
    cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"P2"})");

    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    lm.setConfig(cfg);

    lm.setPanes({{1, 0.6f}, {2, 0.4f}});
    lm.applyLayout(cp);

    const auto& regions = lm.regions();
    const dc::Pane* p1 = scene.getPane(1);
    const dc::Pane* p2 = scene.getPane(2);

    requireTrue(p1 != nullptr && p2 != nullptr, "panes exist in scene");
    requireClose(p1->region.clipYMin, regions[0].clipYMin, 1e-5f, "p1 clipYMin from applyLayout");
    requireClose(p1->region.clipYMax, regions[0].clipYMax, 1e-5f, "p1 clipYMax from applyLayout");
    requireClose(p2->region.clipYMin, regions[1].clipYMin, 1e-5f, "p2 clipYMin from applyLayout");
    requireClose(p2->region.clipYMax, regions[1].clipYMax, 1e-5f, "p2 clipYMax from applyLayout");

    std::printf("  Test 6 (applyLayout commands) PASS\n");
  }

  std::printf("D9.3 layout manager: ALL PASS\n");
  return 0;
}
