#pragma once
#include "dc/ids/Id.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

struct GlyphInfo {
  std::uint32_t codepoint{0};
  // UV in atlas [0..1]
  float u0{0}, v0{0}, u1{0}, v1{0};
  // Metrics in pixels (for layout at font size)
  float advance{0};
  float bearingX{0}, bearingY{0};
  float w{0}, h{0};
};

class GlyphAtlas {
public:
  GlyphAtlas();

  // Load a TTF/OTF from memory.
  bool loadFont(const std::uint8_t* data, std::uint32_t len);

  // Load a TTF/OTF from file.
  bool loadFontFile(const std::string& path);

  // Ensure glyphs for a set of codepoints are rasterized and packed.
  // Returns true if atlas was modified (needs re-upload).
  bool ensureGlyphs(const std::uint32_t* codepoints, std::uint32_t count);

  // Convenience: ensure ASCII printable (32..126).
  bool ensureAscii();

  // Lookup glyph info. Returns nullptr if not rasterized.
  const GlyphInfo* getGlyph(std::uint32_t codepoint) const;

  // Atlas R8 pixel data.
  const std::uint8_t* atlasData() const { return atlas_.data(); }
  std::uint32_t atlasSize() const { return atlasSize_; }

  // True if atlas pixels changed since last call to clearDirty().
  bool isDirty() const { return dirty_; }
  void clearDirty() { dirty_ = false; }

  // Parameters
  void setGlyphPx(std::uint32_t px) { glyphPx_ = px; }
  void setSdfRange(std::uint32_t r) { sdfRange_ = r; }
  void setAtlasSize(std::uint32_t s);

  // When false, stores raw rasterized alpha instead of SDF.
  // Gives pixel-perfect text quality at the rasterized size.
  void setUseSdf(bool v) { useSdf_ = v; }
  bool useSdf() const { return useSdf_; }

private:
  std::uint32_t atlasSize_{1024};
  std::uint32_t glyphPx_{48};
  std::uint32_t sdfRange_{12};
  std::uint32_t pad_{2};
  bool useSdf_{true};

  std::vector<std::uint8_t> atlas_;    // R8 atlas (atlasSize_ x atlasSize_)
  std::vector<std::uint8_t> fontData_; // retained font file bytes
  bool fontLoaded_{false};
  bool dirty_{false};

  std::unordered_map<std::uint32_t, GlyphInfo> glyphs_;

  // Shelf packer state
  struct Shelf {
    std::uint32_t x, y, h;
  };
  std::vector<Shelf> shelves_;

  bool packGlyph(std::uint32_t w, std::uint32_t h,
                 std::uint32_t& outX, std::uint32_t& outY);

  // SDF helpers
  static void distanceTransform(float* field, std::uint32_t w, std::uint32_t h);
  static void buildSdfR8(const std::uint8_t* alpha, std::uint32_t w, std::uint32_t h,
                         std::uint32_t sdfRange, std::uint8_t* out);
};

} // namespace dc
