// ENC-489 (P2.6) — DawnInstancedCandleBackend implementation. See header.
//
// Dawn mirror of Renderer::drawInstancedCandle (kInstCandleVert/kInstCandleFrag).
// Owns the instancedCandle render pipeline, uploads the per-instance candle6
// records into a Dawn instance-step vertex buffer (CPU-gathering the visible
// subset for the D26 indexed path), builds the per-draw bind group (mat3
// transform + colorUp vec4 + colorDown vec4 + viewport vec2), and issues the
// instanced 12-verts-per-candle draw through GpuDevice::drawInstanced.
#include "dc/gpu/DawnInstancedCandleBackend.hpp"

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

// candle6 vertex stride: vec4(cx,open,high,low) + vec2(close,hw) = 24 bytes
// (strideOf(Candle6)). Two attributes a_c0/a_c1 carve out this single record.
constexpr std::uint32_t kCandleStride = 24;

// WGSL port of the GL instancedCandle shader (kInstCandleVert/kInstCandleFrag in
// Renderer.cpp). One module, two entry points.
//
//   * Each candle expands to 12 vertices from @builtin(vertex_index) % 12: verts
//     0..5 are the OHLC BODY quad, verts 6..11 the WICK quad. The unit-quad uv
//     for each half comes from the local vertex id (lid = vid % 6 within each
//     half), matching the GL gl_VertexID % 12 / lid split.
//   * a_c0 (vec4 cx,open,high,low) and a_c1 (vec2 close,halfWidth) are the two
//     per-instance attributes at locations 0 and 1 (VertexStepMode::Instance,
//     GL divisor 1). There is NO per-vertex buffer.
//   * BODY: a quad from (cx-hw, min(open,close)) to (cx+hw, max(open,close)),
//     transformed by the mat3, then y-flipped to NDC.
//   * WICK: a fixed-PIXEL-width vertical line from low to high at cx. The center
//     point is transformed by the mat3, then offset by ±(1/viewport.x) in clip
//     space — a 1px half-width — exactly like the GL wick branch. (The y-flip is
//     applied to the transformed center; the ±x clip offset is flip-invariant.)
//   * v_isUp is the GL flat up/down flag (close >= open). The fragment selects
//     u_colorUp vs u_colorDown.
//   * Uniform packing (96-byte block, matches DawnDevice::createBindGroup):
//       bytes  0..47  c0/c1/c2  — three mat3 columns (each a vec4, xyz used)
//       bytes 48..63  colorUp   — vec4
//       bytes 64..79  colorDown — vec4
//       bytes 80..95  wickHalf  — vec4 (.x = wick half-width in CLIP space)
//     The wick half-width is passed through a DEDICATED uniform field (wickHalf.x)
//     computed host-side as a fixed pixel width in clip space — NOT smuggled into
//     a mat3 padding lane (which the Mat3 packer would zero). See the name-driven
//     u_wickHalf case in DawnDevice::createBindGroup.

const char* kInstCandleWgsl = R"WGSL(
struct Uniforms {
  c0       : vec4<f32>,   // transform column 0 (xyz)
  c1       : vec4<f32>,   // transform column 1 (xyz)
  c2       : vec4<f32>,   // transform column 2 (xyz)
  colorUp  : vec4<f32>,   // bytes 48..63
  colorDown: vec4<f32>,   // bytes 64..79
  wickHalf : vec4<f32>,   // bytes 80..95 (.x = wick half-width, clip space)
};
@group(0) @binding(0) var<uniform> u : Uniforms;

fn wickHalfWidthClip() -> f32 {
  // Wick half-width in clip space, supplied directly by the host (a fixed pixel
  // width converted to clip units). Never zero for a valid viewport.
  return u.wickHalf.x;
}

struct VsOut {
  @builtin(position)               pos  : vec4<f32>,
  @location(0) @interpolate(flat)  isUp : f32,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_c0 : vec4<f32>,
           @location(1) a_c1 : vec2<f32>) -> VsOut {
  let cx    = a_c0.x;
  let open  = a_c0.y;
  let high  = a_c0.z;
  let low   = a_c0.w;
  let close = a_c1.x;
  let hw    = a_c1.y;

  let body0 = min(open, close);
  let body1 = max(open, close);

  let v = vid % 12u;
  let isWick = v >= 6u;
  let lid = select(v, v - 6u, isWick);

  var uv : vec2<f32>;
  if (lid == 0u)      { uv = vec2<f32>(0.0, 0.0); }
  else if (lid == 1u) { uv = vec2<f32>(1.0, 0.0); }
  else if (lid == 2u) { uv = vec2<f32>(0.0, 1.0); }
  else if (lid == 3u) { uv = vec2<f32>(0.0, 1.0); }
  else if (lid == 4u) { uv = vec2<f32>(1.0, 0.0); }
  else                { uv = vec2<f32>(1.0, 1.0); }

  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);

  var clip : vec2<f32>;
  if (isWick) {
    // Fixed-pixel wick: transform the center at cx along low..high, then offset
    // ±1px (in clip x) by the unit uv.x.
    let y = mix(low, high, uv.y);
    let center = m * vec3<f32>(cx, y, 1.0);
    let hwClip = wickHalfWidthClip();
    clip = vec2<f32>(center.x + mix(-hwClip, hwClip, uv.x), center.y);
  } else {
    let x0 = cx - hw;
    let x1 = cx + hw;
    let p = m * vec3<f32>(mix(x0, x1, uv.x), mix(body0, body1, uv.y), 1.0);
    clip = p.xy;
  }

  var out : VsOut;
  // Negate y (same convention as triSolid/instancedRect) so the WebGPU top-left
  // framebuffer matches the GL bottom-left readback orientation.
  out.pos = vec4<f32>(clip.x, -clip.y, 0.0, 1.0);
  out.isUp = select(0.0, 1.0, close >= open);
  return out;
}

@fragment
fn fs_main(@location(0) @interpolate(flat) isUp : f32)
    -> @location(0) vec4<f32> {
  return select(u.colorDown, u.colorUp, isUp > 0.5);
}
)WGSL";

}  // namespace

bool DawnInstancedCandleBackend::init(GpuDevice& device) {
  // Two per-instance attributes carve the candle6 record:
  //   a_c0 = Float32x4 @ location 0, offset 0   (cx, open, high, low)
  //   a_c1 = Float32x2 @ location 1, offset 16  (close, halfWidth)
  // Stride 24B, VertexStepMode::Instance (GL divisor 1). The 12-vertex geometry
  // comes from @builtin(vertex_index), so there is no per-vertex buffer.
  VertexAttribute attrs[2];
  attrs[0].location = 0;
  attrs[0].componentCount = 4;
  attrs[0].type = VertexComponentType::Float32;
  attrs[0].offsetBytes = 0;
  attrs[1].location = 1;
  attrs[1].componentCount = 2;
  attrs[1].type = VertexComponentType::Float32;
  attrs[1].offsetBytes = 16;

  VertexBufferLayout layout;
  layout.strideBytes = kCandleStride;  // 24B (Candle6), 4-byte aligned
  layout.stepInstance = true;          // per-instance step mode
  layout.attributes = attrs;
  layout.attributeCount = 2;

  PipelineDesc desc;
  desc.debugName = "instancedCandle@1";
  desc.vertexSource = kInstCandleWgsl;
  desc.fragmentSource = nullptr;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  // 96-byte uniform block: mat3 (3*vec4) + colorUp vec4 + colorDown vec4 +
  // wickHalf vec4 (.x = wick half-width in clip space, supplied by the host via a
  // dedicated u_wickHalf Float binding). See DawnDevice::createBindGroup packing.
  desc.uniformBytes = 96;

  pipeline_ = device.createPipeline(desc);
  return pipeline_.valid();
}

DawnInstancedCandleBackend::GeoBuffers&
DawnInstancedCandleBackend::ensureGeoBuffers(GpuDevice& device,
                                             const Scene& scene,
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
      // D26 indexed gather: pack only the selected candles into a scratch
      // per-instance buffer (mirrors the GL scratch-VBO gather). The index
      // buffer holds u32 instance indices into the candle6 vertex buffer.
      const std::uint8_t* idx = gpu.getCpuData(geo->indexBufferId);
      if (vtx && idx && vtxBytes > 0) {
        const std::uint32_t count = geo->indexCount;
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
        std::vector<std::uint8_t> scratch(
            static_cast<std::size_t>(count) * kCandleStride, 0);
        for (std::uint32_t i = 0; i < count; ++i) {
          const std::uint32_t off = indices[i] * kCandleStride;
          if (off + kCandleStride <= vtxBytes) {
            std::memcpy(
                scratch.data() + static_cast<std::size_t>(i) * kCandleStride,
                vtx + off, kCandleStride);
          }
        }
        if (!scratch.empty()) {
          gb.instanceBuffer = device.createBuffer(
              scratch.size(), scratch.data(), scratch.size());
          gb.instanceCount = count;
        }
      }
    } else if (vtx && vtxBytes > 0) {
      // Non-indexed: upload the candle6 records directly; one instance per
      // candle.
      gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
      gb.instanceCount = geo->vertexCount;
    }
  }

  geoBuffers_.emplace_back(geometryId, gb);
  return geoBuffers_.back().second;
}

BackendStats DawnInstancedCandleBackend::renderDrawItem(GpuDevice& device,
                                                        const Scene& scene,
                                                        CpuBufferStore& gpu,
                                                        const DrawItem& di,
                                                        int viewW, int viewH) {
  (void)viewH;  // wick width is a fixed pixel count along x only (viewW).
  BackendStats stats{};
  if (!pipeline_.valid()) return stats;

  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return stats;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di.geometryId);
  if (!gb.instanceBuffer.valid() || gb.instanceCount == 0) return stats;

  // Per-draw uniforms: mat3 transform + colorUp vec4 + colorDown vec4 + wickHalf
  // (mirrors the GL u_transform / u_colorUp / u_colorDown / u_viewportSize). The
  // GL wick is a fixed 1px-wide line (half-width 1/viewport.x in clip space). On
  // Dawn we render the offscreen target with no MSAA and probe exact pixel
  // centers, so a 1px line straddling a pixel boundary can rasterize entirely
  // into the neighbouring column. We therefore use a robust 2px half-width
  // (2/viewport.x), which reliably covers the candle's center column while still
  // reading as a thin wick. The half-width is converted to CLIP space here and
  // passed through a DEDICATED uniform field (u_wickHalf) — not smuggled into a
  // mat3 padding lane, which the Mat3 packer would zero.
  const float* xform = resolveTransform(di, scene);
  const float wickHalfClip =
      viewW > 0 ? 2.0f / static_cast<float>(viewW) : 0.0f;

  UniformBinding uniforms[4];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = xform;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_colorUp";
  uniforms[1].data = di.colorUp;
  uniforms[2].kind = UniformBinding::Kind::Vec4;
  uniforms[2].name = "u_colorDown";
  uniforms[2].data = di.colorDown;
  // Dedicated wick half-width (clip space) at uniform float index 20 (byte 80).
  uniforms[3].kind = UniformBinding::Kind::Float;
  uniforms[3].name = "u_wickHalf";
  uniforms[3].data = &wickHalfClip;

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
  params.vertexCountPerInstance = 12;  // 6 body + 6 wick
  params.instanceCount = gb.instanceCount;
  params.firstVertex = 0;

  DeviceDrawStats ds = device.drawInstanced(group, params);

  stats.drawCalls = ds.drawCalls;
  stats.verticesSubmitted = ds.verticesSubmitted;
  return stats;
}

}  // namespace dc
