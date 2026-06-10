// ENC-492 (P2.9) — DawnTextSdfBackend implementation. See header.
//
// Dawn mirror of Renderer::drawTextSdf (kTextSdfVert/kTextSdfFrag). Owns the
// textSDF render pipeline, uploads the GlyphAtlas R8 SDF bitmap into a Dawn R8
// texture (re-uploading on atlas-dirty, mirroring uploadAtlasIfDirty), uploads
// the per-instance Glyph8 records (quad pos/size + atlas UV rect) into a Dawn
// instance-step vertex buffer (CPU-gathering the visible subset for the D26
// indexed path), builds the per-draw bind group (mat3 transform + vec4 color +
// the atlas texture+sampler), and issues the instanced 6-verts-per-quad draw
// through GpuDevice::drawInstanced. The fragment reconstructs the SDF alpha
// (smoothstep around 0.5) so the text edges are crisp + anti-aliased, composited
// by the default Normal alpha blend.
#include "dc/gpu/DawnTextSdfBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"
#include "dc/text/GlyphAtlas.hpp"

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

// Glyph8 vertex stride: vec4(x0,y0,x1,y1) + vec4(u0,v0,u1,v1) = 32 bytes
// (strideOf(Glyph8)).
constexpr std::uint32_t kGlyphStride = 32;

// WGSL port of the GL textSDF shader (kTextSdfVert/kTextSdfFrag). One module,
// two entry points.
//
//   * The unit quad's 6 vertices are generated from @builtin(vertex_index) % 6
//     (matching the GL gl_VertexID % 6 quad); there is NO per-vertex buffer.
//   * a_g0 (vec4 x0,y0,x1,y1, the quad pos/size) is the per-instance attribute at
//     location 0; a_g1 (vec4 u0,v0,u1,v1, the atlas UV rect) at location 1. Both
//     VertexStepMode::Instance. The quad corner is mix()'d between the rect
//     min/max by the unit uv; the atlas UV is mix()'d the same way -> v_uv (GL
//     parity). The position is transformed by the mat3 then y-flipped to NDC.
//   * The fragment samples the R8 atlas at v_uv, reconstructs the SDF coverage
//     alpha = smoothstep(0.45, 0.55, dist) (verbatim from the GL useSdf branch),
//     and outputs vec4(u_color.rgb, u_color.a * alpha). The default Normal alpha
//     blend (DawnDevice) then composites the crisp, anti-aliased SDF edges.
//   * Uniform packing matches DawnDevice::createBindGroup's 64-byte base block
//     (three mat3 columns padded to vec4 + a color vec4). The atlas texture
//     (binding 1) and sampler (binding 2) are added by the sampledTexture
//     pipeline layout.
const char* kTextSdfWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,   // transform column 0 (xyz)
  c1    : vec4<f32>,   // transform column 1 (xyz)
  c2    : vec4<f32>,   // transform column 2 (xyz)
  color : vec4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;
@group(0) @binding(1) var u_atlas : texture_2d<f32>;
@group(0) @binding(2) var u_sampler : sampler;

struct VsOut {
  @builtin(position) pos : vec4<f32>,
  @location(0)       uv  : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_g0 : vec4<f32>,
           @location(1) a_g1 : vec4<f32>) -> VsOut {
  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, 0.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, 0.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, 0.0); }
  else              { uv = vec2<f32>(1.0, 1.0); }

  // Quad corner = mix of the rect (x0,y0)-(x1,y1) by the unit uv.
  let x = mix(a_g0.x, a_g0.z, uv.x);
  let y = mix(a_g0.y, a_g0.w, uv.y);

  // Atlas UV = mix of the atlas rect (u0,v0)-(u1,v1) by the same unit uv (v_uv).
  let auv = vec2<f32>(mix(a_g1.x, a_g1.z, uv.x), mix(a_g1.y, a_g1.w, uv.y));

  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(x, y, 1.0);

  var out : VsOut;
  // Negate y (same convention as triSolid/instancedRect/texturedQuad) so the
  // WebGPU top-left framebuffer matches the GL bottom-left readback orientation.
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.uv = auv;
  return out;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>) -> @location(0) vec4<f32> {
  let val = textureSample(u_atlas, u_sampler, uv).r;
  // SDF reconstruction: crisp, anti-aliased coverage around the 0.5 isoline
  // (matches the GL kTextSdfFrag smoothstep(0.45, 0.55, val) SDF branch).
  let a = smoothstep(0.45, 0.55, val);
  return vec4<f32>(u.color.rgb, u.color.a * a);
}
)WGSL";

}  // namespace

bool DawnTextSdfBackend::init(GpuDevice& device) {
  // Two per-instance attributes from the Glyph8 record (stride 32B,
  // VertexStepMode::Instance, GL divisor 1):
  //   a_g0 = Float32x4 @ location 0, offset 0  (quad pos x0,y0,x1,y1)
  //   a_g1 = Float32x4 @ location 1, offset 16 (atlas UV u0,v0,u1,v1)
  // The unit quad's vertices come from @builtin(vertex_index), so there is no
  // per-vertex buffer.
  VertexAttribute attrs[2];
  attrs[0].location = 0;
  attrs[0].componentCount = 4;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 4;
  attrs[1].type = VertexComponentType::Float32;
  attrs[1].offsetBytes = 16;

  VertexBufferLayout layout;
  layout.strideBytes = kGlyphStride;  // 32B, 4-byte aligned (ENC-485)
  layout.stepInstance = true;         // per-instance step mode
  layout.attributes = attrs;
  layout.attributeCount = 2;

  PipelineDesc desc;
  desc.debugName = "textSDF@1";
  desc.vertexSource = kTextSdfWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  // Default Normal alpha blend so the SDF coverage edges composite over the
  // background (essential for text — partial-coverage edge pixels).
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 64-byte base uniform block: mat3 (3*vec4) + color vec4.
  desc.uniformBytes = 64;
  // ENC-491: declare the texture+sampler binding so DawnDevice builds the group-0
  // layout with binding 1 (atlas texture) + binding 2 (sampler).
  desc.sampledTexture = true;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

TextureHandle DawnTextSdfBackend::uploadAtlasIfDirty(GpuDevice& device) {
  if (!atlas_) return TextureHandle{};

  // Mirror Renderer::uploadAtlasIfDirty: create the R8 atlas texture on first
  // upload (now that the atlas size is known), then re-upload pixels whenever the
  // atlas is dirty (grew/changed).
  if (!atlasTex_.valid()) {
    const std::uint32_t sz = atlas_->atlasSize();
    TextureDesc desc;
    desc.width = sz;
    desc.height = sz;
    desc.format = TextureFormat::R8;       // single-channel SDF coverage
    desc.filter = TextureFilter::Linear;   // matches GL (GL_LINEAR)
    desc.data = atlas_->atlasData();       // initial upload via queue.WriteTexture
    atlasTex_ = device.createTexture(desc);
    atlas_->clearDirty();
  } else if (atlas_->isDirty()) {
    device.updateTexture(atlasTex_, atlas_->atlasData());
    atlas_->clearDirty();
  }
  return atlasTex_;
}

DawnTextSdfBackend::GeoBuffers&
DawnTextSdfBackend::ensureGeoBuffers(GpuDevice& device, const Scene& scene,
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
      // D26 indexed gather: pack only the selected glyph instances into a scratch
      // per-instance buffer (mirrors the GL scratch-VBO gather). The index buffer
      // holds u32 instance indices into the Glyph8 vertex buffer.
      const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
      if (vtx && idx && vtxBytes > 0) {
        const std::uint32_t count = geo->indexCount;
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
        std::vector<std::uint8_t> scratch(
            static_cast<std::size_t>(count) * kGlyphStride, 0);
        for (std::uint32_t i = 0; i < count; ++i) {
          const std::uint32_t off = indices[i] * kGlyphStride;
          if (off + kGlyphStride <= vtxBytes) {
            std::memcpy(scratch.data() + static_cast<std::size_t>(i) * kGlyphStride,
                        vtx + off, kGlyphStride);
          }
        }
        if (!scratch.empty()) {
          gb.instanceBuffer = device.createBuffer(
              scratch.size(), scratch.data(), scratch.size());
          gb.instanceCount = count;
        }
      }
    } else if (vtx && vtxBytes > 0) {
      // Non-indexed: upload the Glyph8 records directly; one instance/glyph.
      gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
      gb.instanceCount = geo->vertexCount;
    }
  }

  geoBuffers_.emplace_back(geometryId, gb);
  return geoBuffers_.back().second;
}

BackendStats DawnTextSdfBackend::renderDrawItem(GpuDevice& device,
                                                const Scene& scene,
                                                CpuBufferStore& gpu,
                                                const DrawItem& di,
                                                int /*viewW*/,
                                                int /*viewH*/) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;
  if (!atlas_) return stats;  // textSDF needs the glyph atlas (GL parity)

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  // Upload / re-upload the R8 SDF atlas (mirrors GL uploadAtlasIfDirty, which the
  // Renderer runs once per frame before the scene walk).
  TextureHandle atlasTex = uploadAtlasIfDirty(device);
  if (!atlasTex.valid()) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.instanceBuffer.valid() || gb.instanceCount == 0) return stats;

  // Per-draw uniforms: mat3 transform + vec4 color + the Sampler2D atlas binding
  // (mirrors GL u_transform / u_color / u_atlas).
  const float* xform = resolveTransform(di, scene);

  UniformBinding uniforms[3];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_color";
  uniforms[1].data = di.color;
  uniforms[2].kind = UniformBinding::Kind::Sampler2D;
  uniforms[2].name = "u_atlas";
  uniforms[2].texture = atlasTex;
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
