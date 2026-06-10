// ENC-496 (P3.4) — Dawn/WebGPU post-process passes (D47 / D78.2).
//
// The Dawn mirror of core/src/gl/PostProcessPass.{hpp,cpp} (the GL
// PostProcessPass + PostProcessStack) and core/src/gl/BuiltinEffects.cpp (the
// vignette / blur / bloom effect shaders). It renders a scene into an offscreen
// texture, then chains fullscreen effect passes — each one samples the PREVIOUS
// pass's output texture and applies its effect, the output of pass N feeding the
// input of pass N+1.
//
// STRUCTURE (parallels the GL PostProcessPass/PostProcessStack):
//   * DawnPostProcessPass — ONE fullscreen-triangle pipeline (no vertex buffer;
//     the 3 covering positions come from @builtin(vertex_index) bit math, exactly
//     like GL's gl_VertexID trick) whose fragment samples an input texture and
//     applies the effect. apply() renders it into a chosen render target.
//   * DawnPostProcessStack — owns the scene target id + an ordered list of
//     passes. apply() renders pass 0 sampling the scene target, pass 1 sampling
//     pass 0's output, ... ending in the final target the caller reads back.
//
// RENDER-TO-TEXTURE (built on ENC-495): each pass renders into a DawnDevice
// render target (RenderTargetHandle id) whose color texture carries
// TextureBinding usage, so the NEXT pass can sample it. The stack assigns target
// ids >= 2 to its scene + intermediate targets (ids 0/1 are reserved for the
// main + pick targets). A pass samples its input via
// DawnDevice::textureForRenderTarget(inputId), which wraps the input target's
// color view + a sampler as a sampledTexture bind group (ENC-491 path).
//
// EFFECT PARAMETERS: a pass carries up to two scalar params + the framebuffer
// resolution, packed into the standard DawnDevice uniform block by name
// (u_viewportSize = resolution, u_param0 / u_param1 = the two effect scalars).
// The WGSL fragment reads them from the matching uniform struct fields. The
// per-effect WGSL (vignette / blur / bloom) lives in DawnPostProcess.cpp and
// matches the GL BuiltinEffects math.
#pragma once

#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

class DawnDevice;

/// One fullscreen post-process pass: a sampledTexture pipeline that samples an
/// input texture and applies an effect, rendering into an output render target.
class DawnPostProcessPass {
 public:
  /// Build the pass from an effect WGSL fragment-shader BODY (the lines inside
  /// fs_main that compute `color` from `texel`/`uv`; see the builtin effects).
  /// The shared fullscreen-triangle vertex shader + uniform/texture bindings are
  /// prepended automatically. Returns false if the pipeline failed to create.
  bool init(DawnDevice& device, const std::string& effectName,
            const std::string& fragmentBody);

  /// Set one of the two scalar effect parameters (param0 / param1). Their meaning
  /// is effect-specific (vignette: strength/radius; blur: direction/_).
  void setParam0(float v) { param0_ = v; }
  void setParam1(float v) { param1_ = v; }

  /// Render the effect: sample `inputTexture` and write the result into render
  /// target `outputTargetId` at (w,h). The caller supplies `inputTexture` (the
  /// previous pass's output, obtained from DawnDevice::textureForRenderTarget).
  void apply(DawnDevice& device, TextureHandle inputTexture,
             std::uint32_t outputTargetId, std::uint32_t w, std::uint32_t h);

  const std::string& name() const { return name_; }
  bool valid() const { return pipeline_.valid(); }

 private:
  std::string name_;
  PipelineHandle pipeline_{};
  float param0_{0.0f};
  float param1_{0.0f};
};

/// Chains post-process passes: render the scene into the scene target, then run
/// each pass sampling the previous output, ending in the final output target.
class DawnPostProcessStack {
 public:
  /// `sceneTargetId` is the render target the scene is drawn into (and the input
  /// to the first pass). Passes render into successive target ids starting at
  /// `firstPassTargetId`. Both default to the post-process reserved range
  /// (>= 2) so they never collide with the main (0) / pick (1) targets.
  void init(std::uint32_t sceneTargetId = 2, std::uint32_t firstPassTargetId = 3);

  /// Append a pass built from an effect WGSL fragment body (see the builtin
  /// effect helpers below). Returns false if the pass failed to build.
  bool addPass(DawnDevice& device, const std::string& effectName,
               const std::string& fragmentBody, float param0 = 0.0f,
               float param1 = 0.0f);

  /// Run the chain at (w,h). The scene must already have been rendered into the
  /// scene target (sceneTargetId). Returns the render target id holding the final
  /// result (the caller reads it back via beginRenderPass(load)+readPixel, or it
  /// is the scene target id if there are no passes).
  std::uint32_t apply(DawnDevice& device, std::uint32_t w, std::uint32_t h);

  std::uint32_t sceneTargetId() const { return sceneTargetId_; }
  std::size_t passCount() const { return passes_.size(); }

 private:
  std::uint32_t sceneTargetId_{2};
  std::uint32_t firstPassTargetId_{3};
  std::vector<DawnPostProcessPass> passes_;
};

// --- Builtin effect WGSL fragment bodies (ports of GL BuiltinEffects) -------
// Each returns the WGSL BODY of fs_main: it has `uv` (vec2, the sampled UV),
// `texel` (vec4, the input texture sampled at uv), `u.resolution` (vec2),
// `u.param0` / `u.param1` (f32) in scope, and must assign `var color : vec4<f32>`.
// The matching scalar params are set by the stack's addPass(param0,param1).

/// Vignette (port of kVignetteFragSrc): darkens toward the corners.
/// param0 = strength, param1 = radius. color = texel.rgb * vignette.
const char* vignetteEffectWgsl();

/// Separable Gaussian blur (port of kBloomBlurFragSrc / the d47_2 blur):
/// 9-tap, param0 = direction (0 = horizontal, 1 = vertical).
const char* blurEffectWgsl();

/// Bloom bright-pass extraction (port of kBloomBrightPassFragSrc):
/// param0 = luminance threshold; keeps only the bright part of the input.
const char* bloomBrightEffectWgsl();

}  // namespace dc
