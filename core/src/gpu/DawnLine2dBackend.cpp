// ENC-486 (P2.3) — DawnLine2dBackend implementation. See header.
//
// The Dawn mirror of the GL `line2d@1` path (Renderer::drawPos2, GL_LINES). It
// owns the line2d render pipeline (LineList topology), uploads the geometry's
// vertex/index data into static Dawn buffers, builds a per-draw bind group with
// the packed transform+color uniform, and issues the indexed-or-arrays LineList
// draw through GpuDevice.
//
// Structure is intentionally identical to DawnTriSolidBackend — the ONLY
// differences are PrimitiveTopology::Lines and the debug name. The pos2 WGSL
// module (vec2 pos -> mat3 transform -> color, with NDC y-flip) is reused
// verbatim from triSolid: line2d shares the exact same vertex layout, uniforms,
// and shader, differing only in primitive assembly.
//
// 1px LIMITATION: WebGPU's LineList renders 1px lines with no width control; the
// GL path's glLineWidth(di.lineWidth) has no native equivalent. di.lineWidth is
// deliberately ignored here — thick lines need quad-expansion, deferred to
// lineAA (ENC-490). See the TODO in renderDrawItem.
#include "dc/gpu/DawnLine2dBackend.hpp"

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

// pos2 WGSL — identical to DawnTriSolidBackend's kTriSolidWgsl. One module, two
// entry points (vs_main / fs_main). vec2 pos -> mat3 transform -> color, with
// the NDC y-flip so the WebGPU top-left framebuffer matches the GL bottom-left
// readback. Reused verbatim: line2d differs from triSolid ONLY in topology.
const char* kPos2Wgsl = R"WGSL(
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
  // top-left after this flip, so the geometry matches the GL baseline.
  return vec4<f32>(p.x, -p.y, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return u.color;
}
)WGSL";

}  // namespace

bool DawnLine2dBackend::init(GpuDevice& device) {
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
  desc.debugName = "line2d@1";
  desc.vertexSource = kPos2Wgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  // LineList: GL_LINES equivalent. 1px lines (no width control in WebGPU).
  desc.topology = PrimitiveTopology::Lines;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

DawnLine2dBackend::GeoBuffers& DawnLine2dBackend::ensureGeoBuffers(
    GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
    std::uint32_t geometryId) {
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) return kv.second;
  }

  GeoBuffers gb;
  const Geometry* geo = scene.getGeometry(geometryId);
  if (geo) {
    // Vertex buffer: static upload of the CPU bytes the CpuBufferStore holds
    // for this geometry (matches triSolid's static-upload approach).
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

BackendStats DawnLine2dBackend::renderDrawItem(GpuDevice& device,
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

  // Per-draw uniforms: mat3 transform + vec4 color (mirrors the GL pos2 path's
  // u_transform / u_color).
  //
  // TODO(ENC-490): di.lineWidth is intentionally ignored. WebGPU's LineList has
  // no line-width control (always 1px); the GL path's glLineWidth(di.lineWidth)
  // has no native equivalent. Widths > 1 require quad-expansion (a thick-line
  // geometry pass), which lands with lineAA in ENC-490.
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

  // NOTE(ENC-485): the per-draw bind group + uniform buffer are held by the
  // render pass until submit and reclaimed at device teardown (same as triSolid).

  stats.drawCalls = ds.drawCalls;
  stats.verticesSubmitted = ds.verticesSubmitted;
  return stats;
}

}  // namespace dc
