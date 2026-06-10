// ENC-487 (P2.4) — DawnTriAABackend implementation. See header.
//
// Dawn mirror of Renderer::drawTriAA (kTriAAVert/kTriAAFrag). Owns the triAA
// render pipeline, statically uploads the geometry's vertex/index data into Dawn
// buffers, builds a per-draw bind group with the packed transform+color uniform,
// and issues the indexed-or-arrays TriangleList draw. The per-vertex alpha rides
// the vertex buffer (pos.xy + alpha at +8) and feeds the fragment's v_alpha.
#include "dc/gpu/DawnTriAABackend.hpp"

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

// WGSL port of the GL triAA shader (kTriAAVert/kTriAAFrag). One module, two
// entry points.
//
//   * Uniform layout matches DawnDevice::createBindGroup's packing: three mat3
//     columns padded to vec4 + an RGBA color vec4 (64 bytes total). triAA reads
//     u.c0/c1/c2 for the transform and u.color for the fill RGBA.
//   * Vertex input: a_pos (vec2, location 0) + a_alpha (f32, location 1) off the
//     SAME 12-byte vertex (GL's `a_pos_alpha` vec3, split into two attributes).
//   * v_alpha is interpolated across the face; the fragment outputs
//     vec4(u.color.rgb, u.color.a * v_alpha) — the edge fringe of the GL path.
//   * Y-FLIP: clip.y is negated so the WebGPU top-left framebuffer matches the
//     GL bottom-left readback (same convention as triSolid).
const char* kTriAAWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,   // transform column 0 (xyz)
  c1    : vec4<f32>,   // transform column 1 (xyz)
  c2    : vec4<f32>,   // transform column 2 (xyz)
  color : vec4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position) pos   : vec4<f32>,
  @location(0)       alpha : f32,
};

@vertex
fn vs_main(@location(0) a_pos : vec2<f32>,
           @location(1) a_alpha : f32) -> VsOut {
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(a_pos, 1.0);
  var out : VsOut;
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.alpha = a_alpha;
  return out;
}

@fragment
fn fs_main(@location(0) v_alpha : f32) -> @location(0) vec4<f32> {
  return vec4<f32>(u.color.rgb, u.color.a * v_alpha);
}
)WGSL";

}  // namespace

bool DawnTriAABackend::init(GpuDevice& device) {
  // Two attributes off one 12-byte vertex: pos (Float32x2 @ offset 0, loc 0) +
  // alpha (Float32 @ offset 8, loc 1). Mirrors GL's vec3 a_pos_alpha, stride 12.
  VertexAttribute attrs[2];
  attrs[0].location = 0;
  attrs[0].componentCount = 2;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 1;
  attrs[1].type = VertexComponentType::Float32;
  attrs[1].offsetBytes = 8;

  VertexBufferLayout layout;
  layout.strideBytes = 12;  // pos2 (8) + alpha (4)
  layout.stepInstance = false;
  layout.attributes = attrs;
  layout.attributeCount = 2;

  PipelineDesc desc;
  desc.debugName = "triAA@1";
  desc.vertexSource = kTriAAWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

DawnTriAABackend::GeoBuffers& DawnTriAABackend::ensureGeoBuffers(
    GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
    std::uint32_t geometryId) {
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) return kv.second;
  }

  GeoBuffers gb;
  const Geometry* geo = scene.getGeometry(geometryId);
  if (geo) {
    const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
    const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);
    if (vtx && vtxBytes > 0) {
      gb.vertexBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    }
    gb.vertexCount = geo->vertexCount;

    if (geo->indexBufferId != 0 && geo->indexCount > 0) {
      const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
      const std::uint32_t idxBytes = gpu.getCpuDataSize(geo->indexBufferId);
      if (idx && idxBytes > 0) {
        gb.indexBuffer = device.createBuffer(idxBytes, idx, idxBytes);
        gb.indexCount = geo->indexCount;
      }
    }
  }

  geoBuffers_.emplace_back(geometryId, gb);
  return geoBuffers_.back().second;
}

BackendStats DawnTriAABackend::renderDrawItem(GpuDevice& device,
                                              const Scene& scene,
                                              CpuBufferStore& gpu,
                                              const DrawItem& di,
                                              int /*viewW*/, int /*viewH*/) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.vertexBuffer.valid()) return stats;

  // Per-draw uniforms: mat3 transform + vec4 color (mirrors the GL u_transform /
  // u_color). The fragment multiplies color.a by the interpolated v_alpha.
  const float* xform = resolveTransform(di, scene);
  UniformBinding uniforms[2];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_color";
  uniforms[1].data = di.color;

  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.vertexBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = gb.indexBuffer;  // invalid() => non-indexed
  bgDesc.indexFormat = IndexFormat::Uint32;
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 2;

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
