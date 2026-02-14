// D9.4 — Layout interaction test (pure C++, no GL)
// Tests: divider hover detection, cursor away, drag resize, three panes.

#include "dc/layout/LayoutInteraction.hpp"
#include "dc/layout/LayoutManager.hpp"

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
  constexpr int W = 400;
  constexpr int H = 300;

  // --- Test 1: Two panes 70/30 → cursor at divider → hoveredDivider == 0 ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    cfg.minFraction = 0.1f;
    lm.setConfig(cfg);
    lm.setPanes({{1, 0.7f}, {2, 0.3f}});

    dc::LayoutInteraction li;
    li.setLayoutManager(&lm);
    li.setPixelViewport(W, H);

    // Divider clip Y position
    float divClipY = lm.dividerClipY(0);
    // Convert clip Y to pixel Y: py = (1.0 - clipY) / 2.0 * H
    double divPixelY = (1.0 - static_cast<double>(divClipY)) / 2.0 * H;

    dc::ViewportInputState input;
    input.cursorX = W / 2.0;
    input.cursorY = divPixelY;
    li.processInput(input);

    requireTrue(li.hoveredDivider() == 0, "hoveredDivider == 0 at divider");
    std::printf("  Test 1 (hover at divider) PASS\n");
  }

  // --- Test 2: Cursor away from divider → hoveredDivider == -1 ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    lm.setConfig(cfg);
    lm.setPanes({{1, 0.7f}, {2, 0.3f}});

    dc::LayoutInteraction li;
    li.setLayoutManager(&lm);
    li.setPixelViewport(W, H);

    dc::ViewportInputState input;
    input.cursorX = W / 2.0;
    input.cursorY = 10.0; // near top of screen, far from divider
    li.processInput(input);

    requireTrue(li.hoveredDivider() == -1, "no hover far from divider");
    std::printf("  Test 2 (cursor away) PASS\n");
  }

  // --- Test 3: Drag → fractions change, respects minFraction ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    cfg.minFraction = 0.1f;
    lm.setConfig(cfg);
    lm.setPanes({{1, 0.5f}, {2, 0.5f}});

    dc::LayoutInteraction li;
    li.setLayoutManager(&lm);
    li.setPixelViewport(W, H);

    float divClipY = lm.dividerClipY(0);
    double divPixelY = (1.0 - static_cast<double>(divClipY)) / 2.0 * H;

    // First, hover over divider
    dc::ViewportInputState hover;
    hover.cursorX = W / 2.0;
    hover.cursorY = divPixelY;
    li.processInput(hover);
    requireTrue(li.hoveredDivider() == 0, "hover before drag");

    // Start drag (cursor still near divider, with drag delta)
    dc::ViewportInputState dragStart;
    dragStart.cursorX = W / 2.0;
    dragStart.cursorY = divPixelY;
    dragStart.dragDy = 20.0;
    dragStart.dragging = true;
    bool changed = li.processInput(dragStart);
    requireTrue(changed, "drag changed layout");
    requireTrue(li.isDragging(), "is dragging");

    // Pane 1 should have grown (divider moved down = pane above grows)
    float frac1 = lm.getFraction(1);
    float frac2 = lm.getFraction(2);
    requireTrue(frac1 > 0.5f, "pane1 grew");
    requireTrue(frac2 < 0.5f, "pane2 shrank");
    requireTrue(frac2 >= cfg.minFraction - 1e-5f, "pane2 >= minFraction");

    std::printf("  Test 3 (drag resize) PASS\n");
  }

  // --- Test 4: Three panes → correct divider index identification ---
  {
    dc::LayoutManager lm;
    dc::LayoutConfig cfg;
    cfg.gap = 0.05f;
    cfg.margin = 0.05f;
    lm.setConfig(cfg);
    lm.setPanes({{1, 0.4f}, {2, 0.3f}, {3, 0.3f}});

    dc::LayoutInteraction li;
    li.setLayoutManager(&lm);
    li.setPixelViewport(W, H);

    requireTrue(lm.dividerCount() == 2, "2 dividers for 3 panes");

    // Test divider 0
    float div0ClipY = lm.dividerClipY(0);
    double div0PixelY = (1.0 - static_cast<double>(div0ClipY)) / 2.0 * H;

    dc::ViewportInputState input0;
    input0.cursorX = W / 2.0;
    input0.cursorY = div0PixelY;
    li.processInput(input0);
    requireTrue(li.hoveredDivider() == 0, "hoveredDivider == 0 for first divider");

    // Test divider 1
    float div1ClipY = lm.dividerClipY(1);
    double div1PixelY = (1.0 - static_cast<double>(div1ClipY)) / 2.0 * H;

    dc::ViewportInputState input1;
    input1.cursorX = W / 2.0;
    input1.cursorY = div1PixelY;
    li.processInput(input1);
    requireTrue(li.hoveredDivider() == 1, "hoveredDivider == 1 for second divider");

    std::printf("  Test 4 (three panes divider IDs) PASS\n");
  }

  std::printf("D9.4 layout interaction: ALL PASS\n");
  return 0;
}
