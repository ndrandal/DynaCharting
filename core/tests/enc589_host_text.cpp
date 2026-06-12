// ENC-589 (P0.1) — Wire the SDF glyph atlas into the WASM host (text marks).
//
// The textSDF@1 pipeline, the DawnTextSdfBackend, and the GlyphAtlas all already
// exist; what was missing was a HOST path that (a) supplies the atlas to the
// DawnSceneRenderer so textSDF@1 actually registers, and (b) turns a string +
// position into the Glyph8 instance geometry the backend consumes. The WASM
// DcEngineHost now does both:
//   * ensureRenderer() constructs DawnSceneRenderer(&atlas_, ...) (was nullptr).
//   * setTextGeometry(bufId, geomId, text, clipX, clipY, fontSize) rasterizes the
//     string's glyphs (GlyphAtlas::ensureGlyphs), lays them out with the shared
//     dc::layoutText helper, writes the Glyph8 bytes into the render store, and
//     sets the geometry vertexCount.
//
// DcEngineHost itself is Emscripten-only (it includes <emscripten/bind.h> and is
// built only as the WASM target), so it cannot be instantiated in a native ctest.
// This test instead exercises the EXACT native mirror of the host's new wiring —
// atlas-supplied DawnSceneRenderer + dc::layoutText -> Glyph8 -> the render store,
// driven through the full scene-walk render() (NOT the standalone backend the
// d3_3 test pokes) — and asserts a known label renders non-blank text pixels in
// its color, on a clean background. If this renders, the host's setTextGeometry/
// loadFont path renders, because it runs the identical code.
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
#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

int main() {
  std::printf("=== ENC-589 host glyph-atlas text wiring ===\n");

#ifndef FONT_PATH
  std::printf("SKIPPED: no FONT_PATH configured\n");
  return 0;
#else
  constexpr int W = 192;
  constexpr int H = 64;

  // --- Atlas: the SAME setup DcEngineHost's ctor uses (atlasSize 512, glyphPx
  //     48, SDF mode). Load the test font via loadFontFile here; the host loads
  //     the SAME bytes via GlyphAtlas::loadFont(bytes) from a JS Uint8Array. ----
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  if (!atlas.loadFontFile(FONT_PATH)) {
    std::fprintf(stderr, "FAIL: font load (%s)\n", FONT_PATH);
    return 1;
  }
  check(atlas.useSdf(), "atlas in SDF mode");

  // --- Renderer WITH the atlas supplied — the crux of ENC-589. Before the
  //     ticket the host passed atlas=nullptr, so textSDF@1 was never registered
  //     and text could not render. ensureRenderer() now passes &atlas_, which is
  //     exactly this. ----------------------------------------------------------
  dc::DawnSceneRenderer renderer(&atlas, /*textures=*/nullptr);
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

  // --- Scene scaffold via the control plane — the SAME applyControl commands a
  //     caller issues before DcEngineHost::setTextGeometry: pane/layer/drawItem/
  //     buffer/geometry(glyph8)/bind(textSDF@1)/color. -------------------------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "buf");
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"glyph8"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"textSDF@1","geometryId":100})"),
      "bind");
  // Cyan text (0,1,1) — distinctive vs the black clear background.
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":1,"a":1})"), "color");

  // --- Reproduce DcEngineHost::setTextGeometry verbatim: ensureGlyphs +
  //     dc::layoutText -> store.setCpuData -> setGeometryVertexCount. -----------
  const std::string label = "Label";
  const double clipX = -0.85;
  const double clipY = -0.6;   // negative baseline -> upper rows after the y-flip
  const double fontSize = 0.55;

  std::vector<std::uint32_t> cps;
  for (unsigned char ch : label) cps.push_back(static_cast<std::uint32_t>(ch));
  atlas.ensureGlyphs(cps.data(), static_cast<std::uint32_t>(cps.size()));
  check(atlas.isDirty(), "atlas dirty after ensureGlyphs (backend will re-upload)");

  dc::TextLayoutResult layout = dc::layoutText(
      atlas, label.c_str(), static_cast<float>(clipX), static_cast<float>(clipY),
      static_cast<float>(fontSize), static_cast<float>(atlas.glyphPx()));
  std::printf("layoutText: %d glyph instances for \"%s\"\n", layout.glyphCount,
              label.c_str());
  check(layout.glyphCount == 5, "5 visible glyph instances for \"Label\"");

  store.setCpuData(
      10, layout.glyphInstances.data(),
      static_cast<std::uint32_t>(layout.glyphInstances.size() * sizeof(float)));
  requireOk(cp.applyJsonText(
      std::string(R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":)") +
      std::to_string(layout.glyphCount) + "}"), "setVC");

  // --- Render the whole scene through the full walk (the host's render()). ----
  dc::Stats stats = renderer.render(scene, store, W, H);
  std::printf("render: drawCalls=%u culled=%u\n", stats.drawCalls,
              stats.culledDrawCalls);
  check(stats.drawCalls >= 1, "textSDF@1 issued a draw call (atlas wired in)");
  check(!atlas.isDirty(), "atlas clean after render (SDF texture uploaded)");

  // --- Read back and classify pixels. Cyan = (0,255,255): a stroke pixel has
  //     strong G+B and near-zero R. -------------------------------------------
  int strongStroke = 0;   // SDF interior (alpha ~1)
  int edgePartial = 0;    // anti-aliased SDF edge (0 < alpha < 1)
  int contaminated = 0;   // red leaked (would mean a wrong atlas sample)
  int minStrokeX = W, maxStrokeX = -1;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      std::uint8_t p[4];
      renderer.device().readPixel(x, y, p);
      const int r = p[0], g = p[1], b = p[2];
      const int gb = (g < b ? g : b);
      if (r > 40) ++contaminated;
      if (gb >= 200) {
        ++strongStroke;
        if (x < minStrokeX) minStrokeX = x;
        if (x > maxStrokeX) maxStrokeX = x;
      } else if (gb >= 24) {
        ++edgePartial;
      }
    }
  }
  std::printf("pixels: strongStroke=%d edgePartial=%d contaminated=%d "
              "strokeXspan=[%d,%d]\n",
              strongStroke, edgePartial, contaminated, minStrokeX, maxStrokeX);

  // Non-blank text: a healthy count of stroke pixels in the text color.
  check(strongStroke > 30, "label renders non-blank text pixels in the text color");
  // SDF reconstruction -> anti-aliased partial-coverage edge pixels.
  check(edgePartial > 10, "SDF edge produces anti-aliased partial-coverage pixels");
  // A 5-glyph word spans horizontally — proves the multi-glyph layout advanced
  // the cursor (not one stacked glyph).
  check(maxStrokeX - minStrokeX > 20, "glyphs span horizontally (cursor advanced)");
  // No red contamination -> the R8 SDF atlas was sampled correctly.
  check(contaminated == 0, "no red contamination (atlas sampled correctly)");

  std::printf("=== ENC-589 host text wiring: %d passed, %d failed (on %s) ===\n",
              passed, failed, renderer.device().backendName().c_str());
  return failed > 0 ? 1 : 0;
#endif
}
