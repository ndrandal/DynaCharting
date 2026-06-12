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

// ENC-569: (re)upload gb's vertex/index buffers from the geometry's CURRENT
// CpuBufferStore bytes. The previous device buffer(s) (if any) are destroyed and
// replaced — a grown/edited stream changes the byte count arbitrarily, so a
// fresh upload is the simplest correct path. The DRAW vertex count is derived
// from the current vertex-buffer size (vtxBytes / strideOf(format)) rather than
// the static geometry.vertexCount, so a growing buffer draws the new vertices.
// Records the source versions used so the caller can short-circuit an unchanged
// frame.
void DawnTriSolidBackend::buildGeoBuffers(GpuDevice& device, const Scene& scene,
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
  // Stamp the versions we are building from up front so an empty/invalid build
  // is still a cache hit until the source changes.
  gb.vtxVersion = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  gb.idxVersion = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  gb.built = true;
  if (!geo) return;

  const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
  const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);
  if (vtx && vtxBytes > 0) {
    gb.vertexBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    // Derive the draw count from the CURRENT buffer size, not the (possibly
    // stale) static geometry.vertexCount — a streaming/re-tessellated buffer
    // draws every vertex it now holds.
    const std::uint32_t stride = strideOf(geo->format);
    gb.vertexCount = stride > 0 ? (vtxBytes / stride) : geo->vertexCount;
  }

  // Index buffer (optional): same fresh upload; u32 indices => idxBytes / 4.
  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
    const std::uint32_t idxBytes = gpu.getCpuDataSize(geo->indexBufferId);
    if (idx && idxBytes > 0) {
      gb.indexBuffer = device.createBuffer(idxBytes, idx, idxBytes);
      gb.indexCount = idxBytes / sizeof(std::uint32_t);
    }
  }
}

DawnTriSolidBackend::GeoBuffers& DawnTriSolidBackend::ensureGeoBuffers(
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

  // ENC-569: (re)build on first use OR when the underlying CPU buffer(s) changed
  // since we last built (streaming grow / in-place edit). Otherwise pure cache
  // hit — no per-frame re-upload for static geometry.
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
