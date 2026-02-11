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

inline TextLayoutResult layoutText(const GlyphAtlas& atlas, const char* text,
                                    float startX, float baselineY,
                                    float fontSize, float glyphPx) {
  TextLayoutResult r;
  float cursorX = startX;
  float scale = fontSize / glyphPx;

  for (const char* p = text; *p; p++) {
    const GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (!g) continue;
    if (g->w <= 0 || g->h <= 0) {
      cursorX += g->advance * scale;
      continue;
    }
    float x0 = cursorX + g->bearingX * scale;
    float y1 = baselineY + g->bearingY * scale;
    float y0 = y1 - g->h * scale;
    float x1 = x0 + g->w * scale;
    r.glyphInstances.push_back(x0); r.glyphInstances.push_back(y0);
    r.glyphInstances.push_back(x1); r.glyphInstances.push_back(y1);
    r.glyphInstances.push_back(g->u0); r.glyphInstances.push_back(g->v0);
    r.glyphInstances.push_back(g->u1); r.glyphInstances.push_back(g->v1);
    r.glyphCount++;
    cursorX += g->advance * scale;
  }
  r.advanceWidth = cursorX - startX;
  return r;
}

inline TextLayoutResult layoutTextRightAligned(const GlyphAtlas& atlas, const char* text,
                                                float endX, float baselineY,
                                                float fontSize, float glyphPx) {
  // First pass: measure width
  float scale = fontSize / glyphPx;
  float width = 0;
  for (const char* p = text; *p; p++) {
    const GlyphInfo* g = atlas.getGlyph(static_cast<std::uint32_t>(*p));
    if (g) width += g->advance * scale;
  }
  return layoutText(atlas, text, endX - width, baselineY, fontSize, glyphPx);
}

} // namespace dc
