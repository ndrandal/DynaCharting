// ENC-487 (P2.4) — DawnTriGradientBackend implementation. See header.
//
// Dawn mirror of Renderer::drawTriGradient (kTriGradientVert/kTriGradientFrag).
// Owns the triGradient render pipeline, statically uploads the geometry's
// vertex/index data into Dawn buffers, builds a per-draw bind group with the
// packed transform uniform, and issues the indexed-or-arrays TriangleList draw.
// The per-vertex RGBA color rides the vertex buffer (pos.xy + color at +8) and
// is interpolated across the face by the rasterizer.
#include "dc/gpu/DawnTriGradientBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"

namespace dc {

namespace {

const float kIdentityMat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

const float* resolveTransform(const DrawItem& di, const Scene& scene) {
  if (!di.transformId) return kIdentityMat3;
  const Transform* t = scene.getTransform(di.transformId);
  return t ? t->mat3 : kIdentityMat3;
}

// WGSL port of the GL triGradient shader (kTriGradientVert/kTriGradientFrag).
// One module, two entry points.
//
//   * Uniform layout matches DawnDevice::createBindGroup's packing: three mat3
//     columns padded to vec4 + an (unused) color vec4 (64 bytes). triGradient
//     reads only u.c0/c1/c2 for the transform; there is NO color uniform — color
//     is per-vertex.
//   * Vertex input: a_pos (vec2, location 0) + a_color (vec4, location 1) off the
//     SAME 24-byte vertex (pos2_color4: pos at 0, color at +8).
//   * v_color is interpolated across the face; the fragment outputs it verbatim
//     (the gradient fill).
//   * Y-FLIP: clip.y is negated so the WebGPU top-left framebuffer matches the
//     GL bottom-left readback (same convention as triSolid).
const char* kTriGradientWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,   // transform column 0 (xyz)
  c1    : vec4<f32>,   // transform column 1 (xyz)
  c2    : vec4<f32>,   // transform column 2 (xyz)
  color : vec4<f32>,   // unused by triGradient (color is per-vertex)
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position) pos   : vec4<f32>,
  @location(0)       color : vec4<f32>,
};

@vertex
fn vs_main(@location(0) a_pos : vec2<f32>,
           @location(1) a_color : vec4<f32>) -> VsOut {
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(a_pos, 1.0);
  var out : VsOut;
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.color = a_color;
  return out;
}

@fragment
fn fs_main(@location(0) v_color : vec4<f32>) -> @location(0) vec4<f32> {
  return v_color;
}
)WGSL";

}  // namespace

bool DawnTriGradientBackend::init(GpuDevice& device) {
  // Two attributes off one 24-byte vertex (pos2_color4): pos (Float32x2 @ 0,
  // loc 0) + color (Float32x4 @ 8, loc 1). Mirrors the GL a_pos / a_color layout.
  VertexAttribute attrs[2];
  attrs[0].location = 0;
  attrs[0].componentCount = 2;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 4;
  attrs[1].type = VertexComponentType::Float32;
  attrs[1].offsetBytes = 8;

  VertexBufferLayout layout;
  layout.strideBytes = 24;  // strideOf(VertexFormat::Pos2Color4)
  layout.stepInstance = false;
  layout.attributes = attrs;
  layout.attributeCount = 2;

  PipelineDesc desc;
  desc.debugName = "triGradient@1";
  desc.vertexSource = kTriGradientWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

// ENC-569: (re)upload gb's vertex/index buffers from the geometry's CURRENT CPU
// bytes (see DawnTriSolidBackend for the rationale). The DRAW vertex count is
// derived from the current buffer size (vtxBytes / strideOf(pos2_color4=24B))
// so a growing / re-tessellated triGradient buffer re-shapes instead of being
// frozen at the first frame's vertexCount.
void DawnTriGradientBackend::buildGeoBuffers(GpuDevice& device,
                                             const Scene& scene,
                                             CpuBufferStore& gpu,
                                             std::uint32_t geometryId,
                                             GeoBuffers& gb) {
  if (gb.vertexBuffer.valid()) {
    device.destroyBuffer(gb.vertexBuffer);
    gb.vertexBuffer = {};
  }
  if (gb.indexBuffer.valid()) {
    device.destroyBuffer(gb.indexBuffer);
    gb.indexBuffer = {};
  }
  gb.vertexCount = 0;
  gb.indexCount = 0;

  const Geometry* geo = scene.getGeometry(geometryId);
  gb.vtxVersion = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  gb.idxVersion = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  gb.built = true;
  if (!geo) return;

  const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
  const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);
  if (vtx && vtxBytes > 0) {
    gb.vertexBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    const std::uint32_t stride = strideOf(geo->format);
    gb.vertexCount = stride > 0 ? (vtxBytes / stride) : geo->vertexCount;
  }

  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
    const std::uint32_t idxBytes = gpu.getCpuDataSize(geo->indexBufferId);
    if (idx && idxBytes > 0) {
      gb.indexBuffer = device.createBuffer(idxBytes, idx, idxBytes);
      gb.indexCount = idxBytes / sizeof(std::uint32_t);
    }
  }
}

DawnTriGradientBackend::GeoBuffers& DawnTriGradientBackend::ensureGeoBuffers(
    GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
    std::uint32_t geometryId) {
  GeoBuffers* gb = nullptr;
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) { gb = &kv.second; break; }
  }
  if (!gb) {
    geoBuffers_.emplace_back(geometryId, GeoBuffers{});
    gb = &geoBuffers_.back().second;
  }

  const Geometry* geo = scene.getGeometry(geometryId);
  const std::uint64_t vtxVer =
      geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  const std::uint64_t idxVer =
      geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  if (!gb->built || vtxVer != gb->vtxVersion || idxVer != gb->idxVersion) {
    buildGeoBuffers(device, scene, gpu, geometryId, *gb);
  }
  return *gb;
}

BackendStats DawnTriGradientBackend::renderDrawItem(GpuDevice& device,
                                                    const Scene& scene,
                                                    CpuBufferStore& gpu,
                                                    const DrawItem& di,
                                                    int /*viewW*/,
                                                    int /*viewH*/) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.vertexBuffer.valid()) return stats;

  // Per-draw uniforms: mat3 transform only (mirrors the GL u_transform). No
  // color uniform — the color is per-vertex. The 64-byte uniform's color slot is
  // left zero (the fragment ignores it).
  const float* xform = resolveTransform(di, scene);
  UniformBinding uniforms[1];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;

  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.vertexBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = gb.indexBuffer;  // invalid() => non-indexed
  bgDesc.indexFormat = IndexFormat::Uint32;
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 1;

  BindGroupHandle group = device.createBindGroup(bgDesc);
  if (!group.valid()) return stats;

  device.bindPipeline(pipeline_);

  DrawParams params;
  params.vertexCount = gb.vertexCount;
  params.indexCount = gb.indexBuffer.valid() ? gb.indexCount : 0u;
  params.firstVertex = 0;

  DeviceDrawStats ds = device.draw(group, params);

  stats.drawCalls = ds.drawCalls;
  stats.verticesSubmitted = ds.verticesSubmitted;
  return stats;
}

}  // namespace dc
