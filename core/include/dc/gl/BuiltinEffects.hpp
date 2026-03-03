#pragma once
// D78: Built-in post-process effects (vignette, bloom)
//
// Factory functions that register pre-configured passes into a PostProcessStack.

#include "dc/gl/PostProcessPass.hpp"
#include <string>

namespace dc {

// Fragment shader sources (for custom usage)
const char* vignetteFragmentSrc();
const char* bloomBrightPassFragSrc();
const char* bloomBlurFragSrc();
const char* bloomCompositeFragSrc();

// Convenience: add a vignette pass to a stack.
// strength: 0.0 (none) to 1.0 (heavy darkening at edges)
// radius: 0.0 (full-frame darkening) to 1.0+ (only corners)
void addVignettePass(PostProcessStack& stack,
                     float strength = 0.3f, float radius = 0.75f);

// Convenience: add a bloom pass chain (bright extract → blur → composite).
// threshold: luminance cutoff for bright areas (0.0 – 1.0)
// intensity: strength of the bloom glow (0.0 – 1.0+)
void addBloomPasses(PostProcessStack& stack,
                    float threshold = 0.7f, float intensity = 0.4f);

} // namespace dc
