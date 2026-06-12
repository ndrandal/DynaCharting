// ENC-609 (P2.2) — DawnInstancedPointColorBackend implementation. See header.
//
// The per-POINT color+size scatter. Modeled directly on
// DawnInstancedRectColorBackend (the keystone per-instance-color rect): same unit
// quad (6 verts from @builtin(vertex_index) % 6), same per-instance packed RGBA8
// color, same caching. The differences: the instance record is Point4Color (16B)
// — a CENTER (x,y) + color + a PIXEL size — and the quad is built by offsetting
// the transformed center by ±size/2 in PIXELS (via the viewport) rather than
// interpolating two data-space corners. So each point is a fixed-pixel dot that
// scales with the viewport, not the affine zoom, with its own per-point color.
#include "dc/gpu/DawnInstancedPointColorBackend.hpp"

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

// Point4Color instance stride: vec2(x,y)=8B + RGBA8 color=4B + f32 size=4B = 16B
// (strideOf(Point4Color)). Three attributes carve it.
constexpr std::uint32_t kPointColorStride = 16;

// WGSL: the per-point scatter shader. Each instance is a CENTER (a_pos), a packed
// per-instance color (a_color, location 1, Unorm8x4 -> vec4 0..1) and a PIXEL
// size (a_size, location 2). The unit quad (6 verts from @builtin(vertex_index) %
// 6) is offset from the transformed center by ±size/2 pixels via the viewport, so
// the dot is a fixed pixel size regardless of the affine zoom. NDC y-flip matches
// every other backend (GL bottom-left readback).
//
//   * Uniform block (80B): three mat3 columns (vec4 each) + an unused color vec4 +
//     viewport vec2 + an unused cornerRadius f32 + pad. Identical layout to
//     instancedRectColor@1 so the shared name-driven createBindGroup packing fills
//     u_transform / u_viewportSize at the same offsets (the color + cornerRadius
//     slots are left unused — points are flat discs with no SDF).
const char* kInstPointColorWgsl = R"WGSL(
struct Uniforms {
  c0           : vec4<f32>,   // transform column 0 (xyz)
  c1           : vec4<f32>,   // transform column 1 (xyz)
  c2           : vec4<f32>,   // transform column 2 (xyz)
  _color       : vec4<f32>,   // unused (color is per-instance); kept for layout
  viewport     : vec2<f32>,   // pixel width/height
  cornerRadius : f32,         // unused (points are flat); kept for layout
  _pad         : f32,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position)         pos   : vec4<f32>,
  @location(0)               uv    : vec2<f32>,
  @location(1) @interpolate(flat) color : vec4<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_pos  : vec2<f32>,
           @location(1) a_color : vec4<f32>,
           @location(2) a_size : f32) -> VsOut {
  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, 0.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, 0.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, 0.0); }
  else              { uv = vec2<f32>(1.0, 1.0); }

  // Transform the data-space center to clip space (then y-flip below).
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let c = m * vec3<f32>(a_pos, 1.0);

  // Offset the corner by ±size/2 PIXELS, converted to clip units (2 clip / view
  // pixels). uv in [0,1] -> [-0.5,0.5] half-extent.
  let halfPx = a_size * 0.5;
  let offsetClip = (uv - vec2<f32>(0.5)) * (2.0 * halfPx) / u.viewport;

  var out : VsOut;
  // Negate y (same convention as every backend) AFTER applying the pixel offset
  // so the dot is square in framebuffer space.
  out.pos = vec4<f32>(c.x + offsetClip.x, -(c.y) + offsetClip.y, 0.0, 1.0);
  out.uv = uv;
  out.color = a_color;  // per-point fill color
  return out;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>,
           @location(1) @interpolate(flat) color : vec4<f32>)
    -> @location(0) vec4<f32> {
  return color;
}
)WGSL";

}  // namespace

bool DawnInstancedPointColorBackend::init(GpuDevice& device) {
  // Three per-instance attributes carve the Point4Color record:
  //   a_pos   = Float32x2 @ location 0, offset 0  (x,y center)
  //   a_color = Unorm8x4  @ location 1, offset 8  (packed RGBA8)
  //   a_size  = Float32   @ location 2, offset 12 (pixel diameter)
  // Stride 16B, VertexStepMode::Instance. The unit quad comes from
  // @builtin(vertex_index), so there is no per-vertex buffer.
  VertexAttribute attrs[3];
  attrs[0].location = 0;
  attrs[0].componentCount = 2;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 4;
  attrs[1].type = VertexComponentType::Unorm8x4;  // packed RGBA8 -> vec4 0..1
  attrs[1].offsetBytes = 8;
  attrs[2].location = 2;
  attrs[2].componentCount = 1;
  attrs[2].type = VertexComponentType::Float32;
  attrs[2].offsetBytes = 12;

  VertexBufferLayout layout;
  layout.strideBytes = kPointColorStride;  // 16B, 4-byte aligned
  layout.stepInstance = true;              // per-instance step mode
  layout.attributes = attrs;
  layout.attributeCount = 3;

  PipelineDesc desc;
  desc.debugName = "instancedPointColor@1";
  desc.vertexSource = kInstPointColorWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 80-byte uniform block: mat3 (3*vec4) + unused color vec4 + viewport vec2 +
  // unused cornerRadius f32 + pad. Matches instancedRectColor@1 so the shared
  // name-driven createBindGroup packing fills u_transform / u_viewportSize at the
  // same offsets.
  desc.uniformBytes = 80;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

// (Re)gather + (re)upload gb's instance buffer from the geometry's CURRENT
// CpuBufferStore bytes (ENC-558). Mirrors DawnInstancedRectColorBackend exactly
// with the 16B Point4Color stride.
void DawnInstancedPointColorBackend::buildGeoBuffers(GpuDevice& device,
                                                     const Scene& scene,
                                                     CpuBufferStore& gpu,
                                                     std::uint32_t geometryId,
                                                     GeoBuffers& gb) {
  if (gb.instanceBuffer.valid()) {
    device.destroyBuffer(gb.instanceBuffer);
    gb.instanceBuffer = {};
  }
  gb.instanceCount = 0;
  gb.bufferCapacity = 0;

  const Geometry* geo = scene.getGeometry(geometryId);
  gb.vtxVersion = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  gb.idxVersion = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  gb.built = true;
  if (!geo) return;

  const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
  const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);

  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    // D26 indexed gather: pack only the selected instances into a scratch
    // per-instance buffer. The index buffer holds u32 instance indices.
    const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
    if (vtx && idx && vtxBytes > 0) {
      const std::uint32_t count = geo->indexCount;
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
      std::vector<std::uint8_t> scratch(
          static_cast<std::size_t>(count) * kPointColorStride, 0);
      for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t off = indices[i] * kPointColorStride;
        if (off + kPointColorStride <= vtxBytes) {
          std::memcpy(
              scratch.data() + static_cast<std::size_t>(i) * kPointColorStride,
              vtx + off, kPointColorStride);
        }
      }
      if (!scratch.empty()) {
        gb.instanceBuffer = device.createBuffer(
            scratch.size(), scratch.data(), scratch.size());
        gb.instanceCount = count;
        gb.bufferCapacity = scratch.size();
      }
    }
  } else if (vtx && vtxBytes > 0) {
    // Non-indexed: upload the Point4Color records directly; one instance per row.
    gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    gb.instanceCount = geo->vertexCount;
    gb.bufferCapacity = vtxBytes;
  }
}

DawnInstancedPointColorBackend::GeoBuffers&
DawnInstancedPointColorBackend::ensureGeoBuffers(GpuDevice& device,
                                                 const Scene& scene,
                                                 CpuBufferStore& gpu,
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
  const std::uint64_t vtxVer = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  const std::uint64_t idxVer = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  if (!gb->built || vtxVer != gb->vtxVersion || idxVer != gb->idxVersion) {
    buildGeoBuffers(device, scene, gpu, geometryId, *gb);
  }
  return *gb;
}

BackendStats DawnInstancedPointColorBackend::renderDrawItem(GpuDevice& device,
                                                            const Scene& scene,
                                                            CpuBufferStore& gpu,
                                                            const DrawItem& di,
                                                            int viewW,
                                                            int viewH) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.instanceBuffer.valid() || gb.instanceCount == 0) return stats;

  // Per-draw uniforms: mat3 transform + vec2 viewport. There is NO u_color — the
  // fill color is per-instance (a_color attribute), and the size is per-instance
  // (a_size attribute), so no cornerRadius either.
  const float* xform = resolveTransform(di, scene);
  const float viewport[2] = {static_cast<float>(viewW),
                             static_cast<float>(viewH)};

  UniformBinding uniforms[2];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec2;
  uniforms[1].name = "u_viewportSize";
  uniforms[1].data = viewport;

  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.instanceBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = {};  // instanced draw: no GPU index buffer (gather is CPU)
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 2;

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
