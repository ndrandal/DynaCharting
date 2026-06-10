// ENC-484 (P2.1) — DawnTriSolidBackend implementation. See header.
//
// The Dawn mirror of GlTriSolidBackend: it owns the triSolid render pipeline,
// uploads the geometry's vertex/index data into Dawn buffers (static, ENC-484),
// builds a per-draw bind group with the packed transform+color uniform, and
// issues the indexed-or-arrays TriangleList draw through GpuDevice.
#include "dc/gpu/DawnTriSolidBackend.hpp"

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

// WGSL port of the GL pos2 shader (kPos2Vert/kPos2Frag in Renderer.cpp /
// GlTriSolidBackend.cpp). One module, two entry points.
//
//   * Uniform layout matches DawnDevice::createBindGroup's packing: three mat3
//     columns padded to vec4 + an RGBA color vec4 (64 bytes total). The shader
//     reconstructs the mat3x3 from the column xyz's.
//   * Y-FLIP: clip.y is negated so the WebGPU top-left framebuffer matches the
//     GL bottom-left readback orientation (see header note).
//   * u_pointSize is omitted — triSolid is triangles, gl_PointSize is irrelevant.
const char* kTriSolidWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,   // transform column 0 (xyz)
  c1    : vec4<f32>,   // transform column 1 (xyz)
  c2    : vec4<f32>,   // transform column 2 (xyz)
  color : vec4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

@vertex
fn vs_main(@location(0) a_pos : vec2<f32>) -> @builtin(position) vec4<f32> {
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(a_pos, 1.0);
  // Negate y: GL clip-space y-up read back bottom-left == WebGPU framebuffer
  // top-left after this flip, so the triangle matches the GL baseline.
  return vec4<f32>(p.x, -p.y, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return u.color;
}
)WGSL";

}  // namespace

bool DawnTriSolidBackend::init(GpuDevice& device) {
  // One Float32x2 attribute (a_pos) at location 0; stride = 8 bytes (pos2).
  VertexAttribute attr;
  attr.location = 0;
  attr.componentCount = 2;
  attr.type = VertexComponentType::Float32;
  attr.offsetBytes = 0;

  VertexBufferLayout layout;
  layout.strideBytes = 8;  // strideOf(VertexFormat::Pos2_Clip)
  layout.stepInstance = false;
  layout.attributes = &attr;
  layout.attributeCount = 1;

  PipelineDesc desc;
  desc.debugName = "triSolid@1";
  // WGSL is a single module carrying both entry points; pass it as the vertex
  // source (DawnDevice compiles one module and looks up vs_main / fs_main).
  desc.vertexSource = kTriSolidWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

DawnTriSolidBackend::GeoBuffers& DawnTriSolidBackend::ensureGeoBuffers(
    GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
    std::uint32_t geometryId) {
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) return kv.second;
  }

  GeoBuffers gb;
  const Geometry* geo = scene.getGeometry(geometryId);
  if (geo) {
    // Vertex buffer: copy the CPU bytes the CpuBufferStore holds for this
    // geometry's vertex buffer into a static Dawn buffer. (The live coalesced
    // streaming path lands per-pipeline in ENC-488/489/490 via
    // CpuBufferStore::uploadDirty + DeviceBufferResolver; triSolid geometry is
    // uploaded once here.)
    const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
    const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);
    if (vtx && vtxBytes > 0) {
      gb.vertexBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    }
    gb.vertexCount = geo->vertexCount;

    // Index buffer (optional): same static upload.
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

BackendStats DawnTriSolidBackend::renderDrawItem(GpuDevice& device,
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

  // Per-draw uniforms: mat3 transform + vec4 color (mirrors GlTriSolidBackend's
  // u_transform / u_color). u_pointSize is intentionally omitted (triangles).
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

  // NOTE(ENC-485): the bind group + its uniform buffer are created per draw and
  // are NOT destroyed here — the render pass holds them until submit, and
  // destroying mid-pass would race that. They are reclaimed at device teardown.
  // A per-frame bind-group recycle/uniform-ring lands with the streaming model.

  stats.drawCalls = ds.drawCalls;
  stats.verticesSubmitted = ds.verticesSubmitted;
  return stats;
}

}  // namespace dc
