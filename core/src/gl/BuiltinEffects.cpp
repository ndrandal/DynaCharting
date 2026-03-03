// D78: Built-in post-process effects
#include "dc/gl/BuiltinEffects.hpp"

namespace dc {

// ---- Vignette ----

static const char* kVignetteFragSrc = R"GLSL(
#version 330 core
uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_strength;
uniform float u_radius;
in vec2 v_uv;
out vec4 outColor;
void main() {
    vec4 color = texture(u_texture, v_uv);
    vec2 center = v_uv - 0.5;
    float dist = length(center) * 1.414; // normalize so corners = 1.0
    float vignette = 1.0 - u_strength * smoothstep(u_radius, 1.0, dist);
    outColor = vec4(color.rgb * vignette, color.a);
}
)GLSL";

const char* vignetteFragmentSrc() { return kVignetteFragSrc; }

void addVignettePass(PostProcessStack& stack, float strength, float radius) {
  stack.addPass("vignette", kVignetteFragSrc);
  stack.setPassUniform("vignette", "u_strength", strength);
  stack.setPassUniform("vignette", "u_radius", radius);
}

// ---- Bloom: bright-pass extraction ----

static const char* kBloomBrightPassFragSrc = R"GLSL(
#version 330 core
uniform sampler2D u_texture;
uniform float u_threshold;
in vec2 v_uv;
out vec4 outColor;
void main() {
    vec4 color = texture(u_texture, v_uv);
    float luminance = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    float brightness = max(luminance - u_threshold, 0.0);
    outColor = color * (brightness / max(luminance, 0.001));
}
)GLSL";

const char* bloomBrightPassFragSrc() { return kBloomBrightPassFragSrc; }

// ---- Bloom: Gaussian blur (separable, applied twice: H then V) ----

static const char* kBloomBlurFragSrc = R"GLSL(
#version 330 core
uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform float u_direction; // 0.0 = horizontal, 1.0 = vertical
in vec2 v_uv;
out vec4 outColor;
void main() {
    vec2 texelSize = 1.0 / u_resolution;
    vec2 dir = (u_direction < 0.5) ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    // 9-tap Gaussian kernel (sigma ~2.5)
    float weights[5] = float[](0.2270270, 0.1945946, 0.1216216, 0.0540540, 0.0162162);

    vec4 result = texture(u_texture, v_uv) * weights[0];
    for (int i = 1; i < 5; ++i) {
        result += texture(u_texture, v_uv + dir * float(i)) * weights[i];
        result += texture(u_texture, v_uv - dir * float(i)) * weights[i];
    }
    outColor = result;
}
)GLSL";

const char* bloomBlurFragSrc() { return kBloomBlurFragSrc; }

// ---- Bloom: composite (additive blend of bloom onto original) ----

static const char* kBloomCompositeFragSrc = R"GLSL(
#version 330 core
uniform sampler2D u_texture; // blurred bloom texture
uniform float u_intensity;
in vec2 v_uv;
out vec4 outColor;
void main() {
    vec4 bloom = texture(u_texture, v_uv);
    // The composite pass adds bloom on top — the original scene
    // is already in the framebuffer from a previous blit or
    // the bloom is blended additively.
    outColor = bloom * u_intensity;
}
)GLSL";

const char* bloomCompositeFragSrc() { return kBloomCompositeFragSrc; }

void addBloomPasses(PostProcessStack& stack, float threshold, float intensity) {
  stack.addPass("bloom_bright", kBloomBrightPassFragSrc);
  stack.setPassUniform("bloom_bright", "u_threshold", threshold);

  stack.addPass("bloom_blur_h", kBloomBlurFragSrc);
  stack.setPassUniform("bloom_blur_h", "u_direction", 0.0f);

  stack.addPass("bloom_blur_v", kBloomBlurFragSrc);
  stack.setPassUniform("bloom_blur_v", "u_direction", 1.0f);

  stack.addPass("bloom_composite", kBloomCompositeFragSrc);
  stack.setPassUniform("bloom_composite", "u_intensity", intensity);
}

} // namespace dc
