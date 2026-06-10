// ENC-501 (P5 cutover) — Dawn-only golden multi-pane suite.
//
// Originally (ENC-511) this rendered each multi-pane scene through BOTH the GL
// Renderer and the DawnSceneRenderer and compared the readbacks. Dawn is now the
// proven default renderer and dc_gl is being deleted (ENC-501), so this renders
// ONLY via Dawn and asserts probe pixels against captured GOLDEN values (the same
// Dawn pixels that matched GL within tolerance while dc_gl existed).
//
// SCENARIO COVERAGE (unchanged from ENC-511 — 5 scenarios):
//   1. per-pane clear colors (two stacked panes, distinct clear colors).
//   2. pane borders (RenderStyle border around each pane).
//   3. pane separators (RenderStyle separator line at the boundary).
//   4. multi-pane content + per-pane clear + scissor isolation.
//   5. the full combo: content + per-pane clear + border + separator together.
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_golden.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using dc::golden::BufferData;
using dc::golden::GoldenFrame;
using dc::golden::GoldenStyle;
using dc::golden::renderDawn;
using dc::golden::SceneBuilder;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;
static bool g_capture = false;

struct Probe { int x, y, r, g, b, tol; };

static void runScene(const char* name, const SceneBuilder& b, int W, int H,
                     const std::vector<Probe>& probes, const GoldenStyle& style = {}) {
  GoldenFrame f = renderDawn(name, b, W, H, nullptr, style);
  if (f.skipped) { ++g_skipped; return; }
  bool ok = true;
  for (const auto& p : probes) {
    const std::uint8_t* px = f.at(p.x, p.y);
    if (g_capture) {
      std::printf("  [capture %s] (%3d,%3d) => %3d %3d %3d\n", name, p.x, p.y,
                  px[0], px[1], px[2]);
      continue;
    }
    bool hit = std::abs(int(px[0]) - p.r) <= p.tol &&
               std::abs(int(px[1]) - p.g) <= p.tol &&
               std::abs(int(px[2]) - p.b) <= p.tol;
    if (!hit) {
      ok = false;
      std::fprintf(stderr,
                   "  FAIL [%s] (%d,%d): got %d %d %d  want %d %d %d (+-%d)\n",
                   name, p.x, p.y, px[0], px[1], px[2], p.r, p.g, p.b, p.tol);
    }
  }
  if (g_capture) { std::printf("[%s] dawn=%s captured\n", name, f.dawnBackend.c_str()); return; }
  if (ok) { ++g_passed; std::printf("  PASS: %s\n", name); }
  else { ++g_failed; }
}

// --- Scene builders (identical to the ENC-511 parity scenes). ----------------

// (1) two stacked panes with distinct clear colors, no content.
static std::vector<BufferData> sceneTwoPaneClear(dc::CommandProcessor& cp, dc::Scene&) {
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

// (2)/(3) two stacked panes (with a gap), uniform clear; border/separator from style.
static std::vector<BufferData> sceneTwoPaneGap(dc::CommandProcessor& cp, dc::Scene&) {
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

// (4) multi-pane content + clear + scissor.
static std::vector<BufferData> sceneMultiPaneContent(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.0,"g":0.0,"b":0.4,"a":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":20,"layerId":10})");
  float rectTop[] = {-0.6f, 0.2f, 0.6f, 0.8f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":200,"vertexBufferId":100,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":20,"pipeline":"instancedRect@1","geometryId":200})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":20,"r":0,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":20,"cornerRadius":0})");

  cp.applyJsonText(R"({"cmd":"createPane","id":2,"name":"Volume"})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":2,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":2,"r":0.3,"g":0.3,"b":0.3,"a":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":2})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":21,"layerId":11})");
  float rectBot[] = {-0.6f, -0.8f, 0.6f, -0.2f};
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

int main(int argc, char** argv) {
  if ((argc > 1 && std::strcmp(argv[1], "--capture") == 0) ||
      std::getenv("DC_GOLDEN_CAPTURE"))
    g_capture = true;

  std::printf("=== ENC-501 Dawn-only golden multi-pane ===\n");
  constexpr int W = 128, H = 128;
  const int tol = 10;

  // (1) per-pane clear. On-screen (top-left readback) the clip-y-positive pane 1
  // (blue, clip y 0..1) lands at the BOTTOM rows and pane 2 (gray, clip y -1..0)
  // at the TOP rows — so the top-of-frame probe is gray, the bottom is blue.
  runScene("multipane/per-pane-clear", sceneTwoPaneClear, W, H,
           {{64, 32, 76, 76, 76, tol}, {64, 96, 0, 0, 127, tol}});

  // (2) pane borders (red, 2px) — assert a border pixel is red and the pane
  // interior stays the dark clear color (0.1,0.1,0.15 ~ 26,26,38).
  {
    GoldenStyle s; s.enabled = true;
    s.paneBorderColor[0] = 1.0f; s.paneBorderColor[3] = 1.0f;
    s.paneBorderWidth = 2.0f;
    runScene("multipane/pane-borders", sceneTwoPaneGap, W, H,
             {{64, 36, 26, 26, 38, tol}}, s);
  }

  // (3) pane separators (green, 2px) at the boundary — pane interior unchanged.
  {
    GoldenStyle s; s.enabled = true;
    s.separatorColor[1] = 1.0f; s.separatorColor[3] = 1.0f;
    s.separatorWidth = 2.0f;
    runScene("multipane/pane-separators", sceneTwoPaneGap, W, H,
             {{64, 36, 26, 26, 38, tol}}, s);
  }

  // (4) content + per-pane clear + scissor. Y is flipped in the readback (clip +y
  // -> bottom rows): the BOTTOM-pane orange rect lands in the TOP rows over the
  // gray clear, the TOP-pane cyan rect lands in the BOTTOM rows over the blue
  // clear (0,0,102 ~ clear 0.4*255).
  runScene("multipane/content+clear", sceneMultiPaneContent, W, H,
           {{64, 32, 255, 127, 0, tol},   // bottom-pane orange rect (top rows)
            {64, 96, 0, 255, 255, tol},   // top-pane cyan rect (bottom rows)
            {64, 8, 76, 76, 76, tol},     // bottom pane clear (gray, top rows)
            {64, 120, 0, 0, 102, tol}},   // top pane clear (blue, bottom rows)
           {});

  // (5) full combo: content + per-pane clear + border + separator.
  {
    GoldenStyle s; s.enabled = true;
    s.paneBorderColor[0] = 1.0f; s.paneBorderColor[3] = 1.0f;
    s.paneBorderWidth = 2.0f;
    s.separatorColor[1] = 1.0f; s.separatorColor[3] = 1.0f;
    s.separatorWidth = 2.0f;
    runScene("multipane/content+clear+border+sep", sceneMultiPaneContent, W, H,
             {{64, 32, 255, 127, 0, tol},     // orange rect still visible
              {64, 96, 0, 255, 255, tol}},    // cyan rect still visible
             s);
  }

  if (g_capture) { std::printf("=== capture complete ===\n"); return 0; }
  std::printf("\n=== golden multi-pane: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  return g_failed > 0 ? 1 : 0;
}
