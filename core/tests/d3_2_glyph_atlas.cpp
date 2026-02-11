#include "dc/text/GlyphAtlas.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  dc::GlyphAtlas atlas;
  atlas.setAtlasSize(512);
  atlas.setGlyphPx(48);
  atlas.setSdfRange(12);

  // Test 1: Load font file
  bool loaded = atlas.loadFontFile(FONT_PATH);
  requireTrue(loaded, "font file loaded");
  std::printf("Font loaded\n");

  // Test 2: Ensure ASCII glyphs
  bool modified = atlas.ensureAscii();
  requireTrue(modified, "ensureAscii returned modified=true");
  requireTrue(atlas.isDirty(), "atlas is dirty after ensureAscii");
  std::printf("ASCII glyphs rasterized\n");

  // Test 3: Check glyph info for 'A'
  const dc::GlyphInfo* gA = atlas.getGlyph('A');
  requireTrue(gA != nullptr, "glyph 'A' found");
  requireTrue(gA->advance > 0, "glyph 'A' has positive advance");
  requireTrue(gA->w > 0, "glyph 'A' has positive width");
  requireTrue(gA->h > 0, "glyph 'A' has positive height");
  requireTrue(gA->u0 >= 0 && gA->u1 <= 1.0f, "glyph 'A' UVs in range");
  requireTrue(gA->u1 > gA->u0, "glyph 'A' u1 > u0");
  requireTrue(gA->v0 > gA->v1, "glyph 'A' v0 > v1 (GL-flipped V)");
  std::printf("Glyph 'A': advance=%.1f w=%.0f h=%.0f UV=(%.3f,%.3f)-(%.3f,%.3f)\n",
              gA->advance, gA->w, gA->h, gA->u0, gA->v0, gA->u1, gA->v1);

  // Test 4: Check space glyph (no bitmap)
  const dc::GlyphInfo* gSpc = atlas.getGlyph(' ');
  requireTrue(gSpc != nullptr, "glyph ' ' found");
  requireTrue(gSpc->advance > 0, "space has positive advance");
  requireTrue(gSpc->w == 0, "space has zero width bitmap");
  std::printf("Space glyph: advance=%.1f\n", gSpc->advance);

  // Test 5: Atlas data — check 'I' which has solid center
  // Note: v1 is the smaller V (top of glyph in atlas array), v0 is the larger V (bottom).
  const dc::GlyphInfo* gI = atlas.getGlyph('I');
  requireTrue(gI != nullptr, "glyph 'I' found");
  std::uint32_t atlasSize = atlas.atlasSize();
  const std::uint8_t* data = atlas.atlasData();
  std::uint32_t ix = static_cast<std::uint32_t>(gI->u0 * static_cast<float>(atlasSize));
  std::uint32_t iy = static_cast<std::uint32_t>(gI->v1 * static_cast<float>(atlasSize)); // v1 = top row
  std::uint32_t midX = ix + static_cast<std::uint32_t>(gI->w / 2);
  std::uint32_t midY = iy + static_cast<std::uint32_t>(gI->h / 2);
  std::uint8_t centerVal = data[midY * atlasSize + midX];
  std::printf("Atlas center of 'I' at (%u,%u): SDF value=%u\n", midX, midY, centerVal);
  requireTrue(centerVal > 128, "center of 'I' is inside (SDF > 128)");

  // Also verify some pixels in 'A' glyph are inside (not all are in the hole)
  int insideCount = 0;
  std::uint32_t ax = static_cast<std::uint32_t>(gA->u0 * static_cast<float>(atlasSize));
  std::uint32_t ay = static_cast<std::uint32_t>(gA->v1 * static_cast<float>(atlasSize)); // v1 = top row
  for (std::uint32_t r = 0; r < static_cast<std::uint32_t>(gA->h); r++) {
    for (std::uint32_t c = 0; c < static_cast<std::uint32_t>(gA->w); c++) {
      if (data[(ay + r) * atlasSize + ax + c] > 128) insideCount++;
    }
  }
  std::printf("Glyph 'A': %d pixels with SDF > 128 (inside)\n", insideCount);
  requireTrue(insideCount > 10, "glyph 'A' has inside pixels");

  // Test 6: Re-ensure same glyphs should not modify
  atlas.clearDirty();
  bool reModified = atlas.ensureAscii();
  requireTrue(!reModified, "re-ensureAscii returned modified=false");
  requireTrue(!atlas.isDirty(), "atlas not dirty after re-ensure");
  std::printf("Re-ensure: no modification (correct)\n");

  // Test 7: Multiple different glyphs exist
  int count = 0;
  for (std::uint32_t c = 33; c <= 126; c++) {
    if (atlas.getGlyph(c)) count++;
  }
  std::printf("Visible ASCII glyphs: %d/94\n", count);
  requireTrue(count == 94, "all 94 visible ASCII glyphs present");

  std::printf("\nD3.2 glyph atlas PASS\n");
  return 0;
}
