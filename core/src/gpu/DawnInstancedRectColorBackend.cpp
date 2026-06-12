// ENC-608 (P2.1) — DawnInstancedRectColorBackend implementation. See header.
//
// The keystone per-instance-color rect. Modeled directly on
// DawnInstancedRectBackend (the single-uniform-color rect), generalizing the
// Candle6 per-instance multi-channel approach: instead of a u_color uniform, the
// fill color is a per-instance packed RGBA8 attribute (location 1, Unorm8x4)
// read straight from the Rect4Color (24B) instance record. This is what lets the
// weather-radar / correlation / footprint / pie views render natively with ZERO
// compute (each cell its own color in one instanced draw).
#include "dc/gpu/DawnInstancedRectColorBackend.hpp"

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

// Rect4Color instance stride: vec4(x0,y0,x1,y1)=16B + RGBA8 color=4B + f32
// scalar/rowid lane=4B = 24B (strideOf(Rect4Color)). Three attributes carve it.
constexpr std::uint32_t kRectColorStride = 24;

// WGSL: the instancedRect@1 shader (DawnInstancedRectBackend) with the fill color
// promoted from a uniform to a PER-INSTANCE attribute (a_color, location 1,
// Unorm8x4 -> vec4<f32> 0..1). The mat3 transform, viewport, rounded-corner SDF
// (D28.2) and NDC y-flip are unchanged.
//
//   * Unit quad: 6 verts from @builtin(vertex_index) % 6 (no per-vertex buffer).
//   * a_rect (vec4 x0,y0,x1,y1) @ location 0, a_color (vec4 rgba) @ location 1,
//     a_scalar (f32) @ location 2 — all VertexStepMode::Instance (divisor 1).
//     a_scalar is the reserved scalar/row-id lane; the shader does not consume it
//     today (it exists so the attribute stride is honored + the lane is live for
//     a future picking path).
//   * Uniform block (64B): three mat3 columns (vec4 each) + viewport vec2 +
//     cornerRadius f32. NO u_color (color is per-instance). The host packs
//     u_transform (bytes 0..47), u_viewportSize (bytes 64..71) and u_cornerRadius
//     (bytes 72..75) via DawnDevice::createBindGroup's name-driven packing, so we
//     declare an 80-byte block (matching instancedRect@1) and leave the color
//     slot (bytes 48..63) unused.
const char* kInstRectColorWgsl = R"WGSL(
struct Uniforms {
  c0           : vec4<f32>,   // transform column 0 (xyz)
  c1           : vec4<f32>,   // transform column 1 (xyz)
  c2           : vec4<f32>,   // transform column 2 (xyz)
  _color       : vec4<f32>,   // unused (color is per-instance); kept for layout
  viewport     : vec2<f32>,   // pixel width/height
  cornerRadius : f32,         // pixels
  _pad         : f32,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position)         pos        : vec4<f32>,
  @location(0)               uv         : vec2<f32>,
  @location(1) @interpolate(flat) halfSizePx : vec2<f32>,
  @location(2) @interpolate(flat) color      : vec4<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_rect  : vec4<f32>,
           @location(1) a_color : vec4<f32>,
           @location(2) a_scalar : f32) -> VsOut {
  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, 0.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, 0.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0, 1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, 0.0); }
  else              { uv = vec2<f32>(1.0, 1.0); }

  let x = mix(a_rect.x, a_rect.z, uv.x);
  let y = mix(a_rect.y, a_rect.w, uv.y);

  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(x, y, 1.0);

  var out : VsOut;
  // Negate y (same convention as instancedRect) so the WebGPU top-left
  // framebuffer matches the GL bottom-left readback orientation.
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.uv = uv;
  out.color = a_color;  // per-instance fill color (the keystone)

  // Rect half-size in pixels for the rounded-corner SDF.
  let c0 = m * vec3<f32>(a_rect.x, a_rect.y, 1.0);
  let c1 = m * vec3<f32>(a_rect.z, a_rect.w, 1.0);
  out.halfSizePx = abs(c1.xy - c0.xy) * 0.5 * u.viewport * 0.5;

  // The reserved scalar/row-id lane is read here so the attribute is live
  // (consumed) without affecting output; a future picking path repurposes it.
  let _keepScalarLive = a_scalar;
  return out;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>,
           @location(1) @interpolate(flat) halfSizePx : vec2<f32>,
           @location(2) @interpolate(flat) color : vec4<f32>)
    -> @location(0) vec4<f32> {
  if (u.cornerRadius > 0.0) {
    let p = (uv - vec2<f32>(0.5)) * 2.0 * halfSizePx;
    let r = min(u.cornerRadius, min(halfSizePx.x, halfSizePx.y));
    let d = length(max(abs(p) - halfSizePx + vec2<f32>(r), vec2<f32>(0.0))) - r;
    let a = 1.0 - smoothstep(-0.5, 0.5, d);
    return vec4<f32>(color.rgb, color.a * a);
  }
  return color;
}
)WGSL";

}  // namespace

bool DawnInstancedRectColorBackend::init(GpuDevice& device) {
  // Three per-instance attributes carve the Rect4Color record:
  //   a_rect   = Float32x4 @ location 0, offset 0   (x0,y0,x1,y1)
  //   a_color  = Unorm8x4  @ location 1, offset 16  (packed RGBA8)
  //   a_scalar = Float32   @ location 2, offset 20  (reserved scalar/row-id lane)
  // Stride 24B, VertexStepMode::Instance. The unit quad comes from
  // @builtin(vertex_index), so there is no per-vertex buffer.
  VertexAttribute attrs[3];
  attrs[0].location = 0;
  attrs[0].componentCount = 4;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 4;
  attrs[1].type = VertexComponentType::Unorm8x4;  // packed RGBA8 -> vec4 0..1
  attrs[1].offsetBytes = 16;
  attrs[2].location = 2;
  attrs[2].componentCount = 1;
  attrs[2].type = VertexComponentType::Float32;
  attrs[2].offsetBytes = 20;

  VertexBufferLayout layout;
  layout.strideBytes = kRectColorStride;  // 24B, 4-byte aligned (ENC-485)
  layout.stepInstance = true;             // per-instance step mode
  layout.attributes = attrs;
  layout.attributeCount = 3;

  PipelineDesc desc;
  desc.debugName = "instancedRectColor@1";
  desc.vertexSource = kInstRectColorWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 80-byte uniform block: mat3 (3*vec4) + unused color vec4 + viewport vec2 +
  // cornerRadius f32 + pad. Matches instancedRect@1's block so the shared
  // name-driven createBindGroup packing fills u_transform / u_viewportSize /
  // u_cornerRadius at the same offsets (color slot left unused).
  desc.uniformBytes = 80;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

// (Re)gather + (re)upload gb's instance buffer from the geometry's CURRENT
// CpuBufferStore bytes (ENC-558). Mirrors DawnInstancedRectBackend exactly with
// the 24B Rect4Color stride.
void DawnInstancedRectColorBackend::buildGeoBuffers(GpuDevice& device,
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
          static_cast<std::size_t>(count) * kRectColorStride, 0);
      for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t off = indices[i] * kRectColorStride;
        if (off + kRectColorStride <= vtxBytes) {
          std::memcpy(
              scratch.data() + static_cast<std::size_t>(i) * kRectColorStride,
              vtx + off, kRectColorStride);
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
    // Non-indexed: upload the Rect4Color records directly; one instance per row.
    gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    gb.instanceCount = geo->vertexCount;
    gb.bufferCapacity = vtxBytes;
  }
}

DawnInstancedRectColorBackend::GeoBuffers&
DawnInstancedRectColorBackend::ensureGeoBuffers(GpuDevice& device,
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

BackendStats DawnInstancedRectColorBackend::renderDrawItem(GpuDevice& device,
                                                           const Scene& scene,
                                                           CpuBufferStore& gpu,
                                                           const DrawItem& di,
                                                           int viewW, int viewH) {
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.instanceBuffer.valid() || gb.instanceCount == 0) return stats;

  // Per-draw uniforms: mat3 transform + vec2 viewport + f32 cornerRadius. There
  // is NO u_color — the fill color is per-instance (a_color attribute).
  const float* xform = resolveTransform(di, scene);
  const float viewport[2] = {static_cast<float>(viewW),
                             static_cast<float>(viewH)};
  const float cornerRadius = di.cornerRadius;

  UniformBinding uniforms[3];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec2;
  uniforms[1].name = "u_viewportSize";
  uniforms[1].data = viewport;
  uniforms[2].kind = UniformBinding::Kind::Float;
  uniforms[2].name = "u_cornerRadius";
  uniforms[2].data = &cornerRadius;

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
