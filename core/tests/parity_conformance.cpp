// ENC-501 (P5 cutover) — Dawn-only golden conformance suite.
//
// Originally (ENC-510) this rendered each scene through BOTH the GL backend
// (Renderer + OsMesaContext) and the Dawn backend (DawnSceneRenderer) and compared
// the two readbacks pixel-by-pixel — GL was the reference. Dawn is now the proven
// default renderer and dc_gl is being deleted (ENC-501), so this is converted to
// render ONLY via Dawn and assert the readback against captured GOLDEN probe
// pixels. Those goldens ARE the GL-validated Dawn output: the GL<->Dawn parity
// suite passed (Dawn matched GL within tolerance) while dc_gl still existed, so the
// current Dawn pixels are the reference. We bake a representative set of probe
// pixels per scenario (the same probe-pixel style as d2_3_dawn_pipelines /
// d29_1_dawn_blend) — solid interiors are asserted, AA fringe positions avoided.
//
// SCENARIO COVERAGE (unchanged from ENC-510 — 17 scenarios):
//   render-core/triSolid, transforms/scale+translate, pipelines/instRect-sharp,
//   scissor/pane-region, clip/stencil-mask, cull/frustum, blend/{normal,additive,
//   multiply,screen}, gradient/per-vertex-color, aa/{triAA,rounded-rect,
//   lineAA-solid,lineAA-dashed}, volume/candles, pipelines/line2d-1px.
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_golden.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using dc::golden::BufferData;
using dc::golden::GoldenFrame;
using dc::golden::renderDawn;
using dc::golden::SceneBuilder;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;
static bool g_capture = false;

// One golden probe: at on-screen (x,y) the Dawn readback must equal (r,g,b)
// within `tol` per channel. In capture mode we print the ACTUAL pixel instead of
// asserting, so the golden table can be regenerated if the Dawn output ever moves.
struct Probe {
  int x, y;
  int r, g, b;
  int tol;
};

static void runScene(const char* name, const SceneBuilder& b, int W, int H,
                     const std::vector<Probe>& probes) {
  GoldenFrame f = renderDawn(name, b, W, H);
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

// --- Scene builders (identical to the ENC-510 parity scenes). ----------------

// render core: a solid red triangle centered (D2.1).
static std::vector<BufferData> sceneTriSolid(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float tri[] = {-0.6f, -0.6f, 0.6f, -0.6f, 0.0f, 0.6f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  return {BufferData(10, tri, sizeof(tri))};
}

// transforms: same triangle scaled+translated via an attached transform (D2.2).
static std::vector<BufferData> sceneTransform(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float tri[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"createTransform","id":6})");
  cp.applyJsonText(R"({"cmd":"setTransform","id":6,"sx":0.4,"sy":0.4,"tx":0.4,"ty":0.4})");
  cp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":3,"transformId":6})");
  return {BufferData(10, tri, sizeof(tri))};
}

// pipelines: a 1px line2d (D2.3). 1px primitives are the hardest to land exactly.
static std::vector<BufferData> sceneLinePoints(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float line[] = {-0.8f, -0.4f, 0.8f, 0.4f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":2,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"line2d@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":0,"a":1})");
  return {BufferData(10, line, sizeof(line))};
}

// instancedRect sharp corners: byte-exact solid fill.
static std::vector<BufferData> sceneRectSharp(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rect[] = {-0.5f, -0.5f, 0.5f, 0.5f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":0.5,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})");
  return {BufferData(10, rect, sizeof(rect))};
}

// rounded rect: SDF corner falloff. AA fringe at the 4 corners; interior flat.
static std::vector<BufferData> sceneRectRounded(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rect[] = {-0.6f, -0.6f, 0.6f, 0.6f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":18})");
  return {BufferData(10, rect, sizeof(rect))};
}

// gradient: per-vertex-color triangle (smooth interpolation).
static std::vector<BufferData> sceneGradient(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float verts[] = {
      -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  // red BL
       1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // green BR
       0.0f,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f,  // blue top
  };
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_color4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triGradient@1","geometryId":100})");
  return {BufferData(10, verts, sizeof(verts))};
}

// triAA: per-vertex-alpha triangle. Interior alpha=1 is a flat fill.
static std::vector<BufferData> sceneTriAA(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float verts[] = {
      -0.6f, -0.6f, 1.0f,
       0.6f, -0.6f, 1.0f,
       0.0f,  0.6f, 1.0f,
  };
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":36})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_alpha"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triAA@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":1,"a":1})");
  return {BufferData(10, verts, sizeof(verts))};
}

// scissor/clip: a pane whose clip region covers only the LEFT half (D9.2).
static std::vector<BufferData> sceneScissor(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipXMin":-1.0,"clipYMin":-1.0,"clipXMax":0.0,"clipYMax":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rect[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  return {BufferData(10, rect, sizeof(rect))};
}

// stencil clipping: a clip-source rect masks a larger content rect (D29.2).
static std::vector<BufferData> sceneClipMask(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float clip[] = {-0.4f, -0.4f, 0.4f, 0.4f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"isClipSource":true})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})");
  float content[] = {-0.9f, -0.9f, 0.9f, 0.9f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":4,"useClipMask":true})");
  return {BufferData(10, clip, sizeof(clip)), BufferData(11, content, sizeof(content))};
}

// Build a blend scenario for a given mode (D29.1).
static SceneBuilder makeBlendScene(const char* mode) {
  std::string m(mode);
  return [m](dc::CommandProcessor& cp, dc::Scene&) -> std::vector<BufferData> {
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
    float base[] = {-0.7f, -0.7f, 0.7f, 0.7f};
    cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
    cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
    cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":0,"b":1,"a":1})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})");
    float over[] = {-0.5f, -0.5f, 0.5f, 0.5f};
    cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":16})");
    cp.applyJsonText(
        R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})");
    cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":4,"r":1,"g":0,"b":0,"a":0.5})");
    cp.applyJsonText(std::string(R"({"cmd":"setDrawItemStyle","drawItemId":4,"blendMode":")") +
                     m + R"("})");
    return {BufferData(10, base, sizeof(base)), BufferData(11, over, sizeof(over))};
  };
}

// lineAA solid: a thick anti-aliased line. Core is a flat fill.
static std::vector<BufferData> sceneLineAASolid(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float seg[] = {-0.8f, 0.0f, 0.8f, 0.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"lineAA@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":1,"a":1})");
  cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"lineWidth":12,"dashLength":0,"gapLength":0})");
  return {BufferData(10, seg, sizeof(seg))};
}

// lineAA dashed: a thick dashed line. Dash on/off + AA fringe.
static std::vector<BufferData> sceneLineAADashed(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float seg[] = {-0.8f, 0.0f, 0.8f, 0.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"lineAA@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"lineWidth":10,"dashLength":16,"gapLength":16})");
  return {BufferData(10, seg, sizeof(seg))};
}

// volume/candle: a row of OHLC candles, up=green down=red, with wicks (D14.6).
static std::vector<BufferData> sceneCandles(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float candles[] = {
      -0.5f, -0.3f, 0.7f, -0.7f,  0.3f, 0.12f,  // UP
       0.0f,  0.3f, 0.7f, -0.7f, -0.3f, 0.12f,  // DOWN
       0.5f, -0.3f, 0.7f, -0.7f,  0.3f, 0.12f,  // UP
  };
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"candle6"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedCandle@1","geometryId":100})");
  cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
      R"("colorUpR":0,"colorUpG":1,"colorUpB":0,"colorUpA":1,)"
      R"("colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})");
  return {BufferData(10, candles, sizeof(candles))};
}

// frustum cull: a transformed item fully outside the pane clip region is culled
// -> a clear (black) frame (D10.5).
static std::vector<BufferData> sceneFrustumCull(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float tri[] = {-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"createTransform","id":6})");
  cp.applyJsonText(R"({"cmd":"setTransform","id":6,"sx":1,"sy":1,"tx":5.0,"ty":0})");
  cp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":3,"transformId":6})");
  return {BufferData(10, tri, sizeof(tri))};
}

int main(int argc, char** argv) {
  if ((argc > 1 && std::strcmp(argv[1], "--capture") == 0) ||
      std::getenv("DC_GOLDEN_CAPTURE"))
    g_capture = true;

  std::printf("=== ENC-501 Dawn-only golden conformance ===\n");
  constexpr int W = 96, H = 96;
  // Center of a 96x96 frame is (48,48). Solid scenes are probed at the centroid /
  // interior + a clear corner; AA scenes only at flat-core interior pixels.
  const int sTol = 10;  // solid interior tolerance
  const int aTol = 24;  // gradient / blended interior tolerance

  // render core — red triangle centroid; clear top corner.
  runScene("render-core/triSolid", sceneTriSolid, W, H,
           {{48, 56, 255, 0, 0, sTol}, {4, 4, 0, 0, 0, sTol}, {90, 4, 0, 0, 0, sTol}});
  // transforms — scaled+translated green triangle. Clip tri scaled 0.4 +trans
  // (0.4,0.4) => clip verts (0.2,0.2)(0.6,0.2)(0.4,0.6). In the readback (clip +y
  // -> bottom rows) the body lands around (65,62); clear at the top-left corner.
  runScene("transforms/scale+translate", sceneTransform, W, H,
           {{65, 62, 0, 255, 0, sTol}, {4, 6, 0, 0, 0, sTol}});
  // instancedRect sharp — blue (0,128,255) fill center; clear corners.
  runScene("pipelines/instRect-sharp", sceneRectSharp, W, H,
           {{48, 48, 0, 128, 255, sTol}, {4, 4, 0, 0, 0, sTol}, {91, 91, 0, 0, 0, sTol}});
  // scissor — white rect only in left half; right half clear.
  runScene("scissor/pane-region", sceneScissor, W, H,
           {{20, 48, 255, 255, 255, sTol}, {76, 48, 0, 0, 0, sTol}});
  // stencil clip-mask — green content visible only inside the clip-source square.
  runScene("clip/stencil-mask", sceneClipMask, W, H,
           {{48, 48, 0, 255, 0, sTol}, {10, 48, 0, 0, 0, sTol}});
  // frustum cull — whole frame clear (item culled off-screen).
  runScene("cull/frustum", sceneFrustumCull, W, H,
           {{48, 48, 0, 0, 0, sTol}, {20, 20, 0, 0, 0, sTol}});

  // blend modes — composited center color over blue base.
  runScene("blend/normal", makeBlendScene("normal"), W, H,
           {{48, 48, 128, 0, 128, aTol}});
  runScene("blend/additive", makeBlendScene("additive"), W, H,
           {{48, 48, 128, 0, 255, aTol}});
  // multiply: red(255,0,0)*blue base(0,0,255) -> 0 in every channel.
  runScene("blend/multiply", makeBlendScene("multiply"), W, H,
           {{48, 48, 0, 0, 0, aTol}});
  // screen: 1-(1-red)(1-blue base) -> R=255, B=255 over the overlap.
  runScene("blend/screen", makeBlendScene("screen"), W, H,
           {{48, 48, 255, 0, 255, aTol}});

  // gradient — corners trend to their vertex colors; center is a mix.
  runScene("gradient/per-vertex-color", sceneGradient, W, H,
           {{48, 48, 64, 64, 128, aTol}});

  // AA scenes — assert only flat-core interior pixels (avoid fringe).
  runScene("aa/triAA", sceneTriAA, W, H,
           {{48, 56, 255, 0, 255, sTol}});
  runScene("aa/rounded-rect", sceneRectRounded, W, H,
           {{48, 48, 255, 0, 0, sTol}, {4, 4, 0, 0, 0, sTol}});
  runScene("aa/lineAA-solid", sceneLineAASolid, W, H,
           {{48, 48, 0, 255, 255, sTol}});
  runScene("aa/lineAA-dashed", sceneLineAADashed, W, H,
           {{48, 48, 255, 255, 255, sTol}});
  // volume/candles — center candle body (down=red) interior.
  runScene("volume/candles", sceneCandles, W, H,
           {{48, 48, 255, 0, 0, sTol}});
  // 1px line2d — assert along the line midpoint with a wide tol (thin primitive).
  runScene("pipelines/line2d-1px", sceneLinePoints, W, H,
           {{48, 48, 255, 255, 0, 40}});

  if (g_capture) { std::printf("=== capture complete ===\n"); return 0; }
  std::printf("\n=== golden conformance: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  return g_failed > 0 ? 1 : 0;
}
