// D24.1 -- MinimapState + MinimapRecipe test

#include "dc/minimap/MinimapState.hpp"
#include "dc/recipe/MinimapRecipe.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(float a, float b, float tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f, tol %.6f)\n", msg, a, b, tol);
    std::exit(1);
  }
}

int main() {
  // ---- Test 1: MinimapState setFullRange + setViewport -> viewWindow normalized ----
  {
    dc::MinimapState state;
    state.setFullRange(0, 1000, 0, 100);
    state.setViewport(200, 400, 25, 75);

    auto w = state.viewWindow();
    requireClose(w.x0, 0.2f, 0.001f, "T1 x0 = 0.2");
    requireClose(w.x1, 0.4f, 0.001f, "T1 x1 = 0.4");
    requireClose(w.y0, 0.25f, 0.001f, "T1 y0 = 0.25");
    requireClose(w.y1, 0.75f, 0.001f, "T1 y1 = 0.75");

    std::printf("  Test 1 (viewWindow normalized): PASS\n");
  }

  // ---- Test 2: Viewport equals full range -> window is (0,0,1,1) ----
  {
    dc::MinimapState state;
    state.setFullRange(0, 500, 0, 200);
    state.setViewport(0, 500, 0, 200);

    auto w = state.viewWindow();
    requireClose(w.x0, 0.0f, 0.001f, "T2 x0 = 0");
    requireClose(w.x1, 1.0f, 0.001f, "T2 x1 = 1");
    requireClose(w.y0, 0.0f, 0.001f, "T2 y0 = 0");
    requireClose(w.y1, 1.0f, 0.001f, "T2 y1 = 1");

    std::printf("  Test 2 (full range -> 0,0,1,1): PASS\n");
  }

  // ---- Test 3: Viewport is left half -> window is (0,0,0.5,1) ----
  {
    dc::MinimapState state;
    state.setFullRange(0, 1000, 0, 100);
    state.setViewport(0, 500, 0, 100);

    auto w = state.viewWindow();
    requireClose(w.x0, 0.0f, 0.001f, "T3 x0 = 0");
    requireClose(w.x1, 0.5f, 0.001f, "T3 x1 = 0.5");
    requireClose(w.y0, 0.0f, 0.001f, "T3 y0 = 0");
    requireClose(w.y1, 1.0f, 0.001f, "T3 y1 = 1");

    std::printf("  Test 3 (left half -> 0,0,0.5,1): PASS\n");
  }

  // ---- Test 4: hitTest inside/outside window ----
  {
    dc::MinimapState state;
    state.setFullRange(0, 1000, 0, 100);
    state.setViewport(200, 400, 25, 75);
    // Window is (0.2, 0.25, 0.4, 0.75)

    requireTrue(state.hitTest(0.3f, 0.5f), "T4 inside hit");
    requireTrue(state.hitTest(0.2f, 0.25f), "T4 corner hit");
    requireTrue(state.hitTest(0.4f, 0.75f), "T4 far corner hit");
    requireTrue(!state.hitTest(0.1f, 0.5f), "T4 left miss");
    requireTrue(!state.hitTest(0.5f, 0.5f), "T4 right miss");
    requireTrue(!state.hitTest(0.3f, 0.1f), "T4 below miss");
    requireTrue(!state.hitTest(0.3f, 0.9f), "T4 above miss");

    std::printf("  Test 4 (hitTest): PASS\n");
  }

  // ---- Test 5: dragTo centers viewport ----
  {
    dc::MinimapState state;
    state.setFullRange(0, 1000, 0, 100);
    state.setViewport(200, 400, 25, 75);
    // View width = 200, view height = 50

    double vxMin, vxMax, vyMin, vyMax;
    // Drag to center of full range (0.5, 0.5)
    state.dragTo(0.5f, 0.5f, vxMin, vxMax, vyMin, vyMax);

    // Center at data (500, 50), half-widths (100, 25)
    requireClose(static_cast<float>(vxMin), 400.0f, 0.1f, "T5 vxMin = 400");
    requireClose(static_cast<float>(vxMax), 600.0f, 0.1f, "T5 vxMax = 600");
    requireClose(static_cast<float>(vyMin), 25.0f, 0.1f, "T5 vyMin = 25");
    requireClose(static_cast<float>(vyMax), 75.0f, 0.1f, "T5 vyMax = 75");

    std::printf("  Test 5 (dragTo): PASS\n");
  }

  // ---- Test 6: MinimapRecipe build() creates 9 scene objects ----
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");

    dc::MinimapRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "minimap";

    dc::MinimapRecipe recipe(2000, cfg);
    auto build = recipe.build();

    for (auto& cmd : build.createCommands) {
      auto r = cp.applyJsonText(cmd);
      requireTrue(r.ok, ("build cmd ok: " + cmd).c_str());
    }

    // 3 buffers (track=2000, window=2003, border=2006)
    requireTrue(scene.hasBuffer(2000), "T6 track buffer");
    requireTrue(scene.hasBuffer(2003), "T6 window buffer");
    requireTrue(scene.hasBuffer(2006), "T6 border buffer");

    // 3 geometries (track=2001, window=2004, border=2007)
    requireTrue(scene.hasGeometry(2001), "T6 track geom");
    requireTrue(scene.hasGeometry(2004), "T6 window geom");
    requireTrue(scene.hasGeometry(2007), "T6 border geom");

    // 3 draw items (track=2002, window=2005, border=2008)
    requireTrue(scene.hasDrawItem(2002), "T6 track DI");
    requireTrue(scene.hasDrawItem(2005), "T6 window DI");
    requireTrue(scene.hasDrawItem(2008), "T6 border DI");

    // drawItemIds returns all 3
    auto ids = recipe.drawItemIds();
    requireTrue(ids.size() == 3, "T6 drawItemIds count = 3");
    requireTrue(ids[0] == 2002, "T6 drawItemIds[0] = track");
    requireTrue(ids[1] == 2005, "T6 drawItemIds[1] = window");
    requireTrue(ids[2] == 2008, "T6 drawItemIds[2] = border");

    std::printf("  Test 6 (build creates 9 objects): PASS\n");
  }

  // ---- Test 7: computeMinimap produces correct rects and borders ----
  {
    dc::MinimapRecipeConfig cfg;
    dc::MinimapRecipe recipe(3000, cfg);

    // View window covering left 30% horizontally, bottom 40% vertically
    dc::MinimapViewWindow window;
    window.x0 = 0.0f;
    window.x1 = 0.3f;
    window.y0 = 0.0f;
    window.y1 = 0.4f;

    // Clip region: [-0.9, -0.9] to [0.9, 0.9]
    auto data = recipe.computeMinimap(window, -0.9f, -0.9f, 0.9f, 0.9f);

    // Track should cover full clip region
    requireClose(data.trackRect[0], -0.9f, 0.001f, "T7 track x0");
    requireClose(data.trackRect[1], -0.9f, 0.001f, "T7 track y0");
    requireClose(data.trackRect[2], 0.9f, 0.001f, "T7 track x1");
    requireClose(data.trackRect[3], 0.9f, 0.001f, "T7 track y1");

    // Window: regionW = 1.8, regionH = 1.8
    // wx0 = -0.9 + 0.0 * 1.8 = -0.9
    // wx1 = -0.9 + 0.3 * 1.8 = -0.36
    // wy0 = -0.9 + 0.0 * 1.8 = -0.9
    // wy1 = -0.9 + 0.4 * 1.8 = -0.18
    requireClose(data.windowRect[0], -0.9f, 0.001f, "T7 win x0");
    requireClose(data.windowRect[1], -0.9f, 0.001f, "T7 win y0");
    requireClose(data.windowRect[2], -0.36f, 0.001f, "T7 win x1");
    requireClose(data.windowRect[3], -0.18f, 0.001f, "T7 win y1");

    // 4 border segments
    requireTrue(data.borderCount == 4, "T7 border count = 4");
    requireTrue(data.borderLines.size() == 16, "T7 border 16 floats");

    // Bottom line: (wx0,wy0) -> (wx1,wy0) = (-0.9,-0.9) -> (-0.36,-0.9)
    requireClose(data.borderLines[0], -0.9f, 0.001f, "T7 bottom x0");
    requireClose(data.borderLines[1], -0.9f, 0.001f, "T7 bottom y0");
    requireClose(data.borderLines[2], -0.36f, 0.001f, "T7 bottom x1");
    requireClose(data.borderLines[3], -0.9f, 0.001f, "T7 bottom y1");

    // Top line: (wx0,wy1) -> (wx1,wy1) = (-0.9,-0.18) -> (-0.36,-0.18)
    requireClose(data.borderLines[4], -0.9f, 0.001f, "T7 top x0");
    requireClose(data.borderLines[5], -0.18f, 0.001f, "T7 top y0");
    requireClose(data.borderLines[6], -0.36f, 0.001f, "T7 top x1");
    requireClose(data.borderLines[7], -0.18f, 0.001f, "T7 top y1");

    // Left line: (wx0,wy0) -> (wx0,wy1) = (-0.9,-0.9) -> (-0.9,-0.18)
    requireClose(data.borderLines[8], -0.9f, 0.001f, "T7 left x0");
    requireClose(data.borderLines[9], -0.9f, 0.001f, "T7 left y0");
    requireClose(data.borderLines[10], -0.9f, 0.001f, "T7 left x1");
    requireClose(data.borderLines[11], -0.18f, 0.001f, "T7 left y1");

    // Right line: (wx1,wy0) -> (wx1,wy1) = (-0.36,-0.9) -> (-0.36,-0.18)
    requireClose(data.borderLines[12], -0.36f, 0.001f, "T7 right x0");
    requireClose(data.borderLines[13], -0.9f, 0.001f, "T7 right y0");
    requireClose(data.borderLines[14], -0.36f, 0.001f, "T7 right x1");
    requireClose(data.borderLines[15], -0.18f, 0.001f, "T7 right y1");

    std::printf("  Test 7 (computeMinimap rects + borders): PASS\n");
  }

  // ---- Test 8: computeMinimap with full viewport -> window covers entire track ----
  {
    dc::MinimapRecipeConfig cfg;
    dc::MinimapRecipe recipe(4000, cfg);

    dc::MinimapViewWindow fullWindow;
    fullWindow.x0 = 0.0f;
    fullWindow.y0 = 0.0f;
    fullWindow.x1 = 1.0f;
    fullWindow.y1 = 1.0f;

    auto data = recipe.computeMinimap(fullWindow, -0.8f, -0.2f, 0.8f, 0.2f);

    // Window should match track exactly
    requireClose(data.windowRect[0], data.trackRect[0], 0.001f, "T8 win x0 = track x0");
    requireClose(data.windowRect[1], data.trackRect[1], 0.001f, "T8 win y0 = track y0");
    requireClose(data.windowRect[2], data.trackRect[2], 0.001f, "T8 win x1 = track x1");
    requireClose(data.windowRect[3], data.trackRect[3], 0.001f, "T8 win y1 = track y1");

    // Verify specific values
    requireClose(data.windowRect[0], -0.8f, 0.001f, "T8 win x0 = -0.8");
    requireClose(data.windowRect[1], -0.2f, 0.001f, "T8 win y0 = -0.2");
    requireClose(data.windowRect[2], 0.8f, 0.001f, "T8 win x1 = 0.8");
    requireClose(data.windowRect[3], 0.2f, 0.001f, "T8 win y1 = 0.2");

    std::printf("  Test 8 (full viewport covers track): PASS\n");
  }

  std::printf("D24.1 minimap: ALL PASS\n");
  return 0;
}
