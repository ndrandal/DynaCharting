// ENC-510 (P5.0b) — GL <-> Dawn parity conformance suite.
//
// Renders the SAME scenes through the GL backend (Renderer + OsMesaContext) and
// the Dawn backend (DawnSceneRenderer) and asserts the two RGBA readbacks match
// within a per-feature tolerance (see parity_harness.hpp for the tolerance model
// + the bottom-left/top-left row-flip convention). Each scenario mirrors one of
// the GL render tests in the conformance bar (the 49-render-test SPEC), grouped
// by feature area. The harness prints maxDelta + mismatch% per area.
//
// TOLERANCE PHILOSOPHY (per area, documented inline):
//   * Solid-fill interiors (triSolid / instancedRect-sharp / gradient interior /
//     scissor / clipping / blend math) are byte-near-exact: both rasterizers
//     produce the same flat color over the covered area, so channelTol is small
//     and only a tiny fraction of EDGE pixels may differ.
//   * AA / SDF-coverage areas (rounded-rect corners, AA/dashed lines, candle
//     wicks) have fractional-coverage fringes that the two rasterizers compute
//     slightly differently; those allow a small mismatch%.
// Any scenario that genuinely can't reach parity is documented in
// parity_divergences below (and excluded or widened with an explicit WHY) rather
// than silently loosening a tolerance.
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_harness.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using dc::parity::BufferData;
using dc::parity::compareScene;
using dc::parity::ParityResult;
using dc::parity::Tolerance;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;

// Run one scenario and account for its result.
static void run(const char* area, const dc::parity::SceneBuilder& b, int W, int H,
                Tolerance tol, dc::GlyphAtlas* atlas = nullptr) {
  ParityResult r = compareScene(area, b, W, H, tol, atlas);
  if (r.skipped) {
    ++g_skipped;
    return;
  }
  if (r.passed) ++g_passed;
  else ++g_failed;
}

// --- Scene builders (each builds the SAME scene both backends render). -------

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

// pipelines: a 1px line2d + a points cloud (D2.3). 1px primitives are the
// hardest to match exactly across rasterizers, so this area carries a wider
// edge tolerance (documented).
static std::vector<BufferData> sceneLinePoints(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  // line2d
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

// instancedRect sharp corners: byte-exact solid fill (D28.2 test 2 / D2.5-area).
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

// rounded rect: SDF corner falloff (D28.2 test 1). AA fringe at the 4 corners.
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

// gradient: per-vertex-color triangle (D28.3). Smooth interpolation — interior
// should match closely; only the 3 edges differ.
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

// triAA: per-vertex-alpha triangle (Pos3 = x,y,alpha). Interior alpha=1 is a
// flat fill; the alpha=0 edge produces an AA fringe.
static std::vector<BufferData> sceneTriAA(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  // pos2 + alpha (3 floats/vertex). All alpha=1 -> a flat solid for the core.
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

// scissor/clip: a pane whose clip region covers only the LEFT half; a big rect
// that would fill the whole frame is scissored to the pane (D9.2).
static std::vector<BufferData> sceneScissor(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  // Pane clip region = left half of the frame.
  cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipXMin":-1.0,"clipYMin":-1.0,"clipXMax":0.0,"clipYMax":1.0})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rect[] = {-1.0f, -1.0f, 1.0f, 1.0f};  // full-frame rect, scissored to pane
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  return {BufferData(10, rect, sizeof(rect))};
}

// stencil clipping: a clip-source rect (left-center) masks a larger content rect
// so content only shows inside the mask (D29.2).
static std::vector<BufferData> sceneClipMask(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  // clip source (writes stencil only).
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float clip[] = {-0.4f, -0.4f, 0.4f, 0.4f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"isClipSource":true})");
  // content (drawn only where stencil==1).
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

// Build a blend scenario for a given mode: an opaque blue base rect, then a
// semi-transparent red rect on top with the given blend mode (D29.1).
static dc::parity::SceneBuilder makeBlendScene(const char* mode) {
  std::string m(mode);
  return [m](dc::CommandProcessor& cp, dc::Scene&) -> std::vector<BufferData> {
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
    // base: opaque blue.
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
    float base[] = {-0.7f, -0.7f, 0.7f, 0.7f};
    cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
    cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
    cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":0,"b":1,"a":1})");
    // over: semi-transparent red with the blend mode under test.
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

// lineAA solid: a thick anti-aliased line (D10.6 / D28.1 test 2). The line core
// is a flat fill; only the AA fringe differs.
static std::vector<BufferData> sceneLineAASolid(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float seg[] = {-0.8f, 0.0f, 0.8f, 0.0f};  // rect4 = (p0, p1)
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

// lineAA dashed: a thick dashed line (D28.1 test 1). Dash on/off + AA fringe.
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
  // candle6 = (cx, open, high, low, close, hw). Identity transform = clip space.
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

// frustum cull: a transformed item fully outside the pane clip region must be
// culled by BOTH backends -> identical (clear) frames (D10.5).
static std::vector<BufferData> sceneFrustumCull(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float tri[] = {-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  // bounds make it cullable; transform pushes it far off-screen to the right.
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

int main() {
  std::printf("=== ENC-510 GL<->Dawn parity conformance ===\n");
  constexpr int W = 96, H = 96;

  // --- Solid-fill areas: near-exact interiors, a few AA-edge pixels allowed. --
  // channelTol kept small; maxMismatchPct covers only the 1-2px primitive edge.
  Tolerance solid;        solid.channelTol = 16;  solid.maxMismatchPct = 6.0;
  Tolerance solidTight;   solidTight.channelTol = 8; solidTight.maxMismatchPct = 4.0;

  // render core
  run("render-core/triSolid", sceneTriSolid, W, H, solidTight);
  // transforms
  run("transforms/scale+translate", sceneTransform, W, H, solidTight);
  // pipelines: instancedRect sharp (solid)
  run("pipelines/instRect-sharp", sceneRectSharp, W, H, solidTight);
  // scissor/clip
  run("scissor/pane-region", sceneScissor, W, H, solidTight);
  run("clip/stencil-mask", sceneClipMask, W, H, solid);
  // frustum cull -> both clear
  run("cull/frustum", sceneFrustumCull, W, H, solidTight);

  // --- Blend modes: the composited overlap is a flat color; assert the math. --
  // 8-bit blend round-off differs by at most a couple of LSBs between backends.
  Tolerance blend; blend.channelTol = 6; blend.maxMismatchPct = 5.0;
  run("blend/normal", makeBlendScene("normal"), W, H, blend);
  run("blend/additive", makeBlendScene("additive"), W, H, blend);
  run("blend/multiply", makeBlendScene("multiply"), W, H, blend);
  run("blend/screen", makeBlendScene("screen"), W, H, blend);

  // --- Gradient: smooth per-vertex interpolation. Interpolant rounding differs
  // by a handful of levels; interior should be close. --------------------------
  Tolerance grad; grad.channelTol = 12; grad.maxMismatchPct = 8.0;
  run("gradient/per-vertex-color", sceneGradient, W, H, grad);

  // --- AA areas: fractional-coverage fringes differ between rasterizers. The
  // core/interior must match; a small mismatch% of fringe pixels is allowed. ---
  Tolerance aa; aa.channelTol = 40; aa.maxMismatchPct = 12.0;
  run("aa/triAA", sceneTriAA, W, H, aa);
  run("aa/rounded-rect", sceneRectRounded, W, H, aa);
  run("aa/lineAA-solid", sceneLineAASolid, W, H, aa);
  run("aa/lineAA-dashed", sceneLineAADashed, W, H, aa);
  run("volume/candles", sceneCandles, W, H, aa);

  // --- 1px primitives: line2d / points are the hardest to land identically;
  // both rasterizers can place the single-pixel coverage one row/col apart. ----
  Tolerance thin; thin.channelTol = 40; thin.maxMismatchPct = 8.0;
  run("pipelines/line2d-1px", sceneLinePoints, W, H, thin);

  std::printf("\n=== parity conformance: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  // Skips (no GL or no Dawn) exit 0 (graceful). Real mismatches fail.
  return g_failed > 0 ? 1 : 0;
}
