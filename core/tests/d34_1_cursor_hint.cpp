// D34.1 — CursorHint: verify all priority combinations
#include "dc/viewport/CursorHint.hpp"

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
  std::printf("=== D34.1 CursorHint Tests ===\n");

  // Test 1: Default context -> Default cursor
  {
    dc::CursorHintContext ctx{};
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Default, "empty context -> Default");
  }

  // Test 2: isOverDrawItem -> Pointer
  {
    dc::CursorHintContext ctx{};
    ctx.isOverDrawItem = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Pointer, "isOverDrawItem -> Pointer");
  }

  // Test 3: isDragging -> Grabbing (highest priority)
  {
    dc::CursorHintContext ctx{};
    ctx.isDragging = true;
    ctx.isOverDrawItem = true;
    ctx.isOverLayoutSplitter = true;
    ctx.isDrawingMode = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Grabbing, "isDragging overrides all -> Grabbing");
  }

  // Test 4: isOverLayoutSplitter (horizontal) -> ResizeV
  {
    dc::CursorHintContext ctx{};
    ctx.isOverLayoutSplitter = true;
    ctx.splitterVertical = false;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::ResizeV, "horizontal splitter -> ResizeV");
  }

  // Test 5: isOverLayoutSplitter (vertical) -> ResizeH
  {
    dc::CursorHintContext ctx{};
    ctx.isOverLayoutSplitter = true;
    ctx.splitterVertical = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::ResizeH, "vertical splitter -> ResizeH");
  }

  // Test 6: Splitter overrides drawItem
  {
    dc::CursorHintContext ctx{};
    ctx.isOverLayoutSplitter = true;
    ctx.isOverDrawItem = true;
    ctx.splitterVertical = false;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::ResizeV, "splitter overrides drawItem");
  }

  // Test 7: isDrawingMode -> Crosshair
  {
    dc::CursorHintContext ctx{};
    ctx.isDrawingMode = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Crosshair, "drawingMode -> Crosshair");
  }

  // Test 8: isMeasuring -> Crosshair
  {
    dc::CursorHintContext ctx{};
    ctx.isMeasuring = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Crosshair, "measuring -> Crosshair");
  }

  // Test 9: drawingMode overrides drawItem
  {
    dc::CursorHintContext ctx{};
    ctx.isDrawingMode = true;
    ctx.isOverDrawItem = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Crosshair, "drawingMode overrides drawItem");
  }

  // Test 10: measuring overrides drawItem
  {
    dc::CursorHintContext ctx{};
    ctx.isMeasuring = true;
    ctx.isOverDrawItem = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Crosshair, "measuring overrides drawItem");
  }

  // Test 11: isDragging overrides splitter
  {
    dc::CursorHintContext ctx{};
    ctx.isDragging = true;
    ctx.isOverLayoutSplitter = true;
    check(dc::CursorHintProvider::resolve(ctx) == dc::CursorHint::Grabbing, "dragging overrides splitter");
  }

  std::printf("=== D34.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
