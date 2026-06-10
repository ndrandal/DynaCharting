// ENC-496 (P3.4) — DawnPostProcess implementation. See the header for the
// post-process chain design and the GL parallels.
#include "dc/gpu/DawnPostProcess.hpp"

#include "dc/gpu/DawnDevice.hpp"

#include <string>

namespace dc {

namespace {

// Identity mat3 (column-major) — the fullscreen pass needs no transform but the
// DawnDevice uniform block always carries the three mat3 columns at offset 0
// (binding 0 minBindingSize), so we pass identity.
const float kIdentityMat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

// The post-process uniform block is the standard DawnDevice 96-byte flat block.
// We use these named lanes (createBindGroup packs by name):
//   u_transform   -> c0/c1/c2  (floats 0..11)   — identity (unused by effects)
//   u_color       -> color     (floats 12..15)  — unused (kept 0); reserved
//   u_viewportSize-> resolution(floats 16,17)   — framebuffer resolution (px)
//   u_cornerRadius-> param0    (float 18)        — effect scalar 0
//   u_lineWidth   -> param1    (float 19)        — effect scalar 1
// The WGSL Uniforms struct mirrors this byte layout exactly (vec4 x4 + vec2 + 2
// f32 = 80 bytes; the block is allocated at 96 so the tail is just padding).
constexpr std::size_t kPostUniformBytes = 96;

// Shared WGSL header: the fullscreen-triangle vertex shader (3 verts covering
// [-1,1]^2 from @builtin(vertex_index) bit math — the WGSL port of the GL
// gl_VertexID trick in PostProcessPass's kPostProcessVert), the uniform struct,
// and the sampled input texture + sampler (ENC-491 binding 1/2). The per-effect
// fragment body is appended after this, wrapped in fs_main.
const char* kPostProcessHeader = R"WGSL(
struct Uniforms {
  c0         : vec4<f32>,   // transform column 0 (xyz) — identity, unused
  c1         : vec4<f32>,
  c2         : vec4<f32>,
  color      : vec4<f32>,   // reserved, unused by builtin effects
  resolution : vec2<f32>,   // framebuffer size in pixels (u_resolution)
  param0     : f32,         // effect scalar 0 (u_cornerRadius lane)
  param1     : f32,         // effect scalar 1 (u_lineWidth lane)
};
@group(0) @binding(0) var<uniform> u : Uniforms;
@group(0) @binding(1) var u_texture : texture_2d<f32>;
@group(0) @binding(2) var u_sampler : sampler;

struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0)       uv  : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32) -> VsOut {
  // Fullscreen triangle: 3 vertices cover the [-1,1] x [-1,1] clip rect, exactly
  // the GL kPostProcessVert math (x = -1 + ((vid & 1) << 2), y = -1 + ((vid & 2) << 1)).
  let x = -1.0 + f32((vid & 1u) << 2u);
  let y = -1.0 + f32((vid & 2u) << 1u);
  var o : VsOut;
  o.pos = vec4<f32>(x, y, 0.0, 1.0);
  // uv = clip*0.5 + 0.5 (GL v_uv). We render straight into the output target's
  // top-left framebuffer and sample the input the same way, so input and output
  // orientations are consistent across the whole chain (no extra y-flip needed).
  o.uv = vec2<f32>(x * 0.5 + 0.5, y * 0.5 + 0.5);
  return o;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {
  let texel = textureSample(u_texture, u_sampler, uv);
  var color : vec4<f32> = texel;
)WGSL";

const char* kPostProcessFooter = R"WGSL(
  return color;
}
)WGSL";

std::string buildEffectWgsl(const std::string& fragmentBody) {
  return std::string(kPostProcessHeader) + fragmentBody + kPostProcessFooter;
}

}  // namespace

// --- DawnPostProcessPass ----------------------------------------------------

bool DawnPostProcessPass::init(DawnDevice& device, const std::string& effectName,
                               const std::string& fragmentBody) {
  name_ = effectName;
  // Keep the WGSL source alive for the duration of createPipeline (PipelineDesc
  // borrows the const char*). It is compiled into a wgpu::ShaderModule there, so
  // it need not outlive this function.
  const std::string wgsl = buildEffectWgsl(fragmentBody);

  PipelineDesc desc;
  desc.debugName = name_.c_str();
  desc.vertexSource = wgsl.c_str();
  desc.fragmentSource = nullptr;
  // No vertex buffers: the 3 fullscreen-triangle positions come from
  // @builtin(vertex_index). DawnDevice::draw() issues a buffer-less Draw when the
  // bind group carries no vertex buffer.
  desc.vertexBuffers = nullptr;
  desc.vertexBufferCount = 0;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  desc.uniformBytes = kPostUniformBytes;
  desc.sampledTexture = true;  // binding 1 = input texture, binding 2 = sampler

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

void DawnPostProcessPass::apply(DawnDevice& device, TextureHandle inputTexture,
                                std::uint32_t outputTargetId, std::uint32_t w,
                                std::uint32_t h) {
  if (!pipeline_.valid() || !inputTexture.valid()) return;

  const float resolution[2] = {static_cast<float>(w), static_cast<float>(h)};

  UniformBinding uniforms[5];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = kIdentityMat3;
  uniforms[1].kind = UniformBinding::Kind::Vec2;
  uniforms[1].name = "u_viewportSize";  // -> resolution lane (floats 16,17)
  uniforms[1].data = resolution;
  uniforms[2].kind = UniformBinding::Kind::Float;
  uniforms[2].name = "u_cornerRadius";  // -> param0 lane (float 18)
  uniforms[2].data = &param0_;
  uniforms[3].kind = UniformBinding::Kind::Float;
  uniforms[3].name = "u_lineWidth";  // -> param1 lane (float 19)
  uniforms[3].data = &param1_;
  uniforms[4].kind = UniformBinding::Kind::Sampler2D;
  uniforms[4].name = "u_texture";
  uniforms[4].texture = inputTexture;
  uniforms[4].textureUnit = 0;

  BindGroupDesc bg;
  bg.pipeline = pipeline_;
  bg.vertexBuffers = nullptr;  // fullscreen triangle: no vertex buffer
  bg.vertexBufferCount = 0;
  bg.indexBuffer = {};
  bg.uniforms = uniforms;
  bg.uniformCount = 5;

  BindGroupHandle group = device.createBindGroup(bg);
  if (!group.valid()) return;

  // Render the fullscreen triangle into the output target. clear=false is fine
  // (the triangle covers every pixel), but we clear to be explicit + match GL's
  // ensureFbo writing the whole output. clearStencil=true since the target also
  // carries a Stencil8 attachment (None clip mode ignores it).
  RenderPassDesc rp;
  rp.target.id = outputTargetId;
  rp.viewportWidth = w;
  rp.viewportHeight = h;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  rp.clearStencil = true;
  device.beginRenderPass(rp);

  device.bindPipeline(pipeline_);

  DrawParams dp;
  dp.vertexCount = 3;  // fullscreen triangle
  dp.indexCount = 0;
  dp.firstVertex = 0;
  device.draw(group, dp);

  device.endRenderPass();

  device.destroyBindGroup(group);
}

// --- DawnPostProcessStack ---------------------------------------------------

void DawnPostProcessStack::init(std::uint32_t sceneTargetId,
                                std::uint32_t firstPassTargetId) {
  sceneTargetId_ = sceneTargetId;
  firstPassTargetId_ = firstPassTargetId;
  passes_.clear();
}

bool DawnPostProcessStack::addPass(DawnDevice& device,
                                   const std::string& effectName,
                                   const std::string& fragmentBody, float param0,
                                   float param1) {
  DawnPostProcessPass pass;
  if (!pass.init(device, effectName, fragmentBody)) return false;
  pass.setParam0(param0);
  pass.setParam1(param1);
  passes_.push_back(std::move(pass));
  return true;
}

std::uint32_t DawnPostProcessStack::apply(DawnDevice& device, std::uint32_t w,
                                          std::uint32_t h) {
  // The scene has already been rendered into sceneTargetId_. Chain the passes:
  // pass 0 samples the scene target, pass 1 samples pass 0's output, ... Each
  // pass renders into a distinct intermediate target id (firstPassTargetId_ + i).
  std::uint32_t inputId = sceneTargetId_;
  std::uint32_t lastOutput = sceneTargetId_;
  for (std::size_t i = 0; i < passes_.size(); ++i) {
    const std::uint32_t outputId =
        firstPassTargetId_ + static_cast<std::uint32_t>(i);
    // Wrap the input target's color as a sampleable texture for this pass.
    TextureHandle inputTex = device.textureForRenderTarget(inputId);
    passes_[i].apply(device, inputTex, outputId, w, h);
    lastOutput = outputId;
    inputId = outputId;  // feed this output into the next pass
  }
  return lastOutput;
}

// --- Builtin effect WGSL fragment bodies ------------------------------------
// Each is the BODY of fs_main: `uv`, `texel`, `u.resolution`, `u.param0`,
// `u.param1` are in scope; assign `color`. Ported from core/src/gl/
// BuiltinEffects.cpp (and the d47_2 blur) to match the GL math.

const char* vignetteEffectWgsl() {
  // Port of kVignetteFragSrc:
  //   center = uv - 0.5; dist = length(center) * 1.414 (corners ~= 1.0)
  //   vignette = 1 - strength * smoothstep(radius, 1.0, dist)
  //   color = texel.rgb * vignette (alpha preserved)
  // param0 = strength, param1 = radius.
  return R"WGSL(
  let strength = u.param0;
  let radius   = u.param1;
  let centerv  = uv - vec2<f32>(0.5, 0.5);
  let dist     = length(centerv) * 1.414;
  let vig      = 1.0 - strength * smoothstep(radius, 1.0, dist);
  color = vec4<f32>(texel.rgb * vig, texel.a);
)WGSL";
}

const char* blurEffectWgsl() {
  // Port of kBloomBlurFragSrc (the separable 9-tap Gaussian, sigma ~2.5), which
  // is also a superset of the d47_2 5-tap blur. param0 = direction (0 = H, 1 = V).
  // texelSize = 1/resolution; the 9 taps sample +/- i*dir with the GL weights.
  return R"WGSL(
  let texelSize = vec2<f32>(1.0, 1.0) / u.resolution;
  var dir : vec2<f32>;
  if (u.param0 < 0.5) { dir = vec2<f32>(texelSize.x, 0.0); }
  else                { dir = vec2<f32>(0.0, texelSize.y); }
  let w0 = 0.2270270;
  let w1 = 0.1945946;
  let w2 = 0.1216216;
  let w3 = 0.0540540;
  let w4 = 0.0162162;
  var result = textureSample(u_texture, u_sampler, uv) * w0;
  result = result + textureSample(u_texture, u_sampler, uv + dir * 1.0) * w1;
  result = result + textureSample(u_texture, u_sampler, uv - dir * 1.0) * w1;
  result = result + textureSample(u_texture, u_sampler, uv + dir * 2.0) * w2;
  result = result + textureSample(u_texture, u_sampler, uv - dir * 2.0) * w2;
  result = result + textureSample(u_texture, u_sampler, uv + dir * 3.0) * w3;
  result = result + textureSample(u_texture, u_sampler, uv - dir * 3.0) * w3;
  result = result + textureSample(u_texture, u_sampler, uv + dir * 4.0) * w4;
  result = result + textureSample(u_texture, u_sampler, uv - dir * 4.0) * w4;
  color = result;
)WGSL";
}

const char* bloomBrightEffectWgsl() {
  // Port of kBloomBrightPassFragSrc: keep only the bright part of the input.
  //   luminance = dot(rgb, vec3(0.2126,0.7152,0.0722))
  //   brightness = max(luminance - threshold, 0)
  //   color = texel * (brightness / max(luminance, 0.001))
  // param0 = threshold.
  return R"WGSL(
  let threshold  = u.param0;
  let luminance  = dot(texel.rgb, vec3<f32>(0.2126, 0.7152, 0.0722));
  let brightness = max(luminance - threshold, 0.0);
  color = texel * (brightness / max(luminance, 0.001));
)WGSL";
}

}  // namespace dc
