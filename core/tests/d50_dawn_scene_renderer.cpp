// ENC-509 (P5.0a) — DawnSceneRenderer: full scene-walk dispatch on Dawn.
//
// Proves the keystone reusable renderer: a SINGLE DawnSceneRenderer owns a
// DawnDevice + all 10 Dawn pipeline backends and walks a whole multi-pipeline
// Scene (built via CommandProcessor JSON commands) into the headless offscreen
// target, dispatching every DrawItem through the backend registry — the Dawn
// equivalent of GL Renderer::render. We then read back and assert each element
// appears at its expected location with the right color, with clear background
// between them. This is the foundation for ENC-510 (conformance) / ENC-500
// (cutover): no more hand-walking the scene per test.
//
// SCENE (one pane, one layer, clip space, identity transform):
//   * triSolid@1    — a RED triangle filling the LEFT third of the frame.
//                     verts clip x in [-0.9,-0.3], y in [-0.7,0.7].
//   * instancedRect@1 — a GREEN rect in the MIDDLE third (rect4 instance,
//                     clip x in [-0.2,0.2], y in [-0.6,0.6]).
//   * line2d@1      — a BLUE horizontal line across the RIGHT third at clip y=0
//                     (x in [0.35,0.9]). 1px LineList (WebGPU has no line width).
//   * textSDF@1     — (only when a test font is available) cyan "Hi" glyphs near
//                     the top; we just assert SOME text-colored pixels appear
//                     (the dedicated d3_3_dawn_text_sdf already nails text parity).
//
// Each colored element is read back at its expected center and asserted; gaps
// between elements are asserted clear. This confirms the full walk dispatches
// triSolid (non-instanced tris), instancedRect (instanced quads) and line2d
// (LineList) — three distinct pipeline classes — in one render() call.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnSceneRenderer.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/debug/Stats.hpp"

#ifdef FONT_PATH
#include "dc/text/GlyphAtlas.hpp"
#include <vector>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
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

static bool isColor(const std::uint8_t* p, int r, int g, int b) {
  auto near = [](int v, int t) { return (v >= t ? v - t : t - v) < 40; };
  return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

int main() {
  std::printf("=== ENC-509 DawnSceneRenderer full scene-walk ===\n");

  constexpr int W = 96;
  constexpr int H = 64;

#ifdef FONT_PATH
  // Optional glyph atlas for textSDF@1 (only when a test font is configured).
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  bool haveFont = atlas.loadFontFile(FONT_PATH);
  if (haveFont) atlas.ensureAscii();
  dc::DawnSceneRenderer renderer(haveFont ? &atlas : nullptr, nullptr);
#else
  bool haveFont = false;
  dc::DawnSceneRenderer renderer;
#endif

  if (!renderer.init()) {
    std::fprintf(stderr, "DawnSceneRenderer::init failed: %s\n",
                 renderer.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnSceneRenderer up: backend=%s adapter=\"%s\"\n",
              renderer.device().backendName().c_str(),
              renderer.device().adapterName().c_str());

  // --- Build the multi-pipeline scene via CommandProcessor JSON. ----------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // 1) triSolid@1 — RED triangle in the LEFT third.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di-tri");
  float tri[] = {-0.9f, -0.7f, -0.3f, -0.7f, -0.6f, 0.7f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf-tri");
  store.setCpuData(10, tri, sizeof(tri));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
      "geom-tri");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
      "bind-tri");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})"), "color-tri");

  // 2) instancedRect@1 — GREEN rect in the MIDDLE third.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "di-rect");
  float rect[] = {-0.2f, -0.6f, 0.2f, 0.6f};  // rect4 instance: x0 y0 x1 y1
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":16})"), "buf-rect");
  store.setCpuData(11, rect, sizeof(rect));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})"),
      "geom-rect");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})"),
      "bind-rect");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})"), "color-rect");

  // 3) line2d@1 — BLUE horizontal line across the RIGHT third at clip y=0.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di-line");
  float line[] = {0.35f, 0.0f, 0.9f, 0.0f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":16})"), "buf-line");
  store.setCpuData(12, line, sizeof(line));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":102,"vertexBufferId":12,"vertexCount":2,"format":"pos2_clip"})"),
      "geom-line");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"line2d@1","geometryId":102})"),
      "bind-line");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":5,"r":0,"g":0,"b":1,"a":1})"), "color-line");

#ifdef FONT_PATH
  // 4) textSDF@1 — cyan "Hi" near the top (only with a font + atlas registered).
  int glyphCount = 0;
  if (haveFont) {
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":6,"layerId":2})"), "di-text");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":13,"byteLength":0})"), "buf-text");
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":103,"vertexBufferId":13,"vertexCount":1,"format":"glyph8"})"),
        "geom-text");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":6,"pipeline":"textSDF@1","geometryId":103})"),
        "bind-text");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":6,"r":0,"g":1,"b":1,"a":1})"), "color-text");

    const char* text = "Hi";
    const float fontSize = 0.5f;
    float cursorX = -0.15f;
    // NDC NOTE: every Dawn backend negates clip-space y so the WebGPU top-left
    // framebuffer matches the GL bottom-left readback (see d2_1_dawn_first_render
    // "apex (0,0.5) maps below center after the flip"). So a glyph's TOP-of-frame
    // readback position needs a NEGATIVE clip-space baseline. Place "Hi" with its
    // quads spanning clip y ~[-0.85,-0.50] so they land in the top third of the
    // readback (rows ~5..16), where the probe below scans for cyan.
    const float baselineY = -0.85f;
    std::vector<float> inst;
    for (const char* p = text; *p; ++p) {
      const dc::GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
      if (!g) continue;
      const float scale = fontSize / 48.0f;
      if (g->w <= 0 || g->h <= 0) { cursorX += g->advance * scale; continue; }
      const float x0 = cursorX + g->bearingX * scale;
      const float y1 = baselineY + g->bearingY * scale;
      const float y0 = y1 - g->h * scale;
      const float x1 = x0 + g->w * scale;
      inst.insert(inst.end(), {x0, y0, x1, y1, g->u0, g->v0, g->u1, g->v1});
      ++glyphCount;
      cursorX += g->advance * scale;
    }
    store.setCpuData(13, inst.data(),
                     static_cast<std::uint32_t>(inst.size() * sizeof(float)));
    requireOk(cp.applyJsonText(
        std::string(R"({"cmd":"setGeometryVertexCount","geometryId":103,"vertexCount":)") +
        std::to_string(glyphCount) + "}"), "setVC-text");
  }
#endif

  // --- Render the WHOLE scene in one walk. --------------------------------
  dc::Stats stats = renderer.render(scene, store, W, H);
  std::printf("render: drawCalls=%u culled=%u\n", stats.drawCalls,
              stats.culledDrawCalls);
  check(stats.drawCalls >= 3, "at least 3 draw calls (tri + rect + line)");

  // --- Read back and assert each element at its expected location. --------
  auto px = [&](int x, int y) {
    static std::uint8_t buf[4];
    renderer.device().readPixel(x, y, buf);
    return buf;
  };

  // triSolid RED: probe near the triangle's lower-left interior. The triangle
  // spans clip x[-0.9,-0.3] -> px[4.8,33.6]; pick px (12, ~H*0.62) inside it.
  {
    const std::uint8_t* p = px(12, H / 2);
    std::printf("  triSolid probe (12,%d): R=%u G=%u B=%u\n", H / 2, p[0], p[1], p[2]);
    check(isColor(p, 255, 0, 0), "triSolid: left third is RED");
  }

  // instancedRect GREEN: rect clip x[-0.2,0.2] -> px[38.4,57.6]; center col 48.
  {
    const std::uint8_t* p = px(W / 2, H / 2);
    std::printf("  instRect probe (%d,%d): R=%u G=%u B=%u\n", W / 2, H / 2, p[0], p[1], p[2]);
    check(isColor(p, 0, 255, 0), "instancedRect: middle third is GREEN");
  }

  // line2d BLUE: line clip x[0.35,0.9] -> px[64.8,91.2] at center row. WebGPU
  // 1px line — probe +/-1 row for robustness.
  {
    bool lit = false;
    for (int dy = -1; dy <= 1 && !lit; ++dy) {
      const std::uint8_t* p = px(78, H / 2 + dy);
      if (isColor(p, 0, 0, 255)) lit = true;
    }
    std::printf("  line2d probe (78,~%d): lit=%d\n", H / 2, lit ? 1 : 0);
    check(lit, "line2d: right third has a BLUE line on the center row");
  }

  // Gap between the rect (ends px ~57) and the line start (px ~65), well off the
  // center line row, should be clear black. Probe (61, H/2-10).
  {
    const std::uint8_t* p = px(61, H / 2 - 10);
    std::printf("  gap probe (61,%d): R=%u G=%u B=%u\n", H / 2 - 10, p[0], p[1], p[2]);
    check(isColor(p, 0, 0, 0), "gap between rect and line is clear (black)");
  }

  // Gap between triangle (ends px ~33) and rect (starts px ~38): probe col 36
  // near bottom (away from the triangle's tall apex), should be clear.
  {
    const std::uint8_t* p = px(36, 6);
    std::printf("  gap2 probe (36,6): R=%u G=%u B=%u\n", p[0], p[1], p[2]);
    check(isColor(p, 0, 0, 0), "gap between triangle and rect is clear (black)");
  }

#ifdef FONT_PATH
  if (haveFont) {
    // Scan the top region for cyan text pixels (g & b strong, r near 0).
    int textPx = 0;
    for (int y = 0; y < H / 3; ++y) {
      for (int x = 0; x < W; ++x) {
        const std::uint8_t* p = px(x, y);
        if (p[0] < 60 && p[1] > 180 && p[2] > 180) ++textPx;
      }
    }
    std::printf("  textSDF cyan pixels in top third: %d (glyphs=%d)\n",
                textPx, glyphCount);
    check(textPx > 5, "textSDF: cyan 'Hi' glyph pixels present near the top");
  }
#endif

  std::printf("=== DawnSceneRenderer: %d passed, %d failed (on %s) ===\n",
              passed, failed, renderer.device().backendName().c_str());
  return failed > 0 ? 1 : 0;
}
