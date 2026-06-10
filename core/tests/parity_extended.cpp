// ENC-512 (P5.0d) — GL <-> Dawn parity: texturedQuad, picking, indexed-gather,
// and a recipe-driven full chart.
//
// Extends the ENC-510/511 parity harness (parity_harness.hpp) to the remaining
// conformance areas so the full render-test bar is covered before the ENC-500
// cutover. Each scenario builds the SAME Scene (identical CommandProcessor JSON)
// and feeds it through BOTH backends, then diffs the readbacks (pixel parity) or
// the decoded pick ids (id-buffer parity).
//
// AREAS (each mirrors a GL render-test in the conformance bar):
//   1. texturedQuad@1   — d36_1_texture_quad / d41_1_texquad_render. The SAME
//                         RGBA texture is uploaded to GL (TextureManager) and Dawn
//                         (TextureSource); a textured quad is rendered both ways
//                         and the readbacks compared. Solid texels + a 2x2
//                         four-corner texture (sampled-texel placement) + color
//                         modulation.
//   2. GPU picking      — d29_3_gpu_picking / d41_2_texquad_pick. The pick PASS is
//                         rendered both ways (GL renderPick vs Dawn
//                         DawnSceneRenderer::renderPick) and the DECODED ids are
//                         compared at several probe pixels (id-buffer parity, not
//                         color). Covers triSolid, instancedRect, texturedQuad,
//                         overlap (topmost wins), and background.
//   3. indexed-gather   — d26_2_indexed_gl. An index/order buffer filters an
//                         instancedRect draw to a diagonal pair of quadrants; the
//                         visible subset must render identically on both backends.
//   4. recipe full chart— a representative chart built via the recipe/command path
//                         (CandleRecipe candles + a line2d axis + a line2d
//                         indicator) over per-pane clear; full-frame parity.
//
// TOLERANCE: solid texel fills + flat instanced fills are near-exact (small
// channelTol, a few edge pixels allowed). The 2x2 sampled-texel scene has a 1-2px
// bilinear seam at the texel boundary (the two samplers reconstruct the midline
// slightly differently), budgeted with a small mismatch%. Picking compares ids
// EXACTLY (no tolerance — an id either decodes equal or it doesn't).
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_harness.hpp"

#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using dc::parity::BufferData;
using dc::parity::compareScene;
using dc::parity::comparePick;
using dc::parity::ParityResult;
using dc::parity::PickParityResult;
using dc::parity::PickProbe;
using dc::parity::TextureInput;
using dc::parity::Tolerance;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;

static void run(const char* area, const dc::parity::SceneBuilder& b, int W, int H,
                Tolerance tol, const TextureInput* tex = nullptr) {
  ParityResult r = compareScene(area, b, W, H, tol, nullptr,
                                dc::parity::ParityStyle{}, tex);
  if (r.skipped) { ++g_skipped; return; }
  if (r.passed) ++g_passed; else ++g_failed;
}

static void runPick(const char* area, const dc::parity::SceneBuilder& b, int W,
                    int H, const std::vector<PickProbe>& probes) {
  PickParityResult r = comparePick(area, b, W, H, probes);
  if (r.skipped) { ++g_skipped; return; }
  if (r.passed) ++g_passed; else ++g_failed;
}

// ===========================================================================
// (1) texturedQuad@1 scene builders.
// ===========================================================================

// A full-viewport textured quad sampling logical textureId 1, white modulation
// (texel passes through). The SAME texture is supplied to both backends by the
// harness. Mirrors d41_1_texquad_render.
static std::vector<BufferData> sceneTexQuad(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};  // pos2_uv4 instance (x0,y0,x1,y1)
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})");
  return {BufferData(10, quad, sizeof(quad))};
}

// Same quad but with a half-green color modulation (u_color = 1,0.5,0,1) over a
// pure-white texture — proves fs out = texel * u_color matches on both backends.
static std::vector<BufferData> sceneTexQuadModulate(dc::CommandProcessor& cp,
                                                    dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0.5,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})");
  return {BufferData(10, quad, sizeof(quad))};
}

// ===========================================================================
// (3) indexed-gather (D26): instancedRect filtered by an index buffer.
//   4 quadrant rects; index selects the diagonal pair (rect0 + rect3). Only the
//   selected subset renders — both backends must show the SAME two quadrants.
//   Mirrors d26_2_indexed_gl test 2.
// ===========================================================================
static std::vector<BufferData> sceneIndexedRect(dc::CommandProcessor& cp,
                                                dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rects[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,  // rect0: bottom-left
       0.0f, -1.0f, 1.0f, 0.0f,  // rect1: bottom-right
      -1.0f,  0.0f, 0.0f, 1.0f,  // rect2: top-left
       0.0f,  0.0f, 1.0f, 1.0f,  // rect3: top-right
  };
  std::uint32_t indices[] = {0, 3};  // diagonal pair
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":64})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":8})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"rect4","indexBufferId":11,"indexCount":2})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})");
  return {BufferData(10, rects, sizeof(rects)),
          BufferData(11, indices, sizeof(indices))};
}

// A textured indexed gather (D26 over texturedQuad@1): same diagonal pair, but
// the visible quadrants are TEXTURED (white texture id 1). Proves the indexed
// gather path matches for the textured pipeline too. Mirrors d36_1 case 3.
static std::vector<BufferData> sceneIndexedTexQuad(dc::CommandProcessor& cp,
                                                   dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float quads[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,
       0.0f, -1.0f, 1.0f, 0.0f,
      -1.0f,  0.0f, 0.0f, 1.0f,
       0.0f,  0.0f, 1.0f, 1.0f,
  };
  std::uint32_t indices[] = {0, 3};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":64})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":8})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"pos2_uv4","indexBufferId":11,"indexCount":2})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})");
  return {BufferData(10, quads, sizeof(quads)),
          BufferData(11, indices, sizeof(indices))};
}

// ===========================================================================
// (2) picking scene builders (drive comparePick).
// ===========================================================================

// A full-viewport triSolid triangle (id 5) — center picks 5, anywhere picks 5.
static std::vector<BufferData> scenePickTri(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})");
  float tri[] = {-1, -1, 3, -1, -1, 3};  // full-screen triangle
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1,"g":0,"b":0,"a":1})");
  return {BufferData(10, tri, sizeof(tri))};
}

// A centered instancedRect (id 72) covering the middle; corners are background.
static std::vector<BufferData> scenePickRect(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":72,"layerId":2})");
  float rect[] = {-0.5f, -0.5f, 0.5f, 0.5f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":72,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":72,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":72,"cornerRadius":0})");
  return {BufferData(10, rect, sizeof(rect))};
}

// Two overlapping full-screen triangles; the topmost (id 60, drawn last) wins.
static std::vector<BufferData> scenePickOverlap(dc::CommandProcessor& cp,
                                                dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  float big[] = {-1, -1, 3, -1, -1, 3};
  // bottom (id 50)
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":50,"r":1,"g":0,"b":0,"a":1})");
  // top (id 60)
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":60,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":60,"pipeline":"triSolid@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":60,"r":0,"g":1,"b":0,"a":1})");
  return {BufferData(10, big, sizeof(big)), BufferData(11, big, sizeof(big))};
}

// A full-viewport textured quad (id 5) — picking parity for texturedQuad@1
// (which maps to the pickInstRect pick pipeline). Mirrors d41_2_texquad_pick.
// The texture itself is irrelevant to picking (id-as-color), so no texture is
// supplied — the pick pass uses the flat id encoder, not the visible texels.
static std::vector<BufferData> scenePickTexQuad(dc::CommandProcessor& cp,
                                                dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})");
  float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"texturedQuad@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":5,"textureId":1})");
  return {BufferData(10, quad, sizeof(quad))};
}

// ===========================================================================
// (4) recipe-driven full chart.
//   Built through the recipe/command path (CandleRecipe + LineRecipe), the way a
//   chart-controller assembles a real chart. A price pane with:
//     * a row of OHLC candles (CandleRecipe -> instancedCandle@1),
//     * a horizontal "axis" baseline (LineRecipe -> line2d@1),
//     * a sloped "indicator" line (LineRecipe -> line2d@1).
//   The recipe emits the create-command JSON; the harness replays it through BOTH
//   backends and supplies the buffer bytes (the recipes create byteLength:0
//   buffers — real data is streamed in separately, mirrored here by setting the
//   buffer bytes + setGeometryVertexCount).
// ===========================================================================
static std::vector<BufferData> sceneRecipeChart(dc::CommandProcessor& cp,
                                                dc::Scene&) {
  std::vector<BufferData> bufs;

  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.05,"g":0.06,"b":0.08,"a":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  auto applyAll = [&](const std::vector<std::string>& cmds) {
    for (const auto& c : cmds) cp.applyJsonText(c);
  };

  // --- Candles via CandleRecipe (idBase 100): buffer 100, geom 101, di 102. ---
  {
    dc::CandleRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "OHLC";
    cfg.createTransform = false;  // identity == clip space, like the GL candle test
    cfg.colorUp[0] = 0; cfg.colorUp[1] = 1; cfg.colorUp[2] = 0; cfg.colorUp[3] = 1;
    cfg.colorDown[0] = 1; cfg.colorDown[1] = 0; cfg.colorDown[2] = 0; cfg.colorDown[3] = 1;
    dc::CandleRecipe rec(100, cfg);
    applyAll(rec.build().createCommands);
    // 3 candles: candle6 = (cx, open, high, low, close, hw).
    float candles[] = {
        -0.5f, -0.3f, 0.6f, -0.6f,  0.3f, 0.12f,  // UP
         0.0f,  0.3f, 0.6f, -0.6f, -0.3f, 0.12f,  // DOWN
         0.5f, -0.3f, 0.6f, -0.6f,  0.3f, 0.12f,  // UP
    };
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":3})");
    bufs.emplace_back(100, candles, sizeof(candles));
  }

  // --- Axis baseline via LineRecipe (idBase 110): buffer 110, geom 111, di 112. ---
  {
    dc::LineRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "Axis";
    cfg.createTransform = false;
    dc::LineRecipe rec(110, cfg);
    applyAll(rec.build().createCommands);
    float axis[] = {-0.9f, -0.75f, 0.9f, -0.75f};  // horizontal baseline
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":111,"vertexCount":2})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":112,"r":0.6,"g":0.6,"b":0.6,"a":1})");
    bufs.emplace_back(110, axis, sizeof(axis));
  }

  // --- Indicator line via LineRecipe (idBase 120): buffer 120, geom 121, di 122. ---
  {
    dc::LineRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "MA";
    cfg.createTransform = false;
    dc::LineRecipe rec(120, cfg);
    applyAll(rec.build().createCommands);
    float ind[] = {-0.9f, -0.2f, -0.3f, 0.1f, 0.3f, -0.05f, 0.9f, 0.25f};  // 4-pt polyline
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":121,"vertexCount":4})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":122,"r":1,"g":0.8,"b":0.0,"a":1})");
    bufs.emplace_back(120, ind, sizeof(ind));
  }

  return bufs;
}

int main() {
  std::printf("=== ENC-512 GL<->Dawn parity: texturedQuad / picking / indexed / recipes ===\n");

  // -----------------------------------------------------------------------
  // (1) texturedQuad@1 parity.
  // -----------------------------------------------------------------------
  // Solid-red 2x2 texture (matches the GL d41_1 baseline). A solid texture has no
  // internal seam, so the textured fill is byte-near-exact.
  {
    TextureInput red;
    red.id = 1; red.width = 2; red.height = 2;
    red.rgba.assign(2 * 2 * 4, 0);
    for (int i = 0; i < 4; ++i) { red.rgba[i*4+0] = 255; red.rgba[i*4+3] = 255; }
    Tolerance solid; solid.channelTol = 8; solid.maxMismatchPct = 4.0;
    run("texturedQuad/solid-red", sceneTexQuad, 96, 96, solid, &red);
  }

  // 2x2 four-corner texture (R,G,B,W) — checks sampled-texel placement parity.
  // The interior is four flat texel regions; only a 1-2px bilinear seam at the
  // texel midlines differs between the two samplers (documented), budgeted with a
  // small mismatch%.
  {
    TextureInput corners;
    corners.id = 1; corners.width = 2; corners.height = 2;
    corners.rgba = {
        255,   0,   0, 255,   // (0,0) RED
          0, 255,   0, 255,   // (1,0) GREEN
          0,   0, 255, 255,   // (0,1) BLUE
        255, 255, 255, 255,   // (1,1) WHITE
    };
    Tolerance tex; tex.channelTol = 24; tex.maxMismatchPct = 12.0;
    run("texturedQuad/4-corner-texels", sceneTexQuad, 96, 96, tex, &corners);
  }

  // color modulation: white texture * (1,0.5,0) — flat modulated fill.
  {
    TextureInput white;
    white.id = 1; white.width = 1; white.height = 1;
    white.rgba = {255, 255, 255, 255};
    Tolerance solid; solid.channelTol = 8; solid.maxMismatchPct = 4.0;
    run("texturedQuad/color-modulate", sceneTexQuadModulate, 96, 96, solid, &white);
  }

  // -----------------------------------------------------------------------
  // (3) indexed-gather (D26) parity.
  // -----------------------------------------------------------------------
  // The visible subset (diagonal quadrant pair) is a flat solid fill — near-exact.
  {
    Tolerance solid; solid.channelTol = 8; solid.maxMismatchPct = 4.0;
    run("indexed-gather/instRect-diagonal", sceneIndexedRect, 96, 96, solid);
  }
  // Indexed gather over the textured pipeline (white texture id 1).
  {
    TextureInput white;
    white.id = 1; white.width = 1; white.height = 1;
    white.rgba = {255, 255, 255, 255};
    Tolerance solid; solid.channelTol = 8; solid.maxMismatchPct = 4.0;
    run("indexed-gather/texQuad-diagonal", sceneIndexedTexQuad, 96, 96, solid, &white);
  }

  // -----------------------------------------------------------------------
  // (2) GPU picking parity (id-buffer parity — decoded ids compared EXACTLY).
  // Probe coords are GL (bottom-left) origin; the harness flips for Dawn.
  // -----------------------------------------------------------------------
  {
    constexpr int W = 64, H = 64;
    // Full-screen triangle: center and an off-center point both pick id 5.
    runPick("pick/triSolid", scenePickTri, W, H,
            {{W/2, H/2, 5}, {W/4, H/2, 5}, {W/2, 3*H/4, 5}});
    // Centered rect: center picks 72, corners are background (0).
    runPick("pick/instRect", scenePickRect, W, H,
            {{W/2, H/2, 72}, {2, 2, 0}, {W-3, H-3, 0}});
    // Overlap: topmost (id 60) wins at center.
    runPick("pick/overlap-topmost", scenePickOverlap, W, H,
            {{W/2, H/2, 60}});
    // texturedQuad full-screen: id 5 everywhere (d41_2 pattern).
    runPick("pick/texturedQuad", scenePickTexQuad, W, H,
            {{W/2, H/2, 5}, {2, 2, 5}, {W-3, H-3, 5}});
  }

  // -----------------------------------------------------------------------
  // (4) recipe-driven full chart parity.
  // Candles (AA wicks) + two polylines over a per-pane clear. The candle bodies +
  // clear are flat fills (near-exact); the candle wicks and the 1px polylines are
  // thin AA primitives whose exact coverage can differ a row/col between the two
  // rasterizers, so this frame carries the same thin-primitive mismatch budget as
  // the lineAA / 1px areas in parity_conformance.
  // -----------------------------------------------------------------------
  {
    Tolerance chart; chart.channelTol = 40; chart.maxMismatchPct = 12.0;
    run("recipe/full-chart", sceneRecipeChart, 128, 128, chart);
  }

  std::printf("\n=== ENC-512 extended parity: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  return g_failed > 0 ? 1 : 0;
}
