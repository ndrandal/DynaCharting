// ENC-501 (P5 cutover) — Dawn-only golden test for the textSDF@1 pipeline
// (axis/labels feature area; d12_6_axis_gl / d3_3 conformance).
//
// Originally (ENC-510) this rendered the same "Hi" glyph run through BOTH the GL
// backend and the DawnSceneRenderer and compared the readbacks within a wide SDF
// tolerance. Dawn is now the proven default renderer and dc_gl is being deleted
// (ENC-501), so this renders ONLY via Dawn and self-validates the readback the way
// the dedicated d3_3_dawn_text_sdf test does: scan the frame and assert
//   * a healthy count of STRONG-STROKE text-colored pixels (SDF alpha ~1 inside),
//   * those strong-stroke pixels actually ARE the text color (cyan),
//   * the background/outside-glyph region stays clear,
//   * the SDF edge produces PARTIAL-COVERAGE (anti-aliased) pixels (smoothstep,
//     not a 1-bit cutout).
// This is the self-validating golden for SDF text: rather than a brittle per-pixel
// golden over an all-AA frame, it asserts the structural properties that the GL<->
// Dawn parity run confirmed (glyphs land, are the text color, are anti-aliased).
// Needs the test font; skips gracefully without it.
#include "parity_golden.hpp"

#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using dc::golden::BufferData;
using dc::golden::GoldenFrame;
using dc::golden::renderDawn;

static int g_passed = 0;
static int g_failed = 0;
static void check(bool cond, const char* name) {
  if (cond) { std::printf("  PASS: %s\n", name); ++g_passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++g_failed; }
}

int main() {
#ifndef FONT_PATH
  std::printf("parity-text: SKIPPED (no FONT_PATH)\n");
  return 0;
#else
  std::printf("=== ENC-501 Dawn-only golden: textSDF@1 ===\n");
  constexpr int W = 128, H = 64;

  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  if (!atlas.loadFontFile(FONT_PATH)) {
    std::printf("parity-text: SKIPPED (font load failed)\n");
    return 0;
  }
  atlas.ensureAscii();

  // Build the glyph instance buffer ("Hi") — the SAME placement the parity test used.
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
  dc::golden::SceneBuilder builder =
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

  GoldenFrame f = renderDawn("axis-text/textSDF", builder, W, H, &atlas);
  if (f.skipped) {
    std::printf("parity-text: SKIPPED (%s)\n", f.skipReason.c_str());
    return 0;
  }
  std::printf("dawn=%s glyphs=%d\n", f.dawnBackend.c_str(), glyphCount);

  // Scan the readback for SDF-text structure (the self-validating golden — the GL<->
  // Dawn parity run confirmed the glyphs land, are cyan, and are anti-aliased).
  int strongStroke = 0;   // text color, full intensity (SDF alpha ~1 inside glyph)
  int edgePartial = 0;    // text color present but partial (AA fringe)
  int background = 0;     // clear/near-black (no spurious coverage)
  int contaminated = 0;   // unexpected red leakage
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      const std::uint8_t* px = f.at(x, y);
      const int r = px[0], g = px[1], b = px[2];
      if (g > 200 && b > 200 && r < 40) ++strongStroke;       // strong cyan
      else if ((g > 40 && b > 40) && g < 200 && b < 200 && r < 40)
        ++edgePartial;                                          // partial cyan
      if (r < 16 && g < 16 && b < 16) ++background;
      if (r > 60 && g < 30 && b < 30) ++contaminated;          // stray red
    }
  }
  std::printf("strongStroke=%d edgePartial=%d background=%d contaminated=%d\n",
              strongStroke, edgePartial, background, contaminated);

  check(glyphCount == 2, "2 visible glyphs for 'Hi'");
  check(strongStroke > 20, "glyph stroke pixels are the text color (SDF alpha ~1 inside)");
  check(edgePartial > 5, "SDF edge produces partial-coverage (anti-aliased) pixels");
  check(background > strongStroke, "background/outside-glyph pixels are clear");
  check(contaminated == 0, "no red contamination (atlas sampled correctly)");

  std::printf("=== parity-text: %d passed, %d failed ===\n", g_passed, g_failed);
  return g_failed > 0 ? 1 : 0;
#endif
}
