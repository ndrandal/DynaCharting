// ENC-495 (P3.3) — DawnPickBackend implementation. See header.
//
// Renders each pickable DrawItem's id (encoded RGB) into the offscreen pick
// target and reads back the probed pixel to decode the id. The pick pipelines
// reuse the visible backends' vertex GEOMETRY but a FLAT-id fragment color, so
// the rendered pick buffer is a map of pixel -> DrawItem id (the GL D29.3 design).
#include "dc/gpu/DawnPickBackend.hpp"

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

// True for the non-instanced "flat" pick pipelines (pos2 geometry, drawn as
// triangles/lines/points but always with the id color). Mirrors GL drawPick's
// first branch.
bool isFlatPipeline(const std::string& p) {
  return p == "triSolid@1" || p == "line2d@1" || p == "points@1" ||
         p == "triAA@1" || p == "triGradient@1";
}

// ---- Pick WGSL shaders (flat id color; mirror the GL kPick* shaders) --------
//
// All return u.color (the encoded id at float index 12 of the shared uniform
// block — see DawnDevice::createBindGroup). Y is negated to match the other Dawn
// backends' GL-parity orientation. No AA / gradient / rounding / candle split:
// pick is a solid id-color fill of the geometry.

// pickFlat: tight pos2 vertices (the gather repacks any source stride to 8B), so
// the pipeline's vertex layout is a single Float32x2 @ location 0, stride 8.
const char* kPickFlatWgsl = R"WGSL(
struct U { c0:vec4<f32>, c1:vec4<f32>, c2:vec4<f32>, color:vec4<f32>, };
@group(0) @binding(0) var<uniform> u : U;
@vertex
fn vs_main(@location(0) a_pos : vec2<f32>) -> @builtin(position) vec4<f32> {
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(a_pos, 1.0);
  return vec4<f32>(p.x, -p.y, 0.0, 1.0);
}
@fragment
fn fs_main() -> @location(0) vec4<f32> { return u.color; }
)WGSL";

// pickInstRect: per-instance rect4 (x0,y0,x1,y1), unit quad from vertex_index%6.
// Used for instancedRect@1 and texturedQuad@1 (whose Pos2Uv4 record's first 4
// floats are the quad min/max, same as GL's pick reuse).
const char* kPickInstRectWgsl = R"WGSL(
struct U { c0:vec4<f32>, c1:vec4<f32>, c2:vec4<f32>, color:vec4<f32>, };
@group(0) @binding(0) var<uniform> u : U;
@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_rect : vec4<f32>) -> @builtin(position) vec4<f32> {
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
  return vec4<f32>(p.x, -p.y, 0.0, 1.0);
}
@fragment
fn fs_main() -> @location(0) vec4<f32> { return u.color; }
)WGSL";

// pickInstCandle: per-instance candle6, body+wick (12 verts). Mirrors GL
// kPickInstCandleVert (fixed-pixel wick via u_wickHalf clip half-width).
const char* kPickInstCandleWgsl = R"WGSL(
struct U {
  c0:vec4<f32>, c1:vec4<f32>, c2:vec4<f32>,
  color:vec4<f32>,     // float 12 (the id color)
  _colorDown:vec4<f32>,// float 16 (unused for pick)
  wickHalf:vec4<f32>,  // float 20 (.x = wick half-width, clip space)
};
@group(0) @binding(0) var<uniform> u : U;
@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_c0 : vec4<f32>,
           @location(1) a_c1 : vec2<f32>) -> @builtin(position) vec4<f32> {
  let cx=a_c0.x; let open=a_c0.y; let high=a_c0.z; let low=a_c0.w;
  let close=a_c1.x; let hw=a_c1.y;
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
    let y = mix(low, high, uv.y);
    let center = m * vec3<f32>(cx, y, 1.0);
    let hwc = u.wickHalf.x;
    clip = vec2<f32>(center.x + mix(-hwc, hwc, uv.x), center.y);
  } else {
    let p = m * vec3<f32>(mix(cx - hw, cx + hw, uv.x), mix(body0, body1, uv.y), 1.0);
    clip = p.xy;
  }
  return vec4<f32>(clip.x, -clip.y, 0.0, 1.0);
}
@fragment
fn fs_main() -> @location(0) vec4<f32> { return u.color; }
)WGSL";

// pickLineAA: per-instance segment (x0,y0,x1,y1) expanded to a thick quad.
// Mirrors GL kPickLineAAVert (line width + AA fringe in clip units).
const char* kPickLineAAWgsl = R"WGSL(
struct U {
  c0:vec4<f32>, c1:vec4<f32>, c2:vec4<f32>,
  color:vec4<f32>,         // float 12 (the id color)
  viewport:vec2<f32>,      // float 16 (unused, parity)
  _pad0:f32,               // float 18
  lineWidth:f32,           // float 19 (clip units)
  aaWidth:f32,             // float 20 (clip units)
};
@group(0) @binding(0) var<uniform> u : U;
@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @location(0) a_rect : vec4<f32>) -> @builtin(position) vec4<f32> {
  let p0 = a_rect.xy;
  let p1 = a_rect.zw;
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let c0 = (m * vec3<f32>(p0, 1.0)).xy;
  let c1 = (m * vec3<f32>(p1, 1.0)).xy;
  let dir = c1 - c0;
  let len = length(dir);
  let d = select(vec2<f32>(1.0, 0.0), dir / len, len > 0.0001);
  let perp = vec2<f32>(-d.y, d.x);
  let totalHW = u.lineWidth * 0.5 + u.aaWidth;
  let v = vid % 6u;
  var uv : vec2<f32>;
  if (v == 0u)      { uv = vec2<f32>(0.0, -1.0); }
  else if (v == 1u) { uv = vec2<f32>(1.0, -1.0); }
  else if (v == 2u) { uv = vec2<f32>(0.0,  1.0); }
  else if (v == 3u) { uv = vec2<f32>(0.0,  1.0); }
  else if (v == 4u) { uv = vec2<f32>(1.0, -1.0); }
  else              { uv = vec2<f32>(1.0,  1.0); }
  let pos = mix(c0, c1, uv.x) + perp * (uv.y * totalHW);
  // c0/c1 already y-up clip; negate y once for the framebuffer orientation.
  return vec4<f32>(pos.x, -pos.y, 0.0, 1.0);
}
@fragment
fn fs_main() -> @location(0) vec4<f32> { return u.color; }
)WGSL";

PipelineHandle makeFlatPipeline(GpuDevice& device, const char* wgsl,
                                const char* name) {
  VertexAttribute attr;
  attr.location = 0;
  attr.componentCount = 2;
  attr.type = VertexComponentType::Float32;
  attr.offsetBytes = 0;
  VertexBufferLayout layout;
  layout.strideBytes = 8;  // tight pos2 (gather repacks to this)
  layout.stepInstance = false;
  layout.attributes = &attr;
  layout.attributeCount = 1;
  PipelineDesc desc;
  desc.debugName = name;
  desc.vertexSource = wgsl;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  return device.createPipeline(desc);
}

PipelineHandle makeRect4Pipeline(GpuDevice& device, const char* wgsl,
                                 const char* name, std::size_t uniformBytes) {
  VertexAttribute attr;
  attr.location = 0;
  attr.componentCount = 4;
  attr.type = VertexComponentType::Float32;
  attr.offsetBytes = 0;
  VertexBufferLayout layout;
  layout.strideBytes = 16;  // rect4 / first 16B of pos2uv4
  layout.stepInstance = true;
  layout.attributes = &attr;
  layout.attributeCount = 1;
  PipelineDesc desc;
  desc.debugName = name;
  desc.vertexSource = wgsl;
  desc.vertexBuffers = &layout;
  desc.vertexBufferCount = 1;
  desc.topology = PrimitiveTopology::Triangles;
  desc.blend = DeviceBlendMode::Normal;
  desc.clip = ClipMode::None;
  desc.uniformBytes = uniformBytes;
  return device.createPipeline(desc);
}

}  // namespace

bool DawnPickBackend::init(GpuDevice& device) {
  pickFlat_ = makeFlatPipeline(device, kPickFlatWgsl, "pickFlat");

  pickInstRect_ = makeRect4Pipeline(device, kPickInstRectWgsl, "pickInstRect",
                                    /*uniformBytes*/ 64);

  // pickLineAA: rect4-strided instance (segment endpoints); 96-byte uniform
  // block (reuses the lineAA tail layout for lineWidth/aaWidth).
  pickLineAA_ = makeRect4Pipeline(device, kPickLineAAWgsl, "pickLineAA",
                                  /*uniformBytes*/ 96);

  // pickInstCandle: two per-instance attributes (a_c0 vec4 @0, a_c1 vec2 @16),
  // candle6 stride 24, 96-byte uniform block.
  {
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
    layout.strideBytes = 24;  // candle6
    layout.stepInstance = true;
    layout.attributes = attrs;
    layout.attributeCount = 2;
    PipelineDesc desc;
    desc.debugName = "pickInstCandle";
    desc.vertexSource = kPickInstCandleWgsl;
    desc.vertexBuffers = &layout;
    desc.vertexBufferCount = 1;
    desc.topology = PrimitiveTopology::Triangles;
    desc.blend = DeviceBlendMode::Normal;
    desc.clip = ClipMode::None;
    desc.uniformBytes = 96;
    pickInstCandle_ = device.createPipeline(desc);
  }

  return pickFlat_.valid() && pickInstRect_.valid() &&
         pickInstCandle_.valid() && pickLineAA_.valid();
}

DawnPickBackend::GeoBuffers& DawnPickBackend::ensureGeoBuffers(
    GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
    const DrawItem& di) {
  const Id geometryId = di.geometryId;
  for (auto& kv : geoBuffers_) {
    if (kv.first == geometryId) return kv.second;
  }

  GeoBuffers gb;
  const Geometry* geo = scene.getGeometry(geometryId);
  if (geo) {
    const std::uint8_t* vtx = gpu.getCpuData(geo->vertexBufferId);
    const std::uint32_t vtxBytes = gpu.getCpuDataSize(geo->vertexBufferId);
    const std::uint8_t* idx =
        (geo->indexBufferId != 0) ? gpu.getCpuData(geo->indexBufferId) : nullptr;
    const bool indexed =
        (geo->indexBufferId != 0 && geo->indexCount > 0 && idx != nullptr);

    if (isFlatPipeline(di.pipeline)) {
      // Repack the source vertices into a TIGHT pos2 (8B) buffer so the single
      // pickFlat pipeline (fixed 8B stride) handles every source stride
      // (triSolid 8, triAA 12, triGradient 24). Indexed draws are expanded into
      // a flat vertex list here (no GPU index buffer needed for pick).
      const std::uint32_t srcStride = strideOf(geo->format);
      std::vector<float> pos;
      if (vtx && srcStride >= 8) {
        auto pushVertex = [&](std::uint32_t vi) {
          const std::uint32_t off = vi * srcStride;
          if (off + 8 <= vtxBytes) {
            const float* fp = reinterpret_cast<const float*>(vtx + off);
            pos.push_back(fp[0]);
            pos.push_back(fp[1]);
          }
        };
        if (indexed) {
          const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
          for (std::uint32_t i = 0; i < geo->indexCount; ++i) pushVertex(indices[i]);
        } else {
          for (std::uint32_t i = 0; i < geo->vertexCount; ++i) pushVertex(i);
        }
      }
      if (!pos.empty()) {
        const std::size_t bytes = pos.size() * sizeof(float);
        gb.vertexBuffer = device.createBuffer(bytes, pos.data(), bytes);
        gb.vertexCount = static_cast<std::uint32_t>(pos.size() / 2);
      }
    } else {
      // Instanced pick (instancedRect / texturedQuad / instancedCandle / lineAA).
      // Gather the per-instance records, honouring the D26 index buffer (the
      // index holds u32 instance indices into the record buffer), exactly like
      // the visible backends.
      std::uint32_t recStride = 16;  // rect4 / pos2uv4 first 16B / lineAA segment
      if (di.pipeline == "instancedCandle@1") recStride = 24;  // candle6

      if (indexed) {
        const std::uint32_t count = geo->indexCount;
        const auto* indices = reinterpret_cast<const std::uint32_t*>(idx);
        std::vector<std::uint8_t> scratch(
            static_cast<std::size_t>(count) * recStride, 0);
        if (vtx) {
          for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t off = indices[i] * recStride;
            if (off + recStride <= vtxBytes) {
              std::memcpy(scratch.data() + static_cast<std::size_t>(i) * recStride,
                          vtx + off, recStride);
            }
          }
        }
        if (!scratch.empty()) {
          gb.vertexBuffer =
              device.createBuffer(scratch.size(), scratch.data(), scratch.size());
          gb.instanceCount = count;
        }
      } else if (vtx && vtxBytes >= recStride) {
        // Non-indexed: the record buffer is already tightly packed. texturedQuad
        // uses Pos2Uv4 (16B) whose first 16B == the quad min/max, so a direct
        // upload + 16B instance stride reads the rect correctly.
        gb.vertexBuffer = device.createBuffer(vtxBytes, vtx, vtxBytes);
        gb.instanceCount = geo->vertexCount;
      }
    }
  }

  geoBuffers_.emplace_back(geometryId, gb);
  return geoBuffers_.back().second;
}

void DawnPickBackend::drawPickItem(GpuDevice& device, const Scene& scene,
                                   CpuBufferStore& gpu, const DrawItem& di,
                                   int viewW, int viewH, const float idColor[4]) {
  const Geometry* geo = scene.getGeometry(di.geometryId);
  if (!geo) return;

  GeoBuffers& gb = ensureGeoBuffers(device, scene, gpu, di);
  const float* xform = resolveTransform(di, scene);

  if (isFlatPipeline(di.pipeline)) {
    if (!gb.vertexBuffer.valid() || gb.vertexCount == 0) return;
    UniformBinding uniforms[2];
    uniforms[0].kind = UniformBinding::Kind::Mat3;
    uniforms[0].name = "u_transform";
    uniforms[0].data = xform;
    uniforms[1].kind = UniformBinding::Kind::Vec4;
    uniforms[1].name = "u_color";
    uniforms[1].data = idColor;

    BindGroupDesc bg;
    bg.pipeline = pickFlat_;
    bg.vertexBuffers = &gb.vertexBuffer;
    bg.vertexBufferCount = 1;
    bg.uniforms = uniforms;
    bg.uniformCount = 2;
    BindGroupHandle group = device.createBindGroup(bg);
    if (!group.valid()) return;
    device.bindPipeline(pickFlat_);
    DrawParams params;
    params.vertexCount = gb.vertexCount;
    device.draw(group, params);
    return;
  }

  if (di.pipeline == "instancedRect@1" || di.pipeline == "texturedQuad@1") {
    if (!gb.vertexBuffer.valid() || gb.instanceCount == 0) return;
    UniformBinding uniforms[2];
    uniforms[0].kind = UniformBinding::Kind::Mat3;
    uniforms[0].name = "u_transform";
    uniforms[0].data = xform;
    uniforms[1].kind = UniformBinding::Kind::Vec4;
    uniforms[1].name = "u_color";
    uniforms[1].data = idColor;

    BindGroupDesc bg;
    bg.pipeline = pickInstRect_;
    bg.vertexBuffers = &gb.vertexBuffer;
    bg.vertexBufferCount = 1;
    bg.uniforms = uniforms;
    bg.uniformCount = 2;
    BindGroupHandle group = device.createBindGroup(bg);
    if (!group.valid()) return;
    device.bindPipeline(pickInstRect_);
    DrawInstancedParams params;
    params.vertexCountPerInstance = 6;
    params.instanceCount = gb.instanceCount;
    device.drawInstanced(group, params);
    return;
  }

  if (di.pipeline == "instancedCandle@1") {
    if (!gb.vertexBuffer.valid() || gb.instanceCount == 0) return;
    const float wickHalfClip =
        viewW > 0 ? 2.0f / static_cast<float>(viewW) : 0.0f;
    UniformBinding uniforms[3];
    uniforms[0].kind = UniformBinding::Kind::Mat3;
    uniforms[0].name = "u_transform";
    uniforms[0].data = xform;
    uniforms[1].kind = UniformBinding::Kind::Vec4;
    uniforms[1].name = "u_color";  // float 12 -> the id color
    uniforms[1].data = idColor;
    uniforms[2].kind = UniformBinding::Kind::Float;
    uniforms[2].name = "u_wickHalf";
    uniforms[2].data = &wickHalfClip;

    BindGroupDesc bg;
    bg.pipeline = pickInstCandle_;
    bg.vertexBuffers = &gb.vertexBuffer;
    bg.vertexBufferCount = 1;
    bg.uniforms = uniforms;
    bg.uniformCount = 3;
    BindGroupHandle group = device.createBindGroup(bg);
    if (!group.valid()) return;
    device.bindPipeline(pickInstCandle_);
    DrawInstancedParams params;
    params.vertexCountPerInstance = 12;
    params.instanceCount = gb.instanceCount;
    device.drawInstanced(group, params);
    return;
  }

  if (di.pipeline == "lineAA@1") {
    if (!gb.vertexBuffer.valid() || gb.instanceCount == 0) return;
    const float lineWidthClip =
        (viewW > 0) ? (di.lineWidth / static_cast<float>(viewW) * 2.0f) : 0.01f;
    const float aaWidthClip =
        (viewW > 0) ? (1.5f / static_cast<float>(viewW) * 2.0f) : 0.005f;
    const float viewport[2] = {static_cast<float>(viewW),
                               static_cast<float>(viewH)};
    UniformBinding uniforms[4];
    uniforms[0].kind = UniformBinding::Kind::Mat3;
    uniforms[0].name = "u_transform";
    uniforms[0].data = xform;
    uniforms[1].kind = UniformBinding::Kind::Vec4;
    uniforms[1].name = "u_color";
    uniforms[1].data = idColor;
    uniforms[2].kind = UniformBinding::Kind::Vec2;
    uniforms[2].name = "u_viewportSize";
    uniforms[2].data = viewport;
    uniforms[3].kind = UniformBinding::Kind::Float;
    uniforms[3].name = "u_lineWidth";
    uniforms[3].data = &lineWidthClip;
    // aaWidth shares float index 20 (u_aaWidth) in the shared block.
    UniformBinding aa;
    aa.kind = UniformBinding::Kind::Float;
    aa.name = "u_aaWidth";
    aa.data = &aaWidthClip;
    UniformBinding all[5] = {uniforms[0], uniforms[1], uniforms[2], uniforms[3], aa};

    BindGroupDesc bg;
    bg.pipeline = pickLineAA_;
    bg.vertexBuffers = &gb.vertexBuffer;
    bg.vertexBufferCount = 1;
    bg.uniforms = all;
    bg.uniformCount = 5;
    BindGroupHandle group = device.createBindGroup(bg);
    if (!group.valid()) return;
    device.bindPipeline(pickLineAA_);
    DrawInstancedParams params;
    params.vertexCountPerInstance = 6;
    params.instanceCount = gb.instanceCount;
    device.drawInstanced(group, params);
    return;
  }

  // textSDF@1 and anything else: not pickable (matches GL).
}

DawnPickResult DawnPickBackend::renderPick(GpuDevice& device, const Scene& scene,
                                           CpuBufferStore& gpu, int viewW,
                                           int viewH, int pickX, int pickY,
                                           EventBus* bus) {
  DawnPickResult result;

  // Dedicated pass into the pick target (id 1), cleared to transparent black so
  // every unhit pixel decodes to id 0. No stencil clip in the pick pass.
  RenderPassDesc pass;
  pass.target.id = kPickTargetId;
  pass.viewportWidth = static_cast<std::uint32_t>(viewW);
  pass.viewportHeight = static_cast<std::uint32_t>(viewH);
  pass.clear = true;
  pass.clearColor[0] = 0.0f;
  pass.clearColor[1] = 0.0f;
  pass.clearColor[2] = 0.0f;
  pass.clearColor[3] = 0.0f;
  pass.clearStencil = true;
  device.beginRenderPass(pass);

  // Pick writes opaque id colors and never clips: force the (Normal, None)
  // pipeline variant regardless of any blend/clip state a prior pass left set,
  // so bindPipeline selects the base pick pipeline.
  device.setBlendMode(DeviceBlendMode::Normal);
  device.setClipState(ClipMode::None);

  // Walk panes -> layers -> draw items in scene order (later draws win, matching
  // GL's topmost-wins overlap semantics — the last id written under the pixel
  // is what reads back).
  for (Id paneId : scene.paneIds()) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;
    for (Id layerId : scene.layerIds()) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;
      for (Id diId : scene.drawItemIds()) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (di->pipeline.empty() || !di->visible) continue;
        if (di->isClipSource) continue;  // clip sources write no color (GL parity)

        // Encode the id as R8G8B8 in [0,1] (the GL D29.3 encoding).
        const float idColor[4] = {
            static_cast<float>(di->id & 0xFF) / 255.0f,
            static_cast<float>((di->id >> 8) & 0xFF) / 255.0f,
            static_cast<float>((di->id >> 16) & 0xFF) / 255.0f,
            1.0f,
        };
        drawPickItem(device, scene, gpu, *di, viewW, viewH, idColor);
      }
    }
  }

  device.endRenderPass();

  // Read back the probed pixel from the pick target and decode the id.
  if (pickX >= 0 && pickX < viewW && pickY >= 0 && pickY < viewH) {
    std::uint8_t px[4] = {0, 0, 0, 0};
    device.readPixel(pickX, pickY, px);
    const std::uint32_t id = static_cast<std::uint32_t>(px[0]) |
                             (static_cast<std::uint32_t>(px[1]) << 8) |
                             (static_cast<std::uint32_t>(px[2]) << 16);
    result.drawItemId = id;

    // ENC-627 (C1): resolve the durable source row id from the side table once a
    // per-instance index is known. `instanceIndex` stays -1 until the ENC-628
    // shader change supplies it, so rowId is -1 here (DrawItem-level pick only) —
    // the wiring is in place for C2 to flip on with no further plumbing.
    result.rowId = instanceTable_.rowIdForInstance(id, result.instanceIndex);

    if (id != 0 && bus) {
      EventData ev;
      ev.type = EventType::GeometryClicked;
      ev.targetId = id;
      ev.payload[0] = static_cast<double>(pickX);
      ev.payload[1] = static_cast<double>(pickY);
      bus->emit(ev);
    }
  }

  return result;
}

}  // namespace dc
