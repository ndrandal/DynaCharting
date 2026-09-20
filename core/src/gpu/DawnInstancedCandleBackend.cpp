// ENC-489 (P2.6) — DawnInstancedCandleBackend implementation. See header.
//
// Dawn mirror of Renderer::drawInstancedCandle (kInstCandleVert/kInstCandleFrag).
// Owns the instancedCandle render pipeline, uploads the per-instance candle6
// records into a Dawn instance-step vertex buffer (CPU-gathering the visible
// subset for the D26 indexed path), builds the per-draw bind group (mat3
// transform + colorUp vec4 + colorDown vec4 + viewport vec2), and issues the
// instanced 12-verts-per-candle draw through GpuDevice::drawInstanced.
#include "dc/gpu/DawnInstancedCandleBackend.hpp"

#include "dc/render/BarSizing.hpp"
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
//       bytes 80..95  wickHalf  — vec4 (.x = wick half-width in CLIP space,
//                                       .y = resolved BODY half-width, clip; 0 = off)
//     The wick half-width is passed through a DEDICATED uniform field (wickHalf.x)
//     computed host-side as a fixed pixel width in clip space — NOT smuggled into
//     a mat3 padding lane (which the Mat3 packer would zero). See the name-driven
//     u_wickHalf case in DawnDevice::createBindGroup.
//   * ENC-1257 — BODY WIDTH IS RESOLVED HOST-SIDE. wickHalf.y carries the body
//     half-width the bar-sizing rule chose (dc/render/BarSizing.hpp): the
//     per-instance `halfWidth` is a data-space number and therefore a
//     scale-free RATIO, which is why the same 0.4 constant fused the showcase's
//     candles into slabs at a 4.5px pitch and produced 47px blocks on the live
//     capture. The host derives the bar pitch from the instance x values,
//     converts to pixels through the transform + viewport, applies the pixel
//     floor/ceiling, and hands the result back in clip space. When the rule does
//     NOT apply (a single bar, no viewport, degenerate transform) wickHalf.y is
//     0 and the shader falls back to the per-instance halfWidth EXACTLY as
//     before — which is what keeps every pre-existing single-bar render
//     byte-identical.

const char* kInstCandleWgsl = R"WGSL(
struct Uniforms {
  c0       : vec4<f32>,   // transform column 0 (xyz)
  c1       : vec4<f32>,   // transform column 1 (xyz)
  c2       : vec4<f32>,   // transform column 2 (xyz)
  colorUp  : vec4<f32>,   // bytes 48..63
  colorDown: vec4<f32>,   // bytes 64..79
  wickHalf : vec4<f32>,   // bytes 80..95 (.x = wick half, .y = body half,
                          //              .z = MINIMUM body height; all clip)
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
    // ENC-1251 — MINIMUM BODY HEIGHT (the doji floor).
    //
    // body0 == body1 whenever open == close, and then both body triangles are
    // DEGENERATE: zero area, zero fragments, no ink at the open/close level at
    // all. That is not a corner case. In a 100 s wiretap of the live dataplane
    // (candles-v1, NEXO/lastPrice, 3 s tumbling windows) 17 of 146 records —
    // 11.6% — had open == close EXACTLY, and every one of them rendered as a
    // bare wick. That is the "several wicks carry no visible body" of SPEC
    // section 1.0, and it is a tier-0 failure: the record carries an open and a
    // close and the mark depicted neither.
    //
    // So the body's CLIP height is floored, exactly as ENC-1257 floors the
    // inter-bar gap and for the same reason — the reader's eye works in pixels,
    // and the rasteriser samples pixel CENTRES with no MSAA. The floor is a
    // floor: a body that already clears it is untouched, bit for bit.
    //
    // The end values are transformed first and the floor applied in CLIP space,
    // so it is a true pixel quantity under any transform and any zoom, rather
    // than a data-space epsilon that would mean something different on every
    // chart.
    let pb0 = m * vec3<f32>(cx, body0, 1.0);
    let pb1 = m * vec3<f32>(cx, body1, 1.0);
    var by0 = pb0.y;
    var by1 = pb1.y;
    let minH = u.wickHalf.z;              // clip units; 0 disables the rule
    if (minH > 0.0 && abs(by1 - by0) < minH) {
      // Grow about the body's midpoint, PRESERVING the quad's orientation (a
      // transform with a negative y scale hands these back in the other order,
      // and flipping the winding here would be a gratuitous difference).
      let mid = (by0 + by1) * 0.5;
      let halfH = select(-minH, minH, by1 >= by0) * 0.5;
      by0 = mid - halfH;
      by1 = mid + halfH;
      // Keep the mark's TOTAL extent exactly low..high: a doji sitting on its
      // own high would otherwise overhang the wick by a pixel and claim a
      // price the bar never traded at. Slide the floored body back inside the
      // wick whenever the wick has the room; never shrink it.
      let pw0 = m * vec3<f32>(cx, low, 1.0);
      let pw1 = m * vec3<f32>(cx, high, 1.0);
      let wlo = min(pw0.y, pw1.y);
      let whi = max(pw0.y, pw1.y);
      if ((whi - wlo) >= minH) {
        let blo = min(by0, by1);
        let bhi = max(by0, by1);
        let shift = max(0.0, wlo - blo) - max(0.0, bhi - whi);
        by0 = by0 + shift;
        by1 = by1 + shift;
      }
    }
    let by = mix(by0, by1, uv.y);

    let bodyHalf = u.wickHalf.y;
    if (bodyHalf > 0.0) {
      // ENC-1257: the host resolved the body half-width in CLIP space from the
      // bar pitch. Transform the candle's centre and offset along clip x, the
      // same construction the wick above uses. For the affine, shear-free
      // transforms charts actually carry this is identical to transforming
      // (cx±hw) directly; it differs only under an x/y shear, which no chart
      // transform has.
      clip = vec2<f32>(pb0.x + mix(-bodyHalf, bodyHalf, uv.x), by);
    } else {
      let x0 = cx - hw;
      let x1 = cx + hw;
      let p = m * vec3<f32>(mix(x0, x1, uv.x), mix(body0, body1, uv.y), 1.0);
      clip = vec2<f32>(p.x, by);
    }
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

// ENC-558: (re)gather + (re)upload gb's instance buffer from the geometry's
// CURRENT CpuBufferStore bytes. The previous device buffer (if any) is destroyed
// and replaced — the scratch gather / direct upload content can change
// arbitrarily as the stream grows, so a fresh upload is the simplest correct
// path. Records the source versions/sizes used so the caller can short-circuit
// an unchanged frame.
void DawnInstancedCandleBackend::buildGeoBuffers(GpuDevice& device,
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
  gb.pitchData = 0.0f;
  gb.nominalHalfData = 0.0f;

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
        gb.bufferCapacity = scratch.size();
        // ENC-1257: measure the pitch over the GATHERED set — with an index
        // buffer the drawn subset is what the reader sees, and its spacing can
        // differ from the source buffer's.
        gb.pitchData = barPitchFromRecords(scratch.data(), scratch.size(),
                                           kCandleStride, 0);
        gb.nominalHalfData = medianRecordField(scratch.data(), scratch.size(),
                                               kCandleStride, 20);
      }
    }
  } else if (vtx && vtxBytes > 0) {
    // Non-indexed: upload the candle6 records directly; one instance per
    // candle.
    gb.instanceBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
    gb.instanceCount = geo->vertexCount;
    gb.bufferCapacity = vtxBytes;
    // ENC-1257: sample only the records that are actually drawn. The CPU buffer
    // routinely runs ahead of vertexCount on the streaming path (the app bumps
    // the count on its own cadence), and a trailing zero-filled region would
    // otherwise poison the median.
    const std::size_t drawnBytes =
        std::min<std::size_t>(vtxBytes, static_cast<std::size_t>(geo->vertexCount) *
                                            kCandleStride);
    gb.pitchData = barPitchFromRecords(vtx, drawnBytes, kCandleStride, 0);
    gb.nominalHalfData = medianRecordField(vtx, drawnBytes, kCandleStride, 20);
  }
}

DawnInstancedCandleBackend::GeoBuffers&
DawnInstancedCandleBackend::ensureGeoBuffers(GpuDevice& device,
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

BackendStats DawnInstancedCandleBackend::renderDrawItem(GpuDevice& device,
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

  // ENC-1257 — resolve the body half-width from the bar pitch. xform column 0's
  // x row is the data->clip x scale; the pitch and the nominal half-width were
  // measured off the instance records when the buffer was last built.
  const CandleBodyResolution body = resolveCandleBodyClip(
      gb.pitchData, gb.nominalHalfData, xform[0], viewW);
  // 0 means "rule does not apply" — the shader then uses the per-instance
  // halfWidth, i.e. exactly the pre-ENC-1257 geometry.
  const float bodyHalfClip = body.apply ? body.halfWidthClip : 0.0f;

  // ENC-1257 — the wick is capped by the same rule. It is a FIXED pixel width
  // and therefore does not shrink with the pitch: at 500 bars across 1300px the
  // 2px wick alone consumes more than the pitch has to spare, so sizing the
  // body correctly and leaving the wick alone STILL fuses the bars. Above a ~5px
  // pitch the cap never binds and the wick is bit-for-bit what it was.
  float wickHalfClip = viewW > 0 ? 2.0f / static_cast<float>(viewW) : 0.0f;
  if (body.apply && body.maxMarkHalfClip < wickHalfClip) {
    wickHalfClip = body.maxMarkHalfClip;
  }

  // ENC-1251 — the minimum body HEIGHT, in clip units. Two device pixels, the
  // same count and the same justification as the wick width above: the target
  // has no MSAA and pixel centres are sampled, so a one-pixel-tall span that
  // happens to straddle a row boundary can rasterise into neither row. Below a
  // valid viewport the rule is off (0) and the geometry is exactly what it was.
  const float bodyMinHeightClip =
      viewH > 0 ? 2.0f * kMinBodyHeightPx / static_cast<float>(viewH) : 0.0f;

  UniformBinding uniforms[6];
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
  // ENC-1257 body half-width (clip space) at uniform float index 21 (byte 84) —
  // wickHalf.y. Shares the flat tail with lineAA's u_fringeEdge, which can never
  // coexist in this pipeline's WGSL struct (same argument the existing
  // wickHalf@20 vs aaWidth@20 overlap rests on).
  uniforms[4].kind = UniformBinding::Kind::Float;
  uniforms[4].name = "u_bodyHalf";
  uniforms[4].data = &bodyHalfClip;
  // ENC-1251 minimum body height (clip space) at uniform float index 22
  // (byte 88) — wickHalf.z.
  uniforms[5].kind = UniformBinding::Kind::Float;
  uniforms[5].name = "u_bodyMinH";
  uniforms[5].data = &bodyMinHeightClip;

  BindGroupDesc bgDesc;
  bgDesc.pipeline = pipeline_;
  bgDesc.vertexBuffers = &gb.instanceBuffer;
  bgDesc.vertexBufferCount = 1;
  bgDesc.indexBuffer = {};  // instanced draw: no GPU index buffer (gather is CPU)
  bgDesc.uniforms = uniforms;
  bgDesc.uniformCount = 6;

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
