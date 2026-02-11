#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace dc {

GlyphAtlas::GlyphAtlas() {
  setAtlasSize(atlasSize_);
}

void GlyphAtlas::setAtlasSize(std::uint32_t s) {
  atlasSize_ = s;
  atlas_.assign(static_cast<std::size_t>(s) * s, 0);
  shelves_.clear();
  shelves_.push_back({1, 1, 0});
  glyphs_.clear();
  dirty_ = true;
}

bool GlyphAtlas::loadFont(const std::uint8_t* data, std::uint32_t len) {
  fontData_.assign(data, data + len);
  fontLoaded_ = true;
  return true;
}

bool GlyphAtlas::loadFontFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return false;
  auto sz = f.tellg();
  if (sz <= 0) return false;
  fontData_.resize(static_cast<std::size_t>(sz));
  f.seekg(0);
  f.read(reinterpret_cast<char*>(fontData_.data()), sz);
  fontLoaded_ = true;
  return true;
}

bool GlyphAtlas::ensureAscii() {
  std::vector<std::uint32_t> cp;
  for (std::uint32_t c = 32; c <= 126; c++) cp.push_back(c);
  return ensureGlyphs(cp.data(), static_cast<std::uint32_t>(cp.size()));
}

bool GlyphAtlas::ensureGlyphs(const std::uint32_t* codepoints, std::uint32_t count) {
  if (!fontLoaded_) return false;

  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, fontData_.data(), 0)) {
    std::fprintf(stderr, "GlyphAtlas: stbtt_InitFont failed\n");
    return false;
  }

  float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(glyphPx_));
  bool modified = false;

  for (std::uint32_t i = 0; i < count; i++) {
    std::uint32_t cp = codepoints[i];
    if (glyphs_.find(cp) != glyphs_.end()) continue;

    int glyphIdx = stbtt_FindGlyphIndex(&font, static_cast<int>(cp));

    // Get metrics
    int advW = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&font, glyphIdx, &advW, &lsb);

    int ix0, iy0, ix1, iy1;
    stbtt_GetGlyphBitmapBox(&font, glyphIdx, scale, scale, &ix0, &iy0, &ix1, &iy1);

    int gw = ix1 - ix0;
    int gh = iy1 - iy0;

    if (gw <= 0 || gh <= 0) {
      // Whitespace glyph — no bitmap, just metrics
      GlyphInfo info;
      info.codepoint = cp;
      info.advance = static_cast<float>(advW) * scale;
      info.bearingX = static_cast<float>(ix0);
      info.bearingY = static_cast<float>(-iy0);
      info.w = 0;
      info.h = 0;
      glyphs_[cp] = info;
      continue;
    }

    // Rasterize glyph at high resolution
    std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(gw) * gh, 0);
    stbtt_MakeGlyphBitmap(&font, bitmap.data(), gw, gh, gw, scale, scale, glyphIdx);

    // Build SDF from rasterized bitmap
    std::uint32_t sdfW = static_cast<std::uint32_t>(gw);
    std::uint32_t sdfH = static_cast<std::uint32_t>(gh);
    std::vector<std::uint8_t> sdf(static_cast<std::size_t>(sdfW) * sdfH);
    buildSdfR8(bitmap.data(), sdfW, sdfH, sdfRange_, sdf.data());

    // Pack into atlas
    std::uint32_t cellW = sdfW + pad_ * 2;
    std::uint32_t cellH = sdfH + pad_ * 2;
    std::uint32_t ax, ay;
    if (!packGlyph(cellW, cellH, ax, ay)) {
      std::fprintf(stderr, "GlyphAtlas: atlas full (cp=%u)\n", cp);
      continue;
    }

    // Copy SDF into atlas at (ax+pad, ay+pad)
    for (std::uint32_t row = 0; row < sdfH; row++) {
      std::uint32_t dstY = ay + pad_ + row;
      std::uint32_t dstX = ax + pad_;
      std::memcpy(&atlas_[dstY * atlasSize_ + dstX],
                   &sdf[row * sdfW],
                   sdfW);
    }

    float invAtlas = 1.0f / static_cast<float>(atlasSize_);
    GlyphInfo info;
    info.codepoint = cp;
    info.u0 = static_cast<float>(ax + pad_) * invAtlas;
    info.u1 = static_cast<float>(ax + pad_ + sdfW) * invAtlas;
    // V coordinates: OpenGL row 0 = bottom (V=0), but our atlas row 0 = top.
    // Swap so v0 = bottom of glyph (larger atlas row → larger V) and
    // v1 = top of glyph (smaller atlas row → smaller V).
    // The shader maps uv.y 0→1 (bottom→top of quad) to v0→v1, giving
    // correct orientation.
    info.v0 = static_cast<float>(ay + pad_ + sdfH) * invAtlas;
    info.v1 = static_cast<float>(ay + pad_) * invAtlas;
    info.advance = static_cast<float>(advW) * scale;
    info.bearingX = static_cast<float>(ix0);
    info.bearingY = static_cast<float>(-iy0);
    info.w = static_cast<float>(gw);
    info.h = static_cast<float>(gh);
    glyphs_[cp] = info;
    modified = true;
  }

  if (modified) dirty_ = true;
  return modified;
}

const GlyphInfo* GlyphAtlas::getGlyph(std::uint32_t codepoint) const {
  auto it = glyphs_.find(codepoint);
  return it == glyphs_.end() ? nullptr : &it->second;
}

bool GlyphAtlas::packGlyph(std::uint32_t w, std::uint32_t h,
                            std::uint32_t& outX, std::uint32_t& outY) {
  for (auto& shelf : shelves_) {
    if (shelf.x + w <= atlasSize_ - 1 && shelf.y + h <= atlasSize_ - 1) {
      if (shelf.h == 0) shelf.h = h;
      if (h <= shelf.h) {
        outX = shelf.x;
        outY = shelf.y;
        shelf.x += w;
        return true;
      }
    }
  }

  // New shelf
  auto& last = shelves_.back();
  std::uint32_t ny = last.y + last.h;
  if (ny + h > atlasSize_ - 1) return false;

  outX = 1;
  outY = ny;
  shelves_.push_back({1 + w, ny, h});
  return true;
}

// 2-pass chamfer distance transform
void GlyphAtlas::distanceTransform(float* field, std::uint32_t w, std::uint32_t h) {
  constexpr float INF = 1e20f;
  constexpr float DIAG = 1.4142135f;

  // Forward pass (top-left to bottom-right)
  for (std::uint32_t y = 0; y < h; y++) {
    for (std::uint32_t x = 0; x < w; x++) {
      std::size_t i = static_cast<std::size_t>(y) * w + x;
      if (field[i] == 0.0f) continue; // outside — distance is 0

      field[i] = INF;
      if (x > 0)
        field[i] = std::min(field[i], field[i - 1] + 1.0f);
      if (y > 0)
        field[i] = std::min(field[i], field[i - w] + 1.0f);
      if (x > 0 && y > 0)
        field[i] = std::min(field[i], field[i - w - 1] + DIAG);
      if (x + 1 < w && y > 0)
        field[i] = std::min(field[i], field[i - w + 1] + DIAG);
    }
  }

  // Backward pass (bottom-right to top-left)
  for (std::uint32_t y = h; y-- > 0; ) {
    for (std::uint32_t x = w; x-- > 0; ) {
      std::size_t i = static_cast<std::size_t>(y) * w + x;
      if (field[i] == 0.0f) continue;

      if (x + 1 < w)
        field[i] = std::min(field[i], field[i + 1] + 1.0f);
      if (y + 1 < h)
        field[i] = std::min(field[i], field[i + w] + 1.0f);
      if (x + 1 < w && y + 1 < h)
        field[i] = std::min(field[i], field[i + w + 1] + DIAG);
      if (x > 0 && y + 1 < h)
        field[i] = std::min(field[i], field[i + w - 1] + DIAG);
    }
  }
}

void GlyphAtlas::buildSdfR8(const std::uint8_t* alpha, std::uint32_t w, std::uint32_t h,
                              std::uint32_t sdfRange, std::uint8_t* out) {
  std::size_t n = static_cast<std::size_t>(w) * h;
  float rangePx = static_cast<float>(sdfRange);

  // Build inside/outside masks as float fields
  std::vector<float> inside(n);
  std::vector<float> outside(n);
  for (std::size_t i = 0; i < n; i++) {
    bool isIn = alpha[i] > 127;
    inside[i]  = isIn ? 1.0f : 0.0f;
    outside[i] = isIn ? 0.0f : 1.0f;
  }

  distanceTransform(inside.data(), w, h);
  distanceTransform(outside.data(), w, h);

  for (std::size_t i = 0; i < n; i++) {
    float sd = inside[i] - outside[i]; // positive inside, negative outside
    float clamped = std::max(-rangePx, std::min(rangePx, sd));
    float normalized = 128.0f + (clamped / rangePx) * 127.0f;
    out[i] = static_cast<std::uint8_t>(std::max(0.0f, std::min(255.0f, normalized)));
  }
}

} // namespace dc
