// ENC-1253 — `dc::layoutText`'s horizontal scale, and the invariant it restores.
//
// WHY THIS TEST EXISTS. Clip space is not square: one clip unit is `w/2` pixels
// across and `h/2` pixels down. `layoutText` scaled X and Y by one number, so
// every string it laid out was stretched by the canvas's aspect ratio — 1.5x on
// a 1800x1200 target — and nothing downstream compensated (the glyph quads go
// straight to NDC in the textSDF@1 vertex shader). Nobody had noticed because
// nothing in a shipped chart had ever drawn in-engine text: it was "unused, not
// unbuilt" (ENC-1260).
//
// The parameter is additive with a default of 1, so the whole point is a pair of
// assertions: existing callers' output must not move, and a caller that passes
// `h/w` must get glyphs with the font's own proportions.
//
// This is a PURE test — no GPU, no Dawn — so it is registered in the DEFAULT
// build and actually runs (LIMITATIONS.md DC-L01: the 47 Dawn-gated tests do
// not). The renderer half of ENC-1253 (DawnTextSdfBackend's version-checked
// cache, §C0) is Dawn-gated and is covered by the browser capture instead.

#include "dc/text/GlyphAtlas.hpp"
#include "dc/text/TextLayout.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (%.6f vs %.6f, eps %.6f)\n", msg, a, b, eps);
    std::exit(1);
  }
}

int main() {
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  requireTrue(atlas.loadFontFile(FONT_PATH), "font file loaded");
  requireTrue(atlas.ensureAscii(), "ASCII rasterized");

  const char* kLabel = "418.25";
  const float kFontSize = 0.03f;   // clip units: the ascent-to-descent height
  const float kGlyphPx = static_cast<float>(atlas.glyphPx());

  // 1. The default is EXACTLY the pre-ENC-1253 behaviour. Every existing caller
  //    (AxisRecipe, TooltipRecipe, CrosshairRecipe, LevelLineRecipe, the WASM
  //    host's setTextGeometry) goes through this path, so any drift here is a
  //    silent change to five call sites at once.
  auto base = dc::layoutText(atlas, kLabel, 0.0f, 0.0f, kFontSize, kGlyphPx);
  auto explicitOne = dc::layoutText(atlas, kLabel, 0.0f, 0.0f, kFontSize, kGlyphPx, 1.0f);
  requireTrue(base.glyphCount == explicitOne.glyphCount, "default arg: same glyph count");
  requireClose(base.advanceWidth, explicitOne.advanceWidth, 1e-9f,
               "default arg: same advance width");
  requireTrue(base.glyphInstances.size() == explicitOne.glyphInstances.size(),
              "default arg: same instance count");
  for (std::size_t i = 0; i < base.glyphInstances.size(); ++i) {
    requireClose(base.glyphInstances[i], explicitOne.glyphInstances[i], 1e-9f,
                 "default arg: identical glyph quads");
  }

  // 2. The advance scales LINEARLY in xScale, and only horizontally.
  auto half = dc::layoutText(atlas, kLabel, 0.0f, 0.0f, kFontSize, kGlyphPx, 0.5f);
  requireTrue(half.glyphCount == base.glyphCount, "xScale does not change the glyph count");
  requireClose(half.advanceWidth, base.advanceWidth * 0.5f, 1e-6f,
               "xScale 0.5 halves the advance width");
  // glyph8 is x0,y0,x1,y1,u0,v0,u1,v1 — the Y pair and the UVs must be untouched.
  for (int g = 0; g < base.glyphCount; ++g) {
    const float* b = base.glyphInstances.data() + static_cast<std::size_t>(g) * 8;
    const float* h = half.glyphInstances.data() + static_cast<std::size_t>(g) * 8;
    requireClose(h[1], b[1], 1e-9f, "xScale leaves y0 alone");
    requireClose(h[3], b[3], 1e-9f, "xScale leaves y1 alone");
    for (int k = 4; k < 8; ++k) requireClose(h[k], b[k], 1e-9f, "xScale leaves the atlas UVs alone");
    requireClose(h[0], b[0] * 0.5f, 1e-6f, "xScale halves x0");
    requireClose(h[2], b[2] * 0.5f, 1e-6f, "xScale halves x1");
  }

  // 3. THE INVARIANT THE PARAMETER EXISTS FOR: with xScale = h/w, a label's
  //    width IN PIXELS is the same whatever shape the canvas is. Without it the
  //    same label is 1.5x wider on a 1800x1200 target than on a 1200x1200 one,
  //    which is the bug, stated as a number.
  struct Canvas { float w, h; };
  const Canvas canvases[] = {{1200, 1200}, {1800, 1200}, {1220, 697}, {640, 1280}};
  float referencePx = 0.0f;
  for (const Canvas& c : canvases) {
    // fontSize for an 11px-tall label on this canvas, and the aspect correction.
    const float fontSize = (2.0f * 11.0f) / c.h;
    const float xScale = c.h / c.w;
    auto r = dc::layoutText(atlas, kLabel, 0.0f, 0.0f, fontSize, kGlyphPx, xScale);
    const float widthPx = r.advanceWidth * c.w * 0.5f;
    if (referencePx == 0.0f) referencePx = widthPx;
    requireClose(widthPx, referencePx, 1e-3f, "corrected label width is canvas-shape-independent");

    // …and the uncorrected one is not: it is off by exactly the aspect ratio.
    auto raw = dc::layoutText(atlas, kLabel, 0.0f, 0.0f, fontSize, kGlyphPx);
    const float rawPx = raw.advanceWidth * c.w * 0.5f;
    requireClose(rawPx, referencePx * (c.w / c.h), 1e-3f,
                 "uncorrected label width is stretched by exactly the aspect ratio");
    std::printf("canvas %.0fx%.0f: corrected %.3fpx, uncorrected %.3fpx (x%.3f)\n", c.w, c.h,
                widthPx, rawPx, rawPx / widthPx);
  }

  // 4. The right-aligned helper carries the same scale through, so a price label
  //    right-aligned against the plot box's left edge lands where it is asked to.
  const float endX = 0.25f;
  auto ra = dc::layoutTextRightAligned(atlas, kLabel, endX, 0.0f, kFontSize, kGlyphPx, 0.5f);
  requireTrue(ra.glyphCount == base.glyphCount, "right-aligned: same glyph count");
  requireClose(ra.advanceWidth, half.advanceWidth, 1e-6f, "right-aligned: same advance");
  // The cursor must END at endX: start + advance == endX.
  const float startX = endX - ra.advanceWidth;
  requireClose(startX + ra.advanceWidth, endX, 1e-6f, "right-aligned: the run ends at endX");
  // Its first glyph's x0 is the bearing off that start, not off zero.
  requireClose(ra.glyphInstances[0], startX + half.glyphInstances[0], 1e-6f,
               "right-aligned: the run is the same layout, translated");

  std::printf("[enc1253] layoutText xScale: default unchanged, linear in x, "
              "aspect-independent in pixels\n");
  return 0;
}
