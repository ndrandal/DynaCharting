// ENC-612 (P2) — 256×1 color-ramp LUT implementation. See dc/scale/ColorLut.hpp.
#include "dc/scale/ColorLut.hpp"

namespace dc {

std::vector<std::uint8_t> buildColorLut(const ColorRamp& ramp) {
  std::vector<std::uint8_t> lut(static_cast<std::size_t>(kColorLutWidth) * 4u);
  for (std::uint32_t i = 0; i < kColorLutWidth; ++i) {
    // t spans the full [0,1] with the endpoints inclusive (i=0 -> t=0,
    // i=255 -> t=1), so texel 0/255 are exactly the ramp's end colors.
    const double t = static_cast<double>(i) /
                     static_cast<double>(kColorLutWidth - 1);
    const Rgba8 c = ramp.sample(t);
    const std::size_t o = static_cast<std::size_t>(i) * 4u;
    lut[o + 0] = c.r;
    lut[o + 1] = c.g;
    lut[o + 2] = c.b;
    lut[o + 3] = c.a;
  }
  return lut;
}

std::vector<std::uint8_t> buildColorLut(const SequentialColorScale& scale) {
  return buildColorLut(scale.ramp());
}

std::vector<std::uint8_t> buildColorLut(const DivergingColorScale& scale) {
  return buildColorLut(scale.ramp());
}

}  // namespace dc
