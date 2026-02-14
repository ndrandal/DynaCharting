// D11.1 — Extended Input + Click Detection test
// Tests: click at center → hasClick() true + clickData() ≈ (50,50),
// next frame without click → hasClick() false (one-shot),
// KeyCode enum values and defaults.

#include "dc/viewport/InputState.hpp"
#include "dc/viewport/InputMapper.hpp"
#include "dc/viewport/Viewport.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Click at center → hasClick() true, clickData() ≈ (50,50) ---
  {
    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(400, 300);

    dc::InputMapper mapper;
    mapper.setViewports({&vp});

    dc::ViewportInputState input;
    input.cursorX = 200; // center X
    input.cursorY = 150; // center Y
    input.clicked = true;

    mapper.processInput(input);

    requireTrue(mapper.hasClick(), "hasClick true after click");

    double dx, dy;
    bool ok = mapper.clickData(dx, dy);
    requireTrue(ok, "clickData returns true");
    requireTrue(std::fabs(dx - 50.0) < 1.0, "clickData X ≈ 50");
    requireTrue(std::fabs(dy - 50.0) < 1.0, "clickData Y ≈ 50");

    double px, py;
    ok = mapper.clickPixel(px, py);
    requireTrue(ok, "clickPixel returns true");
    requireTrue(std::fabs(px - 200.0) < 0.01, "clickPixel X = 200");
    requireTrue(std::fabs(py - 150.0) < 0.01, "clickPixel Y = 150");

    std::printf("  Test 1 (click center → data ≈ 50,50) PASS\n");
  }

  // --- Test 2: Next frame without click → hasClick() false (one-shot) ---
  {
    dc::Viewport vp;
    vp.setDataRange(0, 100, 0, 100);
    vp.setPixelViewport(400, 300);

    dc::InputMapper mapper;
    mapper.setViewports({&vp});

    // Frame 1: click
    dc::ViewportInputState input1;
    input1.cursorX = 200;
    input1.cursorY = 150;
    input1.clicked = true;
    mapper.processInput(input1);
    requireTrue(mapper.hasClick(), "frame 1 has click");

    // Frame 2: no click
    dc::ViewportInputState input2;
    input2.cursorX = 200;
    input2.cursorY = 150;
    input2.clicked = false;
    mapper.processInput(input2);
    requireTrue(!mapper.hasClick(), "frame 2 no click (one-shot)");

    std::printf("  Test 2 (one-shot click resets next frame) PASS\n");
  }

  // --- Test 3: KeyCode enum values and defaults ---
  {
    requireTrue(static_cast<int>(dc::KeyCode::None) == 0, "KeyCode::None == 0");
    requireTrue(static_cast<int>(dc::KeyCode::Left) == 1, "KeyCode::Left == 1");
    requireTrue(static_cast<int>(dc::KeyCode::Right) == 2, "KeyCode::Right == 2");
    requireTrue(static_cast<int>(dc::KeyCode::Up) == 3, "KeyCode::Up == 3");
    requireTrue(static_cast<int>(dc::KeyCode::Down) == 4, "KeyCode::Down == 4");
    requireTrue(static_cast<int>(dc::KeyCode::Home) == 5, "KeyCode::Home == 5");
    requireTrue(static_cast<int>(dc::KeyCode::End) == 6, "KeyCode::End == 6");

    dc::ViewportInputState state;
    requireTrue(!state.clicked, "default clicked=false");
    requireTrue(state.keyPressed == dc::KeyCode::None, "default keyPressed=None");

    std::printf("  Test 3 (KeyCode enum + defaults) PASS\n");
  }

  std::printf("D11.1 input_click: ALL PASS\n");
  return 0;
}
