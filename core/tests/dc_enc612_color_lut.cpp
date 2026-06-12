// ENC-612 (P2) — 256x1 color-ramp LUT bytes: the BYTE-LEVEL proof that
// buildColorLut() bakes a ColorScale ramp into the exact 256x1 RGBA8 texel row
// the texturedQuad LUT path uploads + samples in-shader (RESEARCH §4.2 last
// paragraph). The sibling Dawn render test (dc_enc612_dawn_lut) proves the GPU
// samples it as a smooth gradient; this proves the bytes are correct + exact.
#include "dc/scale/ColorLut.hpp"
#include "dc/scale/ColorScale.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

static int passed = 0;
static int failed = 0;

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
  std::printf("=== ENC-612: 256x1 color-ramp LUT bytes ===\n");

  // A simple 2-stop blue->red ramp: t=0 pure blue, t=1 pure red. Linear lerp in
  // 8-bit space per channel — so texel i is a deterministic mix.
  dc::ColorRamp blueRed = dc::ColorRamp::blueRed();
  std::vector<std::uint8_t> lut = dc::buildColorLut(blueRed);

  // Exact size: 256 texels * 4 bytes = 1024 bytes, tightly packed.
  check(lut.size() == static_cast<std::size_t>(dc::kColorLutWidth) * 4u,
        "lut: 256x1 RGBA8 == 1024 bytes (tight pack)");
  check(dc::kColorLutWidth == 256u, "lut: width is 256 (RESEARCH §4.2)");

  // The endpoints are EXACTLY the ramp's end colors (i=0 -> t=0, i=255 -> t=1).
  const dc::Rgba8 c0 = blueRed.sample(0.0);
  const dc::Rgba8 c255 = blueRed.sample(1.0);
  check(lut[0] == c0.r && lut[1] == c0.g && lut[2] == c0.b && lut[3] == c0.a,
        "lut: texel 0 == ramp.sample(0) (R,G,B,A byte order)");
  const std::size_t last = 255u * 4u;
  check(lut[last + 0] == c255.r && lut[last + 1] == c255.g &&
            lut[last + 2] == c255.b && lut[last + 3] == c255.a,
        "lut: texel 255 == ramp.sample(1)");

  // Every texel i is EXACTLY ramp.sample(i/255) — the byte-exact contract the
  // GPU samples. No off-by-one, no truncation drift.
  bool allExact = true;
  for (std::uint32_t i = 0; i < dc::kColorLutWidth; ++i) {
    const double t = static_cast<double>(i) / 255.0;
    const dc::Rgba8 c = blueRed.sample(t);
    const std::size_t o = static_cast<std::size_t>(i) * 4u;
    if (lut[o + 0] != c.r || lut[o + 1] != c.g || lut[o + 2] != c.b ||
        lut[o + 3] != c.a) {
      allExact = false;
      break;
    }
  }
  check(allExact, "lut: ALL 256 texels == ramp.sample(i/255) byte-exact");

  // A blue->red ramp is MONOTONE: red rises and blue falls left->right (smooth
  // per-pixel gradient — the whole point of the LUT path).
  bool redRises = true, blueFalls = true;
  for (std::uint32_t i = 1; i < dc::kColorLutWidth; ++i) {
    const std::size_t a = static_cast<std::size_t>(i - 1) * 4u;
    const std::size_t b = static_cast<std::size_t>(i) * 4u;
    if (lut[b + 0] < lut[a + 0]) redRises = false;   // R channel
    if (lut[b + 2] > lut[a + 2]) blueFalls = false;  // B channel
  }
  check(redRises, "lut: R channel monotonically RISES blue->red");
  check(blueFalls, "lut: B channel monotonically FALLS blue->red");

  // The two ends are DISTINCT (a real gradient, not a flat color).
  check(lut[0] != lut[last + 0] || lut[2] != lut[last + 2],
        "lut: endpoints distinct (a real gradient)");

  // Bake straight from a SequentialColorScale (viridis default): the overload is
  // the ramp overload, so its endpoints match the scale's ramp ends.
  dc::SequentialColorScale seq;  // viridis
  std::vector<std::uint8_t> seqLut = dc::buildColorLut(seq);
  const dc::Rgba8 sc0 = seq.ramp().sample(0.0);
  const dc::Rgba8 sc255 = seq.ramp().sample(1.0);
  check(seqLut.size() == 1024u, "seq-lut: 1024 bytes");
  check(seqLut[0] == sc0.r && seqLut[1] == sc0.g && seqLut[2] == sc0.b,
        "seq-lut: texel 0 == viridis t=0");
  check(seqLut[1020] == sc255.r && seqLut[1021] == sc255.g &&
            seqLut[1022] == sc255.b,
        "seq-lut: texel 255 == viridis t=1");
  // Viridis is dark-purple -> yellow; the two ends differ strongly.
  check(seqLut[0] != seqLut[1020] || seqLut[1] != seqLut[1021] ||
            seqLut[2] != seqLut[1022],
        "seq-lut: viridis endpoints distinct");

  std::printf("=== ENC-612 LUT bytes: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
