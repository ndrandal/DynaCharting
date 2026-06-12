// ENC-491 (P2.8) — DawnTexturedQuadBackend implementation. See header.
//
// Dawn mirror of Renderer::drawTexturedQuad (kTexQuadVert/kTexQuadFrag). Owns the
// texturedQuad render pipeline, uploads the per-instance Pos2Uv4 rect records
// into a Dawn instance-step vertex buffer (CPU-gathering the visible subset for
// the D26 indexed path), lazily creates a Dawn texture from the TextureSource
// pixels, builds the per-draw bind group (mat3 transform + vec4 color + the
// texture+sampler), and issues the instanced 6-verts-per-quad draw through
// GpuDevice::drawInstanced.
#include "dc/gpu/DawnTexturedQuadBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"

#include <cstring>
#include <vector>

namespace dc {

namespace {

const float kIdentityMat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

const float* resolveTransform(const DrawItem& di, const Scene& scene) {
  if (!di.transformId) return kIdentityMat3;
  const Transform* t = scene.getTransform(di.transformId);
  return t ? t->mat3 : kIdentityMat3;
}

// Pos2Uv4 vertex stride: vec4(x0,y0,x1,y1) = 16 bytes (strideOf(Pos2Uv4)).
constexpr std::uint32_t kQuadStride = 16;

// WGSL port of the GL texturedQuad shader (kTexQuadVert/kTexQuadFrag). One
// module, two entry points.
//
//   * The unit quad's 6 vertices are generated from @builtin(vertex_index) % 6
//     (matching the GL gl_VertexID % 6 quad); there is NO per-vertex buffer.
//   * a_pos_uv (vec4 x0,y0,x1,y1) is the single per-instance attribute at
//     location 0 (VertexStepMode::Instance). The quad corner is mix()'d between
//     the rect min/max by the unit uv, transformed by the mat3, then y-flipped to
//     NDC. That SAME uv is passed through as the texture coordinate (GL's v_uv).
//   * The fragment samples the texture at v_uv and modulates by u_color:
//       outColor = texel * u_color   (verbatim from kTexQuadFrag).
//   * Uniform packing matches DawnDevice::createBindGroup's 64-byte base block
//     (three mat3 columns padded to vec4 + a color vec4). The texture (binding 1)
//     and sampler (binding 2) are added by the sampledTexture pipeline layout.
const char* kTexQuadWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,   // transform column 0 (xyz)
  c1    : vec4<f32>,   // transform column 1 (xyz)
  c2    : vec4<f32>,   // transform column 2 (xyz)
  color : vec4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;
@group(0) @binding(1) var u_texture : texture_2d<f32>;
@group(0) @binding(2) var u_sampler : sampler;

struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0)       uv  : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_pos_uv : vec4<f32>) -> VsOut {
  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, 0.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, 0.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, 0.0); }
  else              { uv = vec2<f32>(1.0, 1.0); }

  let x = mix(a_pos_uv.x, a_pos_uv.z, uv.x);
  let y = mix(a_pos_uv.y, a_pos_uv.w, uv.y);

  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(x, y, 1.0);

  var out : VsOut;
  // Negate y (same convention as triSolid/instancedRect) so the WebGPU top-left
  // framebuffer matches the GL bottom-left readback orientation.
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.uv = uv;
  return out;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {
  let texel = textureSample(u_texture, u_sampler, uv);
  return texel * u.color;
}
)WGSL";

}  // namespace

bool DawnTexturedQuadBackend::init(GpuDevice& device) {
  // One per-instance attribute: a_pos_uv = Float32x4 @ location 0, stride 16B,
  // VertexStepMode::Instance (GL divisor 1). The unit quad's vertices come from
  // @builtin(vertex_index), so there is no per-vertex buffer.
  VertexAttribute attr;
  attr.location = 0;
  attr.componentCount = 4;
  attr.type = VertexComponentType::Float32;
  attr.offsetBytes = 0;

  VertexBufferLayout layout;
  layout.strideBytes = kQuadStride;  // 16B, 4-byte aligned (ENC-485)
  layout.stepInstance = true;        // per-instance step mode
  layout.attributes = &attr;
  layout.attributeCount = 1;

  PipelineDesc desc;
  desc.debugName = "texturedQuad@1";
  desc.vertexSource = kTexQuadWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 64-byte base uniform block: mat3 (3*vec4) + color vec4. No viewport/radius.
  desc.uniformBytes = 64;
  // ENC-491: declare the texture+sampler binding so DawnDevice builds the group-0
  // layout with binding 1 (texture) + binding 2 (sampler).
  desc.sampledTexture = true;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

DawnTexturedQuadBackend::GeoBuffers&
DawnTexturedQuadBackend::ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                                          CpuBufferStore& gpu,
                                          std::uint32_t geometryId) {
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) return kv.second;
  }

  GeoBuffers gb;
  const Geometry* geo = scene.getGeometry(geometryId);
  if (geo) {
    const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
    const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);

    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      // D26 indexed gather: pack only the selected instances into a scratch
      // per-instance buffer (mirrors the GL scratch-VBO gather). The index
      // buffer holds u32 instance indices into the Pos2Uv4 vertex buffer.
      const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
      if (vtx && idx && vtxBytes > 0) {
        const std::uint32_t count = geo->indexCount;
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
        std::vector<std::uint8_t> scratch(
            static_cast<std::size_t>(count) * kQuadStride, 0);
        for (std::uint32_t i = 0; i < count; ++i) {
          const std::uint32_t off = indices[i] * kQuadStride;
          if (off + kQuadStride <= vtxBytes) {
            std::memcpy(scratch.data() + static_cast<std::size_t>(i) * kQuadStride,
                        vtx + off, kQuadStride);
          }
        }
        if (!scratch.empty()) {
          gb.instanceBuffer = device.createBuffer(
              scratch.size(), scratch.data(), scratch.size());
          gb.instanceCount = count;
        }
      }
    } else if (vtx && vtxBytes > 0) {
      // Non-indexed: upload the Pos2Uv4 records directly; one instance/vertex.
      gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
      gb.instanceCount = geo->vertexCount;
    }
  }

  geoBuffers_.emplace_back(geometryId, gb);
  return geoBuffers_.back().second;
}

TextureHandle DawnTexturedQuadBackend::ensureTexture(GpuDevice& device,
                                                     std::uint32_t textureId) {
  CachedTexture* cached = nullptr;
  for (auto& kv : textureHandles_) {
    if (kv.first == textureId) { cached = &kv.second; break; }
  }

  // ENC-568: re-upload when the source pixels changed (animated texture track) —
  // the texture analogue of ENC-558's instanced-buffer re-read. A cache hit
  // (same version) skips straight to the existing handle.
  const std::uint64_t srcVer = textures_ ? textures_->getTextureVersion(textureId) : 0;
  if (cached && cached->handle.valid() && cached->version == srcVer) {
    return cached->handle;
  }

  const std::uint8_t* data = nullptr;
  std::uint32_t w = 0, ht = 0;
  TextureFormat fmt = TextureFormat::RGBA8;
  if (!textures_ ||
      !textures_->getTexturePixels(textureId, &data, &w, &ht, &fmt) || !data) {
    // No pixels available; keep any prior handle (don't churn) or cache empty.
    if (cached) return cached->handle;
    textureHandles_.emplace_back(textureId, CachedTexture{});
    return {};
  }

  if (!cached) {
    textureHandles_.emplace_back(textureId, CachedTexture{});
    cached = &textureHandles_.back().second;
  }

  // Same dimensions/format → cheap in-place re-upload (queueWriteTexture).
  // Otherwise (first use, or the texture was resized) (re)create it.
  if (cached->handle.valid() && cached->w == w && cached->h == ht && cached->format == fmt) {
    device.updateTexture(cached->handle, data);
  } else {
    if (cached->handle.valid()) device.destroyTexture(cached->handle);
    TextureDesc td;
    td.width = w;
    td.height = ht;
    td.format = fmt;
    td.filter = TextureFilter::Linear;  // matches GL TextureManager (GL_LINEAR)
    td.data = data;                     // initial upload via queue.WriteTexture
    cached->handle = device.createTexture(td);
    cached->w = w;
    cached->h = ht;
    cached->format = fmt;
  }
  cached->version = srcVer;
  return cached->handle;
}

BackendStats DawnTexturedQuadBackend::renderDrawItem(GpuDevice& device,
                                                     const Scene& scene,
                                                     CpuBufferStore& gpu,
                                                     const DrawItem& di,
                                                     int /*viewW*/,
                                                     int /*viewH*/) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;
  if (di.textureId == 0) return stats;  // texturedQuad needs a texture (GL parity)

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.instanceBuffer.valid() || gb.instanceCount == 0) return stats;

  TextureHandle tex = ensureTexture(device, di.textureId);
  if (!tex.valid()) return stats;

  // Per-draw uniforms: mat3 transform + vec4 color (the color modulation) + the
  // Sampler2D texture binding (mirrors GL u_transform / u_color / u_texture).
  const float* xform = resolveTransform(di, scene);

  UniformBinding uniforms[3];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_color";
  uniforms[1].data = di.color;
  uniforms[2].kind = UniformBinding::Kind::Sampler2D;
  uniforms[2].name = "u_texture";
  uniforms[2].texture = tex;
  uniforms[2].textureUnit = 0;

  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.instanceBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = {};  // instanced draw: no GPU index buffer (gather is CPU)
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 3;

  BindGroupHandle group = device.createBindGroup(bgDesc);
  if (!group.valid()) return stats;

  device.bindPipeline(pipeline_);

  DrawInstancedParams params;
  params.vertexCountPerInstance = 6;  // unit quad (two triangles)
  params.instanceCount = gb.instanceCount;
  params.firstVertex = 0;

  DeviceDrawStats ds = device.drawInstanced(group, params);

  stats.drawCalls = ds.drawCalls;
  stats.verticesSubmitted = ds.verticesSubmitted;
  return stats;
}

}  // namespace dc
