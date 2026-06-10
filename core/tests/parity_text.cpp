// ENC-510 (P5.0b) — GL <-> Dawn parity for the textSDF@1 pipeline (axis/labels
// feature area; d12_6_axis_gl / d3_3 conformance). Renders the same "Hi" glyph
// run through both backends (each with the SAME GlyphAtlas) and compares.
//
// WHY TEXT IS A SEPARATE TEST WITH A WIDER TOLERANCE
// --------------------------------------------------
// Text is SDF coverage: every glyph pixel is a smoothstep around the 0.5 isoline,
// so almost EVERY lit pixel is a partial-coverage (anti-aliased) pixel, and the
// GL (OSMesa/llvmpipe) and Dawn (Vulkan/lavapipe) samplers reconstruct that
// coverage slightly differently (bilinear filtering + smoothstep rounding). So
// unlike the solid-fill areas, text has NO large flat-fill core to match
// byte-exactly — the parity bar is "the same glyphs land in the same place with
// the same color", expressed as a moderate channelTol + a higher allowed
// mismatch%. This is a documented, expected GL<->Dawn divergence class, not a
// bug. Needs the test font; skips gracefully without it.
#include "parity_harness.hpp"

#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <string>
#include <vector>

using dc::parity::BufferData;
using dc::parity::compareScene;
using dc::parity::ParityResult;
using dc::parity::Tolerance;

int main() {
#ifndef FONT_PATH
  std::printf("parity-text: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  std::printf("=== ENC-510 GL<->Dawn parity: textSDF@1 ===\n");
  constexpr int W = 128, H = 64;

  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  if (!atlas.loadFontFile(FONT_PATH)) {
    std::printf("parity-text: SKIPPED (font load failed)\n");
    return 0;
  }
  atlas.ensureAscii();

  // Build the glyph instance buffer ("Hi") ONCE; both backends consume the same
  // bytes. NOTE on baseline sign: both Renderer (GL) and the Dawn backends apply
  // the SAME u_transform to glyph quads; the only y convention difference is the
  // readback origin, which the harness's row-flip mapping already accounts for.
  // So we place "Hi" near clip center and let the harness align the rows.
  const char* text = "Hi";
  const float fontSize = 0.6f;
  const float baselineY = -0.15f;
  float cursorX = -0.35f;
  std::vector<float> inst;
  int glyphCount = 0;
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

  std::vector<float> instCopy = inst;
  int gc = glyphCount;
  dc::parity::SceneBuilder builder =
      [instCopy, gc](dc::CommandProcessor& cp, dc::Scene&) -> std::vector<BufferData> {
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
    cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})");
    cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"glyph8"})");
    cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"textSDF@1","geometryId":100})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":1,"a":1})");
    cp.applyJsonText(
        std::string(R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":)") +
        std::to_string(gc) + "}");
    return {BufferData(10, instCopy.data(), instCopy.size() * sizeof(float))};
  };

  // SDF coverage: moderate channel tolerance (filtering/smoothstep round-off)
  // and a higher allowed mismatch% (most glyph pixels are AA fringe).
  Tolerance text_tol;
  text_tol.channelTol = 60;
  text_tol.maxMismatchPct = 14.0;

  ParityResult r = compareScene("axis-text/textSDF", builder, W, H, text_tol, &atlas);
  if (r.skipped) {
    std::printf("parity-text: SKIPPED (%s)\n", r.skipReason.c_str());
    return 0;
  }
  std::printf("=== parity-text: %s (glyphs=%d) ===\n",
              r.passed ? "PASS" : "FAIL", glyphCount);
  return r.passed ? 0 : 1;
#endif
}
