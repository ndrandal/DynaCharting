// ENC-511 (P5.0c) — GL <-> Dawn multi-pane parity suite.
//
// Proves the DawnSceneRenderer multi-pane features added in ENC-511 match the GL
// Renderer pixel-for-pixel (within the harness tolerance, see parity_harness.hpp):
//   1. PER-PANE CLEAR COLORS (D10.4 pattern, d10_4_pane_clear.cpp): two stacked
//      panes with distinct clear colors; each pane region must be cleared to its
//      color, the gap between them stays the frame-clear black.
//   2. PANE BORDERS (D78.3 pattern, d78_3_pane_borders.cpp): two panes with a
//      RenderStyle border; the border edge rects appear around each pane.
//   3. PANE SEPARATORS (D78.3 pattern): two panes with a RenderStyle separator;
//      a line appears at the boundary between them.
//   4. MULTI-PANE CONTENT + CLEAR (d9_6_multi_pane.cpp pattern): two panes with
//      distinct clear colors AND distinct content (a rect each), scissored to
//      their region — content + clear + scissor isolation all at once.
//
// The SAME Scene (identical CommandProcessor JSON) and the SAME ParityStyle are
// fed to BOTH backends; the harness renders each and diffs the RGBA readbacks.
//
// TOLERANCE: per-pane clear interiors are flat solid fills (near-exact). Borders
// and separators are thin (1-2px) features that GL draws as wide GL_LINES and
// Dawn draws as thin filled rects; their exact pixel coverage can differ by a
// row/col between the two rasterizers, so those areas carry a small mismatch%
// budget (the same philosophy as the 1px-primitive area in parity_conformance).
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_harness.hpp"

#include <cstdio>
#include <vector>

using dc::parity::BufferData;
using dc::parity::compareScene;
using dc::parity::ParityResult;
using dc::parity::ParityStyle;
using dc::parity::Tolerance;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;

static void run(const char* area, const dc::parity::SceneBuilder& b, int W, int H,
                Tolerance tol, const ParityStyle& style = {}) {
  ParityResult r = compareScene(area, b, W, H, tol, nullptr, style);
  if (r.skipped) {
    ++g_skipped;
    return;
  }
  if (r.passed) ++g_passed;
  else ++g_failed;
}

// --- Scene builders ---------------------------------------------------------

// (1) two stacked panes with distinct clear colors, no content (d10_4 pattern).
//   top    = upper half, dark blue clear.
//   bottom = lower half, dark gray clear.
static std::vector<BufferData> sceneTwoPaneClear(dc::CommandProcessor& cp,
                                                 dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.0,"g":0.0,"b":0.5,"a":1.0})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.3,"g":0.3,"b":0.3,"a":1.0})");
  return {};
}

// (2)/(3) two stacked panes (with a gap) and uniform clear (d78_3 pattern). The
//   border/separator come from the ParityStyle, applied identically to both
//   backends. Gap (between 0.05 and -0.05 in clip y) stays the frame black.
static std::vector<BufferData> sceneTwoPaneGap(dc::CommandProcessor& cp,
                                               dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Top"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.05,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.1,"g":0.1,"b":0.15,"a":1})");
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Bottom"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-0.95,"clipYMax":-0.05,"clipXMin":-0.95,"clipXMax":0.95})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.1,"g":0.1,"b":0.15,"a":1})");
  return {};
}

// (4) multi-pane content + clear + scissor (d9_6 pattern, simplified to a
//   deterministic static scene): two panes, each cleared to its own color and
//   holding a full-region rect of a distinct content color (scissored to its
//   pane). Exercises per-pane clear, scissor isolation, and content together.
static std::vector<BufferData> sceneMultiPaneContent(dc::CommandProcessor& cp,
                                                     dc::Scene&) {
  // Top pane: dark blue clear, a centered cyan rect.
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.0,"g":0.0,"b":0.4,"a":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":20,"layerId":10})");
  float rectTop[] = {-0.6f, 0.2f, 0.6f, 0.8f};  // rect4 inside the top pane
  cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":100,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":20,"pipeline":"instancedRect@1","geometryId":200})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":20,"r":0,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":20,"cornerRadius":0})");

  // Bottom pane: dark gray clear, a centered orange rect.
  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Volume"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.3,"g":0.3,"b":0.3,"a":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":2})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":21,"layerId":11})");
  float rectBot[] = {-0.6f, -0.8f, 0.6f, -0.2f};  // rect4 inside the bottom pane
  cp.applyJsonText(R"({"cmd":"createBuffer","id":101,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":201,"vertexBufferId":101,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":21,"pipeline":"instancedRect@1","geometryId":201})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":21,"r":1,"g":0.5,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":21,"cornerRadius":0})");

  return {BufferData(100, rectTop, sizeof(rectTop)),
          BufferData(101, rectBot, sizeof(rectBot))};
}

int main() {
  std::printf("=== ENC-511 GL<->Dawn multi-pane parity ===\n");
  constexpr int W = 128, H = 128;

  // Per-pane clear interiors are flat solid fills — near-exact. A thin band of
  // pixels at the pane boundary / scissor edge may differ between rasterizers.
  Tolerance clearTol;   clearTol.channelTol = 8;  clearTol.maxMismatchPct = 4.0;
  // Borders/separators: thin (wide-line in GL vs thin-rect in Dawn) — allow a
  // few percent of edge pixels to differ (same budget as 1px primitives).
  Tolerance thinTol;    thinTol.channelTol = 40; thinTol.maxMismatchPct = 10.0;
  // Content + clear + scissor: solid fills, a few edge pixels allowed.
  Tolerance contentTol; contentTol.channelTol = 12; contentTol.maxMismatchPct = 6.0;

  // (1) two panes, distinct clear colors.
  run("multipane/per-pane-clear", sceneTwoPaneClear, W, H, clearTol);

  // (2) panes with borders (red, 2px).
  {
    ParityStyle s;
    s.enabled = true;
    s.paneBorderColor[0] = 1.0f; s.paneBorderColor[3] = 1.0f;
    s.paneBorderWidth = 2.0f;
    run("multipane/pane-borders", sceneTwoPaneGap, W, H, thinTol, s);
  }

  // (3) panes with a separator (green, 2px) at the boundary.
  {
    ParityStyle s;
    s.enabled = true;
    s.separatorColor[1] = 1.0f; s.separatorColor[3] = 1.0f;
    s.separatorWidth = 2.0f;
    run("multipane/pane-separators", sceneTwoPaneGap, W, H, thinTol, s);
  }

  // (4) multi-pane content + per-pane clear + scissor isolation.
  run("multipane/content+clear", sceneMultiPaneContent, W, H, contentTol);

  // (5) the full combo: content + per-pane clear + border + separator together.
  {
    ParityStyle s;
    s.enabled = true;
    s.paneBorderColor[0] = 1.0f; s.paneBorderColor[3] = 1.0f;
    s.paneBorderWidth = 2.0f;
    s.separatorColor[1] = 1.0f; s.separatorColor[3] = 1.0f;
    s.separatorWidth = 2.0f;
    run("multipane/content+clear+border+sep", sceneMultiPaneContent, W, H,
        thinTol, s);
  }

  std::printf("\n=== multi-pane parity: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  return g_failed > 0 ? 1 : 0;
}
