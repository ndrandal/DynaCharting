#pragma once
#include "dc/text/GlyphAtlas.hpp"
#include <cstdint>
#include <vector>

namespace dc {

struct TextLayoutResult {
  std::vector<float> glyphInstances; // glyph8: x0,y0,x1,y1,u0,v0,u1,v1
  int glyphCount{0};
  float advanceWidth{0};
};

// Lay `text` out from a baseline origin, in CLIP units.
//
// `fontSize` is the text's ascent-to-descent height in CLIP units: the atlas
// rasterizes at `glyphPx` with `stbtt_ScaleForPixelHeight(glyphPx)`, so the em
// box is exactly `glyphPx` atlas pixels tall and every metric is scaled by
// `fontSize / glyphPx`.
//
// `xScale` (ENC-1253) scales the HORIZONTAL contribution only — the advance,
// the left bearing and the glyph width. It exists because clip space is not
// square: one clip unit is `w/2` pixels across and `h/2` pixels down, so a
// single isotropic scale renders every string stretched by the canvas's aspect
// ratio (1.5x on a 1800x1200 target). Nothing downstream compensated for that —
// the glyph quads go straight to NDC in the textSDF@1 vertex shader — so text
// authored before this parameter existed was simply wide. Pass `h/w` to get
// glyphs whose on-screen proportions match the font; the default of 1 preserves
// the pre-ENC-1253 behaviour exactly for existing callers.
inline TextLayoutResult layoutText(const GlyphAtlas& atlas, const char* text,
                                    float startX, float baselineY,
                                    float fontSize, float glyphPx,
                                    float xScale = 1.0f) {
  TextLayoutResult r;
  float cursorX = startX;
  float scale = fontSize / glyphPx;
  float xs = scale * xScale;

  for (const char* p = text; *p; p++) {
    const GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(static_cast<unsigned char>(*p)));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += g->advance * xs;
      continue;
    }
    float x0 = cursorX + g->bearingX * xs;
    float y1 = baselineY + g->bearingY * scale;
    float y0 = y1 - g->h * scale;
    float x1 = x0 + g->w * xs;
    r.glyphInstances.push_back(x0); r.glyphInstances.push_back(y0);
    r.glyphInstances.push_back(x1); r.glyphInstances.push_back(y1);
    r.glyphInstances.push_back(g->u0); r.glyphInstances.push_back(g->v0);
    r.glyphInstances.push_back(g->u1); r.glyphInstances.push_back(g->v1);
    r.glyphCount++;
    cursorX += g->advance * xs;
  }
  r.advanceWidth = cursorX - startX;
  return r;
}

inline TextLayoutResult layoutTextRightAligned(const GlyphAtlas& atlas, const char* text,
                                                float endX, float baselineY,
                                                float fontSize, float glyphPx,
                                                float xScale = 1.0f) {
  // First pass: measure width
  float xs = (fontSize / glyphPx) * xScale;
  float width = 0;
  for (const char* p = text; *p; p++) {
    const GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(static_cast<unsigned char>(*p)));
    if (g) width += g->advance * xs;
  }
  return layoutText(atlas, text, endX - width, baselineY, fontSize, glyphPx, xScale);
}

} // namespace dc
