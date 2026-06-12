// ENC-612 (P2) — 256×1 COLOR-RAMP LUT TEXTURE (RESEARCH §4.2 last paragraph).
//
// WHAT THIS IS
// ------------
// "Per-pixel color (KDE/spectrogram) uses a 256×1 LUT texture sampled in-shader,
// not per-row." (RESEARCH §4.2). This helper bakes a ColorRamp (or the ramp of a
// SequentialColorScale / DivergingColorScale) into a 256×1 row of RGBA8 texels —
// the exact byte layout the existing texturedQuad@1 pipeline uploads as a texture
// and samples continuously in-shader. A texturedQuad spanning a field, whose
// horizontal UV runs 0..1, then samples a SMOOTH per-pixel gradient straight from
// the ramp (continuous color for smooth fields), with ZERO per-row work.
//
// The texel byte order is R,G,B,A (little-endian, matching Rgba8::toU32 and the
// WebGPU RGBA8Unorm memory layout), tightly packed (256*4 = 1024 bytes), so the
// bytes feed `DawnDevice::createTexture` / a `TextureSource` verbatim. Texel i
// (i in 0..255) is the ramp sampled at t = i/255, so texel 0 is the ramp's t=0
// color and texel 255 its t=1 color — a continuous left→right gradient.
#pragma once

#include "dc/scale/ColorScale.hpp"

#include <cstdint>
#include <vector>

namespace dc {

// The fixed LUT width (RESEARCH §4.2 names 256). One texel per column, 1 row.
constexpr std::uint32_t kColorLutWidth = 256;

// Build a 256×1 RGBA8 LUT (1024 bytes, R,G,B,A per texel, row-major) from a ramp.
// Texel i = ramp.sample(i / 255.0). The result is ready to upload as an RGBA8
// texture of width kColorLutWidth, height 1 (the texturedQuad LUT path).
std::vector<std::uint8_t> buildColorLut(const ColorRamp& ramp);

// Convenience overloads: bake straight from a color scale's ramp.
std::vector<std::uint8_t> buildColorLut(const SequentialColorScale& scale);
std::vector<std::uint8_t> buildColorLut(const DivergingColorScale& scale);

}  // namespace dc
