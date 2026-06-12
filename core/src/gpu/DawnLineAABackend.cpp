// ENC-490 (P2.7) — DawnLineAABackend implementation. See header.
//
// Dawn mirror of Renderer::drawLineAA (kLineAAVert/kLineAAFrag). Owns the lineAA
// render pipeline, uploads the per-instance rect4 segment records into a Dawn
// instance-step vertex buffer (CPU-gathering the visible subset for the D26
// indexed path), builds the per-draw bind group (mat3 transform + vec4 color +
// vec2 viewport + f32 cornerRadius slot + f32 lineWidth + f32 aaWidth +
// f32 fringeEdge + f32 dashLen + f32 gapLen), and issues the instanced
// 6-verts-per-quad draw through GpuDevice::drawInstanced.
//
// WebGPU has no native line width: each segment is expanded into a quad of
// width (lineWidth + 2*aaWidth) in the vertex shader, and the fragment shader
// does the AA coverage falloff + dash on/off — reproducing the GL semantics
// exactly (same v_dist / v_along, same smoothstep, same dash mod()).
#include "dc/gpu/DawnLineAABackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"

#include <algorithm>
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

// rect4 (segment endpoints p0=xy, p1=zw) stride = 16 bytes (strideOf(Rect4)).
constexpr std::uint32_t kRectStride = 16;

// WGSL port of the GL lineAA shader (kLineAAVert/kLineAAFrag in Renderer.cpp).
// One module, two entry points.
//
//   * The unit quad's 6 vertices are generated from @builtin(vertex_index) % 6,
//     each corner a (uv.x in {0,1} along the segment, uv.y in {-1,+1} across) —
//     EXACTLY the GL vid->uv table. There is NO per-vertex buffer.
//   * a_rect (vec4) is the single per-instance attribute at location 0
//     (VertexStepMode::Instance): xy = segment start p0, zw = segment end p1.
//   * Quad expansion: transform p0/p1 by the mat3 (clip space, then y-flipped to
//     NDC); the perp is built from the flipped clip-space direction so the quad
//     is correctly oriented; corners are mix(c0,c1,uv.x) + perp*(uv.y*totalHW),
//     totalHW = lineWidth/2 + aaWidth (clip units).
//   * v_dist = uv.y*totalHW/halfWidth: 0 at center, 1 at nominal edge, >1 in the
//     AA fringe (GL parity). v_along = uv.x * segment length in PIXELS (clip
//     delta * viewport/2) for the dash phase.
//   * Fragment: dash discard (mod over dashLen+gapLen) then AA coverage
//     a = 1 - smoothstep(1, fringeEdge, |v_dist|); composited by Normal blend.
//
// Uniform packing matches DawnDevice::createBindGroup's 96-byte block.
const char* kLineAAWgsl = R"WGSL(
struct Uniforms {
  c0           : vec4<f32>,   // transform column 0 (xyz)
  c1           : vec4<f32>,   // transform column 1 (xyz)
  c2           : vec4<f32>,   // transform column 2 (xyz)
  color        : vec4<f32>,
  viewport     : vec2<f32>,   // pixel width/height
  cornerRadius : f32,         // unused for lineAA (slot parity with instancedRect)
  lineWidth    : f32,         // clip units
  aaWidth      : f32,         // clip units (AA fringe beyond nominal edge)
  fringeEdge   : f32,         // v_dist value where coverage reaches 0
  dashLen      : f32,         // pixels (0 => solid)
  gapLen       : f32,         // pixels
};
@group(0) @binding(0) var<uniform> u : Uniforms;

struct VsOut {
  @builtin(position)         pos    : vec4<f32>,
  @location(0)               vdist  : f32,
  @location(1)               valong : f32,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_rect : vec4<f32>) -> VsOut {
  let p0 = a_rect.xy;
  let p1 = a_rect.zw;

  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let t0 = m * vec3<f32>(p0, 1.0);
  let t1 = m * vec3<f32>(p1, 1.0);
  // Y-flip into NDC (same convention as triSolid/instancedRect). Build the line
  // direction / perp from the already-flipped clip positions.
  let c0 = vec2<f32>(t0.x, -t0.y);
  let c1 = vec2<f32>(t1.x, -t1.y);

  let dir = c1 - c0;
  let len = length(dir);
  let d = select(vec2<f32>(1.0, 0.0), dir / len, len > 0.0001);
  let perp = vec2<f32>(-d.y, d.x);

  let hw = u.lineWidth * 0.5;
  let totalHW = hw + u.aaWidth;

  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, -1.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, -1.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0,  1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0,  1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, -1.0); }
  else              { uv = vec2<f32>(1.0,  1.0); }

  let pos2 = mix(c0, c1, uv.x) + perp * (uv.y * totalHW);

  var out : VsOut;
  out.pos = vec4<f32>(pos2, 0.0, 1.0);
  // v_dist: 0 at center, 1.0 at nominal edge, >1.0 in the AA fringe.
  out.vdist = uv.y * totalHW / max(hw, 0.0001);
  // v_along: pixel-space distance along the line for the dash pattern. The clip
  // delta (dir) maps to pixels via viewport/2 (NDC half-extent == 1).
  let dirPx = dir * u.viewport * 0.5;
  out.valong = uv.x * length(dirPx);
  return out;
}

@fragment
fn fs_main(@location(0) vdist : f32,
           @location(1) valong : f32) -> @location(0) vec4<f32> {
  // D28.1 dash pattern: discard fragments that fall in a gap.
  if (u.dashLen > 0.0) {
    let cycle = u.dashLen + u.gapLen;
    let posInCycle = valong - floor(valong / cycle) * cycle;  // mod(valong, cycle)
    if (posInCycle > u.dashLen) {
      discard;
    }
  }
  let dd = abs(vdist);
  // dd <= 1: inside nominal line (full alpha). dd in (1, fringeEdge): AA fade.
  let a = 1.0 - smoothstep(1.0, u.fringeEdge, dd);
  return vec4<f32>(u.color.rgb, u.color.a * a);
}
)WGSL";

}  // namespace

bool DawnLineAABackend::init(GpuDevice& device) {
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
  desc.debugName = "lineAA@1";
  desc.vertexSource = kLineAAWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;  // AA coverage + dash edges composite
  desc.clip = ClipMode::None;
  // 96-byte uniform block: mat3 (3*vec4) + color vec4 + viewport vec2 +
  // cornerRadius f32 + lineWidth + aaWidth + fringeEdge + dashLen + gapLen.
  desc.uniformBytes = 96;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

// ENC-569: (re)gather + (re)upload gb's instance buffer from the geometry's
// CURRENT CpuBufferStore bytes (mirrors the ENC-558 instanced rect path). The
// previous device buffer (if any) is destroyed and replaced. The instance count
// is derived from the CURRENT buffer size (vtxBytes / rect4=16B) for the
// non-indexed path so a streaming/growing thick-line buffer draws the new
// segments; the indexed path uses the current index count. Records the source
// versions used so an unchanged frame is a no-op.
void DawnLineAABackend::buildGeoBuffers(GpuDevice& device, const Scene& scene,
                                        CpuBufferStore& gpu,
                                        std::uint32_t geometryId,
                                        GeoBuffers& gb) {
  if (gb.instanceBuffer.valid()) {
    device.destroyBuffer(gb.instanceBuffer);
    gb.instanceBuffer = {};
  }
  gb.instanceCount = 0;

  const Geometry* geo = scene.getGeometry(geometryId);
  gb.vtxVersion = geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  gb.idxVersion = geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  gb.built = true;
  if (!geo) return;

  const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
  const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);

  if (geo->indexBufferId != 0 && geo->indexCount > 0) {
    // D26 indexed gather: pack only the selected segments into a scratch
    // per-instance buffer (mirrors the GL scratch-VBO gather). The index
    // buffer holds u32 segment indices into the rect4 vertex buffer. Use the
    // CURRENT index-buffer size so a growing index set draws the new segments.
    const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
    const std::uint32_t idxBytes = gpu.getCpuDataSize(geo->indexBufferId);
    if (vtx && idx && vtxBytes > 0 && idxBytes > 0) {
      const std::uint32_t count = idxBytes / sizeof(std::uint32_t);
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
      }
    }
  } else if (vtx && vtxBytes > 0) {
    // Non-indexed: upload the rect4 segment records directly; one instance
    // (one line segment) per rect4 record in the CURRENT buffer.
    gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    gb.instanceCount = vtxBytes / kRectStride;
  }
}

DawnLineAABackend::GeoBuffers&
DawnLineAABackend::ensureGeoBuffers(GpuDevice& device, const Scene& scene,
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
  const std::uint64_t vtxVer =
      geo ? gpu.getCpuDataVersion(geo->vertexBufferId) : 0;
  const std::uint64_t idxVer =
      geo ? gpu.getCpuDataVersion(geo->indexBufferId) : 0;
  if (!gb->built || vtxVer != gb->vtxVersion || idxVer != gb->idxVersion) {
    buildGeoBuffers(device, scene, gpu, geometryId, *gb);
  }
  return *gb;
}

BackendStats DawnLineAABackend::renderDrawItem(GpuDevice& device,
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

  // Per-draw uniforms, derived exactly as in Renderer::drawLineAA.
  const float* xform = resolveTransform(di, scene);
  const float viewport[2] = {static_cast<float>(viewW),
                             static_cast<float>(viewH)};

  // lineWidth (px) -> clip units (NDC spans 2 over viewW pixels).
  const float lineWidthClip =
      (viewW > 0) ? (di.lineWidth / static_cast<float>(viewW) * 2.0f) : 0.01f;
  // AA fringe: 1.5px beyond the nominal edge, in clip units.
  const float aaWidthClip =
      (viewW > 0) ? (1.5f / static_cast<float>(viewW) * 2.0f) : 0.005f;
  // fringeEdge in v_dist space: (hw + aaWidth) / hw.
  const float hw = lineWidthClip * 0.5f;
  const float fringeEdge =
      (hw > 0.0001f) ? ((hw + aaWidthClip) / hw) : 2.0f;
  const float cornerRadius = 0.0f;  // unused for lineAA (slot parity)
  const float dashLen = di.dashLength;
  const float gapLen = di.gapLength;

  UniformBinding uniforms[9];
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
  uniforms[4].kind = UniformBinding::Kind::Float;
  uniforms[4].name = "u_lineWidth";
  uniforms[4].data = &lineWidthClip;
  uniforms[5].kind = UniformBinding::Kind::Float;
  uniforms[5].name = "u_aaWidth";
  uniforms[5].data = &aaWidthClip;
  uniforms[6].kind = UniformBinding::Kind::Float;
  uniforms[6].name = "u_fringeEdge";
  uniforms[6].data = &fringeEdge;
  uniforms[7].kind = UniformBinding::Kind::Float;
  uniforms[7].name = "u_dashLen";
  uniforms[7].data = &dashLen;
  uniforms[8].kind = UniformBinding::Kind::Float;
  uniforms[8].name = "u_gapLen";
  uniforms[8].data = &gapLen;

  // The single per-instance buffer feeds slot 0 (instance step mode).
  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.instanceBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = {};  // instanced draw: no GPU index buffer (gather is CPU)
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 9;

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
