// ENC-488 (P2.5) — DawnInstancedRectBackend implementation. See header.
//
// Dawn mirror of Renderer::drawInstancedRect (kInstRectVert/kInstRectFrag). Owns
// the instancedRect render pipeline, uploads the per-instance rect4 records into
// a Dawn instance-step vertex buffer (CPU-gathering the visible subset for the
// D26 indexed path), builds the per-draw bind group (mat3 transform + vec4 color
// + vec2 viewport + f32 cornerRadius), and issues the instanced 6-verts-per-quad
// draw through GpuDevice::drawInstanced.
#include "dc/gpu/DawnInstancedRectBackend.hpp"

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

// rect4 vertex stride: vec4(x0,y0,x1,y1) = 16 bytes (strideOf(Rect4)).
constexpr std::uint32_t kRectStride = 16;

// WGSL port of the GL instancedRect shader (kInstRectVert/kInstRectFrag in
// Renderer.cpp). One module, two entry points.
//
//   * The unit quad's 6 vertices are generated from @builtin(vertex_index) % 6
//     (matching the GL gl_VertexID % 6 quad); there is NO per-vertex buffer.
//   * a_rect (vec4 x0,y0,x1,y1) is the single per-instance attribute at location
//     0 (VertexStepMode::Instance). The quad corner is mix()'d between the rect
//     min/max by the unit uv, transformed by the mat3, then y-flipped to NDC.
//   * v_halfSizePx is the rect half-extent in PIXELS (transform the two rect
//     corners, take the clip-space delta, scale by viewport/2 then *0.5) — used
//     by the rounded-corner SDF. abs() makes it orientation-independent so the
//     NDC y-flip doesn't change the magnitude.
//   * The fragment SDF is the GL rounded-box distance verbatim: round only when
//     cornerRadius > 0; otherwise output the flat color (sharp corners).
//   * Uniform packing matches DawnDevice::createBindGroup's 80-byte block.
const char* kInstRectWgsl = R"WGSL(
struct Uniforms {
  c0           : vec4<f32>,   // transform column 0 (xyz)
  c1           : vec4<f32>,   // transform column 1 (xyz)
  c2           : vec4<f32>,   // transform column 2 (xyz)
  color        : vec4<f32>,
  viewport     : vec2<f32>,   // pixel width/height
  cornerRadius : f32,         // pixels
  _pad         : f32,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position)         pos        : vec4<f32>,
  @location(0)               uv         : vec2<f32>,
  @location(1) @interpolate(flat) halfSizePx : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_rect : vec4<f32>) -> VsOut {
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
  // Negate y (same convention as triSolid) so the WebGPU top-left framebuffer
  // matches the GL bottom-left readback orientation.
  out.pos = vec4<f32>(p.x, -p.y, 0.0, 1.0);
  out.uv = uv;

  // Rect half-size in pixels for the rounded-corner SDF.
  let c0 = m * vec3<f32>(a_rect.x, a_rect.y, 1.0);
  let c1 = m * vec3<f32>(a_rect.z, a_rect.w, 1.0);
  out.halfSizePx = abs(c1.xy - c0.xy) * 0.5 * u.viewport * 0.5;
  return out;
}

@fragment
fn fs_main(@location(0) uv : vec2<f32>,
           @location(1) @interpolate(flat) halfSizePx : vec2<f32>)
    -> @location(0) vec4<f32> {
  if (u.cornerRadius > 0.0) {
    let p = (uv - vec2<f32>(0.5)) * 2.0 * halfSizePx;
    let r = min(u.cornerRadius, min(halfSizePx.x, halfSizePx.y));
    let d = length(max(abs(p) - halfSizePx + vec2<f32>(r), vec2<f32>(0.0))) - r;
    let a = 1.0 - smoothstep(-0.5, 0.5, d);
    return vec4<f32>(u.color.rgb, u.color.a * a);
  }
  return u.color;
}
)WGSL";

}  // namespace

bool DawnInstancedRectBackend::init(GpuDevice& device) {
  // One per-instance attribute: a_rect = Float32x4 @ location 0, stride 16B,
  // VertexStepMode::Instance (GL divisor 1). The unit quad's vertices come from
  // @builtin(vertex_index), so there is no per-vertex buffer.
  VertexAttribute attr;
  attr.location = 0;
  attr.componentCount = 4;
  attr.type = VertexComponentType::Float32;
  attr.offsetBytes = 0;

  VertexBufferLayout layout;
  layout.strideBytes = kRectStride;  // 16B, 4-byte aligned (ENC-485)
  layout.stepInstance = true;        // per-instance step mode
  layout.attributes = &attr;
  layout.attributeCount = 1;

  PipelineDesc desc;
  desc.debugName = "instancedRect@1";
  desc.vertexSource = kInstRectWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 80-byte uniform block: mat3 (3*vec4) + color vec4 + viewport vec2 +
  // cornerRadius f32 + pad. See DawnDevice::createBindGroup packing.
  desc.uniformBytes = 80;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

// ENC-558: (re)gather + (re)upload gb's instance buffer from the geometry's
// CURRENT CpuBufferStore bytes. The previous device buffer (if any) is destroyed
// and replaced — the scratch gather / direct upload content can change
// arbitrarily as the stream grows, so a fresh upload is the simplest correct
// path. Records the source versions/sizes used so the caller can short-circuit
// an unchanged frame.
void DawnInstancedRectBackend::buildGeoBuffers(GpuDevice& device,
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
  // Stamp the versions we are building from up front so an empty/invalid build
  // is still a cache hit (won't rebuild every frame) until the source changes.
  gb.vtxVersion = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  gb.idxVersion = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  gb.built = true;
  if (!geo) return;

  const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
  const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);

  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    // D26 indexed gather: pack only the selected instances into a scratch
    // per-instance buffer (mirrors the GL scratch-VBO gather). The index
    // buffer holds u32 instance indices into the rect4 vertex buffer.
    const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
    if (vtx && idx && vtxBytes > 0) {
      const std::uint32_t count = geo->indexCount;
      const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
      std::vector<std::uint8_t> scratch(
          static_cast<std::size_t>(count) * kRectStride, 0);
      for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t off = indices[i] * kRectStride;
        if (off + kRectStride <= vtxBytes) {
          std::memcpy(scratch.data() + static_cast<std::size_t>(i) * kRectStride,
                      vtx + off, kRectStride);
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
    // Non-indexed: upload the rect4 records directly; one instance per vertex.
    gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    gb.instanceCount = geo->vertexCount;
    gb.bufferCapacity = vtxBytes;
  }
}

DawnInstancedRectBackend::GeoBuffers&
DawnInstancedRectBackend::ensureGeoBuffers(GpuDevice& device, const Scene& scene,
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

  // ENC-558: (re)build on first use OR when the underlying CPU buffer(s) changed
  // since we last built (streaming grow / in-place edit). Otherwise it's a pure
  // cache hit — no per-frame re-upload for static geometry.
  const Geometry* geo = scene.getGeometry(geometryId);
  const std::uint64_t vtxVer = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  const std::uint64_t idxVer = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  if (!gb->built || vtxVer != gb->vtxVersion || idxVer != gb->idxVersion) {
    buildGeoBuffers(device, scene, gpu, geometryId, *gb);
  }
  return *gb;
}

BackendStats DawnInstancedRectBackend::renderDrawItem(GpuDevice& device,
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

  // Per-draw uniforms: mat3 transform + vec4 color + vec2 viewport + f32 corner
  // radius (mirrors the GL u_transform / u_color / u_viewportSize /
  // u_cornerRadius).
  const float* xform = resolveTransform(di, scene);
  const float viewport[2] = {static_cast<float>(viewW),
                             static_cast<float>(viewH)};
  const float cornerRadius = di.cornerRadius;

  UniformBinding uniforms[4];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_color";
  uniforms[1].data = di.color;
  uniforms[2].kind = UniformBinding::Kind::Vec2;
  uniforms[2].name = "u_viewportSize";
  uniforms[2].data = viewport;
  uniforms[3].kind = UniformBinding::Kind::Float;
  uniforms[3].name = "u_cornerRadius";
  uniforms[3].data = &cornerRadius;

  // The single per-instance buffer feeds slot 0 (instance step mode).
  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.instanceBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = {};  // instanced draw: no GPU index buffer (gather is CPU)
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 4;

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
