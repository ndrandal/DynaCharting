// ENC-501 (P5 cutover) — Dawn-only golden extended suite: texturedQuad, picking,
// indexed-gather, and a recipe-driven full chart.
//
// Originally (ENC-512) this rendered each scene through BOTH the GL backend and
// the DawnSceneRenderer and compared the readbacks (pixel parity) or the decoded
// pick ids (id-buffer parity). Dawn is now the proven default renderer and dc_gl
// is being deleted (ENC-501), so this renders ONLY via Dawn and asserts:
//   * color scenes  -> probe pixels match captured GOLDEN values,
//   * pick scenes   -> decoded DrawItem ids match the expected ids (self-validating;
//                      the expected id IS the golden — picking is exact, no readback
//                      reference needed).
//
// SCENARIO COVERAGE (unchanged from ENC-512 — 10 scenarios):
//   texturedQuad/{solid-red, 4-corner-texels, color-modulate},
//   indexed-gather/{instRect-diagonal, texQuad-diagonal},
//   pick/{triSolid, instRect, overlap-topmost, texturedQuad},
//   recipe/full-chart.
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "parity_golden.hpp"

#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using dc::golden::BufferData;
using dc::golden::GoldenFrame;
using dc::golden::PickFrame;
using dc::golden::PickProbe;
using dc::golden::pickDawn;
using dc::golden::renderDawn;
using dc::golden::SceneBuilder;
using dc::golden::TextureInput;

static int g_failed = 0;
static int g_passed = 0;
static int g_skipped = 0;
static bool g_capture = false;

struct Probe { int x, y, r, g, b, tol; };

static void runScene(const char* name, const SceneBuilder& b, int W, int H,
                     const std::vector<Probe>& probes,
                     const TextureInput* tex = nullptr) {
  GoldenFrame f = renderDawn(name, b, W, H, nullptr, {}, tex);
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

// Picking is self-validating: the expected id IS the golden (an id either decodes
// equal or it doesn't — no GL reference needed).
static void runPick(const char* name, const SceneBuilder& b, int W, int H,
                    const std::vector<PickProbe>& probes) {
  PickFrame f = pickDawn(name, b, W, H, probes);
  if (f.skipped) { ++g_skipped; return; }
  bool ok = true;
  for (const auto& row : f.rows) {
    if (g_capture) {
      std::printf("  [capture %s] probe(%d,%d) expect=%u got=%u\n", name, row.x,
                  row.y, row.expectId, row.gotId);
      continue;
    }
    if (!row.match) {
      ok = false;
      std::fprintf(stderr, "  FAIL [%s] probe(%d,%d): expect id=%u got id=%u\n",
                   name, row.x, row.y, row.expectId, row.gotId);
    }
  }
  if (g_capture) { std::printf("[%s] dawn=%s captured\n", name, f.dawnBackend.c_str()); return; }
  if (ok) { ++g_passed; std::printf("  PASS: %s\n", name); }
  else { ++g_failed; }
}

// ===========================================================================
// texturedQuad@1 scene builders.
// ===========================================================================
static std::vector<BufferData> sceneTexQuad(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float quad[] = {-1.0f, -1.0f, 1.0f, 1.0f};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"pos2_uv4"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"texturedQuad@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":1,"b":1,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":3,"textureId":1})");
  return {BufferData(10, quad, sizeof(quad))};
}

static std::vector<BufferData> sceneTexQuadModulate(dc::CommandProcessor& cp, dc::Scene&) {
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
// indexed-gather (D26): instancedRect / texturedQuad filtered by an index buffer.
// ===========================================================================
static std::vector<BufferData> sceneIndexedRect(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
  float rects[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,  // rect0: bottom-left
       0.0f, -1.0f, 1.0f, 0.0f,  // rect1: bottom-right
      -1.0f,  0.0f, 0.0f, 1.0f,  // rect2: top-left
       0.0f,  0.0f, 1.0f, 1.0f,  // rect3: top-right
  };
  std::uint32_t indices[] = {0, 3};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":64})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":8})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"rect4","indexBufferId":11,"indexCount":2})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})");
  return {BufferData(10, rects, sizeof(rects)), BufferData(11, indices, sizeof(indices))};
}

static std::vector<BufferData> sceneIndexedTexQuad(dc::CommandProcessor& cp, dc::Scene&) {
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
  return {BufferData(10, quads, sizeof(quads)), BufferData(11, indices, sizeof(indices))};
}

// ===========================================================================
// picking scene builders (drive pickDawn).
// ===========================================================================
static std::vector<BufferData> scenePickTri(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})");
  float tri[] = {-1, -1, 3, -1, -1, 3};
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1,"g":0,"b":0,"a":1})");
  return {BufferData(10, tri, sizeof(tri))};
}

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

static std::vector<BufferData> scenePickOverlap(dc::CommandProcessor& cp, dc::Scene&) {
  cp.applyJsonText(R"({"cmd":"createPane","id":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
  float big[] = {-1, -1, 3, -1, -1, 3};
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":100})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":50,"r":1,"g":0,"b":0,"a":1})");
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":60,"layerId":2})");
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})");
  cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})");
  cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":60,"pipeline":"triSolid@1","geometryId":101})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":60,"r":0,"g":1,"b":0,"a":1})");
  return {BufferData(10, big, sizeof(big)), BufferData(11, big, sizeof(big))};
}

static std::vector<BufferData> scenePickTexQuad(dc::CommandProcessor& cp, dc::Scene&) {
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
// recipe-driven full chart (CandleRecipe + LineRecipe).
// ===========================================================================
static std::vector<BufferData> sceneRecipeChart(dc::CommandProcessor& cp, dc::Scene&) {
  std::vector<BufferData> bufs;
  cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})");
  cp.applyJsonText(R"({"cmd":"setPaneClearColor","id":1,"r":0.05,"g":0.06,"b":0.08,"a":1})");
  cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");

  auto applyAll = [&](const std::vector<std::string>& cmds) {
    for (const auto& c : cmds) cp.applyJsonText(c);
  };

  {
    dc::CandleRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "OHLC";
    cfg.createTransform = false;
    cfg.colorUp[0] = 0; cfg.colorUp[1] = 1; cfg.colorUp[2] = 0; cfg.colorUp[3] = 1;
    cfg.colorDown[0] = 1; cfg.colorDown[1] = 0; cfg.colorDown[2] = 0; cfg.colorDown[3] = 1;
    dc::CandleRecipe rec(100, cfg);
    applyAll(rec.build().createCommands);
    float candles[] = {
        -0.5f, -0.3f, 0.6f, -0.6f,  0.3f, 0.12f,  // UP
         0.0f,  0.3f, 0.6f, -0.6f, -0.3f, 0.12f,  // DOWN
         0.5f, -0.3f, 0.6f, -0.6f,  0.3f, 0.12f,  // UP
    };
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":3})");
    bufs.emplace_back(100, candles, sizeof(candles));
  }
  {
    dc::LineRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "Axis";
    cfg.createTransform = false;
    dc::LineRecipe rec(110, cfg);
    applyAll(rec.build().createCommands);
    float axis[] = {-0.9f, -0.75f, 0.9f, -0.75f};
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":111,"vertexCount":2})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":112,"r":0.6,"g":0.6,"b":0.6,"a":1})");
    bufs.emplace_back(110, axis, sizeof(axis));
  }
  {
    dc::LineRecipeConfig cfg;
    cfg.paneId = 1; cfg.layerId = 2; cfg.name = "MA";
    cfg.createTransform = false;
    dc::LineRecipe rec(120, cfg);
    applyAll(rec.build().createCommands);
    float ind[] = {-0.9f, -0.2f, -0.3f, 0.1f, 0.3f, -0.05f, 0.9f, 0.25f};
    cp.applyJsonText(R"({"cmd":"setGeometryVertexCount","geometryId":121,"vertexCount":4})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":122,"r":1,"g":0.8,"b":0.0,"a":1})");
    bufs.emplace_back(120, ind, sizeof(ind));
  }
  return bufs;
}

int main(int argc, char** argv) {
  if ((argc > 1 && std::strcmp(argv[1], "--capture") == 0) ||
      std::getenv("DC_GOLDEN_CAPTURE"))
    g_capture = true;

  std::printf("=== ENC-501 Dawn-only golden extended ===\n");
  const int tol = 10;

  // -----------------------------------------------------------------------
  // texturedQuad@1.
  // -----------------------------------------------------------------------
  // solid-red 2x2 texture -> full-frame red.
  {
    TextureInput red;
    red.id = 1; red.width = 2; red.height = 2;
    red.rgba.assign(2 * 2 * 4, 0);
    for (int i = 0; i < 4; ++i) { red.rgba[i*4+0] = 255; red.rgba[i*4+3] = 255; }
    runScene("texturedQuad/solid-red", sceneTexQuad, 96, 96,
             {{48, 48, 255, 0, 0, tol}, {10, 10, 255, 0, 0, tol}}, &red);
  }
  // 2x2 four-corner texture (R,G,B,W) — probe each quadrant interior (away from
  // the bilinear midline seam). On-screen the texture maps with v flipped vs the
  // texel rows, so the captured probes are baked from the actual Dawn output.
  {
    TextureInput corners;
    corners.id = 1; corners.width = 2; corners.height = 2;
    corners.rgba = {
        255,   0,   0, 255,   // (0,0) RED
          0, 255,   0, 255,   // (1,0) GREEN
          0,   0, 255, 255,   // (0,1) BLUE
        255, 255, 255, 255,   // (1,1) WHITE
    };
    // Texel layout maps to screen quadrants as (captured from the readback):
    //   (24,24)=RED  (72,24)=GREEN  (24,72)=BLUE  (72,72)=WHITE.
    runScene("texturedQuad/4-corner-texels", sceneTexQuad, 96, 96,
             {{24, 24, 255, 0, 0, 24}, {72, 24, 0, 255, 0, 24},
              {24, 72, 0, 0, 255, 24}, {72, 72, 255, 255, 255, 24}}, &corners);
  }
  // color modulation: white texture * (1,0.5,0) -> (255,128,0).
  {
    TextureInput white;
    white.id = 1; white.width = 1; white.height = 1;
    white.rgba = {255, 255, 255, 255};
    runScene("texturedQuad/color-modulate", sceneTexQuadModulate, 96, 96,
             {{48, 48, 255, 128, 0, tol}}, &white);
  }

  // -----------------------------------------------------------------------
  // indexed-gather (D26): diagonal quadrant pair (rect0 BL + rect3 TR) visible.
  // -----------------------------------------------------------------------
  {
    // rect0 is clip BL (-1,-1..0,0); rect3 is clip TR (0,0..1,1). In the readback
    // (clip +y -> bottom rows) the two SELECTED quadrants land at top-left (24,24)
    // and bottom-right (72,72); the unselected pair stays clear.
    runScene("indexed-gather/instRect-diagonal", sceneIndexedRect, 96, 96,
             {{24, 24, 255, 0, 0, tol},    // selected quadrant -> red
              {72, 72, 255, 0, 0, tol},    // selected quadrant -> red
              {72, 24, 0, 0, 0, tol},      // unselected quadrant -> clear
              {24, 72, 0, 0, 0, tol}});    // unselected quadrant -> clear
  }
  {
    TextureInput white;
    white.id = 1; white.width = 1; white.height = 1;
    white.rgba = {255, 255, 255, 255};
    runScene("indexed-gather/texQuad-diagonal", sceneIndexedTexQuad, 96, 96,
             {{24, 24, 255, 255, 255, tol}, {72, 72, 255, 255, 255, tol},
              {72, 24, 0, 0, 0, tol}, {24, 72, 0, 0, 0, tol}}, &white);
  }

  // -----------------------------------------------------------------------
  // GPU picking (id-buffer parity — decoded ids asserted EXACTLY; self-validating).
  // -----------------------------------------------------------------------
  {
    constexpr int W = 64, H = 64;
    runPick("pick/triSolid", scenePickTri, W, H,
            {{W/2, H/2, 5}, {W/4, H/2, 5}, {W/2, 3*H/4, 5}});
    runPick("pick/instRect", scenePickRect, W, H,
            {{W/2, H/2, 72}, {2, 2, 0}, {W-3, H-3, 0}});
    runPick("pick/overlap-topmost", scenePickOverlap, W, H,
            {{W/2, H/2, 60}});
    runPick("pick/texturedQuad", scenePickTexQuad, W, H,
            {{W/2, H/2, 5}, {2, 2, 5}, {W-3, H-3, 5}});
  }

  // -----------------------------------------------------------------------
  // recipe-driven full chart: candles + axis + indicator over a dark clear.
  // -----------------------------------------------------------------------
  {
    // Probe the pane clear background + the center (down) candle body. Thin
    // primitives (wicks/polylines) are avoided as probe targets.
    runScene("recipe/full-chart", sceneRecipeChart, 128, 128,
             {{6, 6, 12, 15, 20, 12},        // dark pane clear (0.05,0.06,0.08)
              {64, 64, 255, 0, 0, 20}});     // center down-candle body (red)
  }

  if (g_capture) { std::printf("=== capture complete ===\n"); return 0; }
  std::printf("\n=== golden extended: %d passed, %d failed, %d skipped ===\n",
              g_passed, g_failed, g_skipped);
  return g_failed > 0 ? 1 : 0;
}
