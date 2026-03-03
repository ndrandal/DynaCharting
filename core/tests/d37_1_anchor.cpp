// D37.1 — Anchor: verify clip positions for all 9 anchor points, pixel offsets
#include "dc/layout/Anchor.hpp"

#include <cmath>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static bool near(float a, float b, float eps = 1e-4f) {
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
  std::printf("=== D37.1 Anchor Tests ===\n");

  // Full-screen pane: [-1,1] x [-1,1]
  dc::PaneRegion full{-1.0f, 1.0f, -1.0f, 1.0f};
  int viewW = 800, viewH = 600;

  // Test 1-9: All 9 anchor points with no offset
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::TopLeft, full, 0, 0, viewW, viewH);
    check(near(cx, -1.0f) && near(cy, 1.0f), "TopLeft: (-1, 1)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::TopCenter, full, 0, 0, viewW, viewH);
    check(near(cx, 0.0f) && near(cy, 1.0f), "TopCenter: (0, 1)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::TopRight, full, 0, 0, viewW, viewH);
    check(near(cx, 1.0f) && near(cy, 1.0f), "TopRight: (1, 1)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::MiddleLeft, full, 0, 0, viewW, viewH);
    check(near(cx, -1.0f) && near(cy, 0.0f), "MiddleLeft: (-1, 0)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::Center, full, 0, 0, viewW, viewH);
    check(near(cx, 0.0f) && near(cy, 0.0f), "Center: (0, 0)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::MiddleRight, full, 0, 0, viewW, viewH);
    check(near(cx, 1.0f) && near(cy, 0.0f), "MiddleRight: (1, 0)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::BottomLeft, full, 0, 0, viewW, viewH);
    check(near(cx, -1.0f) && near(cy, -1.0f), "BottomLeft: (-1, -1)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::BottomCenter, full, 0, 0, viewW, viewH);
    check(near(cx, 0.0f) && near(cy, -1.0f), "BottomCenter: (0, -1)");
  }
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::BottomRight, full, 0, 0, viewW, viewH);
    check(near(cx, 1.0f) && near(cy, -1.0f), "BottomRight: (1, -1)");
  }

  // Test 10: Pixel offset X
  {
    // 800px viewport: 2.0 clip / 800px = 0.0025 clip/px
    float pxToClipX = 2.0f / 800.0f;
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::TopLeft, full, 10.0f, 0, viewW, viewH);
    check(near(cx, -1.0f + 10.0f * pxToClipX), "TopLeft + 10px X offset");
    check(near(cy, 1.0f), "Y unchanged with X-only offset");
  }

  // Test 11: Pixel offset Y (positive Y = downward = decrease clip Y)
  {
    float pxToClipY = 2.0f / 600.0f;
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::TopLeft, full, 0, 20.0f, viewW, viewH);
    check(near(cx, -1.0f), "X unchanged with Y-only offset");
    check(near(cy, 1.0f - 20.0f * pxToClipY), "TopLeft + 20px Y offset (downward)");
  }

  // Test 12: Sub-pane region
  {
    dc::PaneRegion sub{0.0f, 1.0f, -0.5f, 0.5f}; // top half, center horizontal
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::Center, sub, 0, 0, viewW, viewH);
    check(near(cx, 0.0f), "sub-pane Center X = 0");
    check(near(cy, 0.5f), "sub-pane Center Y = 0.5");

    auto [cx2, cy2] = dc::computeAnchorClipPosition(dc::AnchorPoint::BottomLeft, sub, 0, 0, viewW, viewH);
    check(near(cx2, -0.5f), "sub-pane BottomLeft X = -0.5");
    check(near(cy2, 0.0f), "sub-pane BottomLeft Y = 0");
  }

  // Test 13: Zero-size viewport handles gracefully
  {
    auto [cx, cy] = dc::computeAnchorClipPosition(dc::AnchorPoint::Center, full, 10.0f, 10.0f, 0, 0);
    check(near(cx, 0.0f) && near(cy, 0.0f), "zero viewport: offsets ignored");
  }

  std::printf("=== D37.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
