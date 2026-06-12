// ENC-490 (P2.7) — Dawn/WebGPU backend for the `lineAA@1` pipeline
// (anti-aliased thick lines + D28.1 dash patterns).
//
// The Dawn mirror of Renderer::drawLineAA (kLineAAVert/kLineAAFrag in
// core/src/gl/Renderer.cpp). WebGPU has NO native line width, so a thick/AA line
// is drawn by QUAD-EXPANSION: each segment is expanded in the vertex shader into
// a screen-aligned quad of width (lineWidth + 2*aaWidth), and the fragment shader
// computes the AA coverage falloff and applies the dash on/off pattern. This
// lands on top of the shared `drawInstanced` foundation in DawnDevice (ENC-488)
// exactly like DawnInstancedRectBackend.
//
// BEHAVIOURAL PARITY with the GL drawLineAA path:
//   * geometry      — a unit quad (6 vertices, two triangles) generated in the
//                     vertex shader from @builtin(vertex_index) % 6; NO per-vertex
//                     buffer (matches the GL gl_VertexID % 6 quad).
//   * per-instance  — one rect4 attribute a_rect = vec4(x0,y0,x1,y1) at location
//                     0, VertexStepMode::Instance (GL divisor 1). The xy/zw hold
//                     the segment endpoints p0/p1 (NOT a rect min/max). Stride 16B
//                     (4-byte aligned, ENC-485).
//   * quad expansion — transform p0/p1 by the mat3, take the clip-space direction
//                     and its perpendicular, and offset the quad corners by
//                     perp * (uv.y * totalHalfWidth) where totalHalfWidth =
//                     lineWidth/2 + aaWidth. v_dist is the signed cross-line
//                     coordinate normalised to the nominal half-width (0 center,
//                     1 nominal edge, >1 in the AA fringe). v_along is the
//                     PIXEL-space distance along the segment for the dash phase.
//   * AA coverage   — fragment: a = 1 - smoothstep(1, fringeEdge, |v_dist|).
//   * dash (D28.1)  — fragment: when dashLen > 0, discard fragments whose
//                     mod(v_along, dashLen+gapLen) lies in the gap.
//   * D26 indexed gather — when the geometry carries an index/order buffer the
//                     visible segments are GATHERED CPU-side into a scratch
//                     per-instance buffer (mirrors the GL scratch-VBO gather).
//
// UNIFORMS (the 96-byte block; see DawnDevice::createBindGroup name-driven
// packing): mat3 transform + vec4 color + vec2 viewportSize + f32 cornerRadius
// (unused here, kept for slot parity) + f32 lineWidth + f32 aaWidth +
// f32 fringeEdge + f32 dashLen + f32 gapLen.
//
// WGSL NOTES (vs the GLSL):
//   * NDC Y-FLIP: clip.y is negated (same convention as triSolid/instancedRect)
//     so the WebGPU top-left framebuffer matches the GL bottom-left readback.
//     The perp is computed from the already-flipped clip positions so the quad
//     stays correctly oriented around the line.
//   * Default Normal alpha blend (applied by DawnDevice) composites the AA
//     coverage and dash edges correctly.
//
// SCOPE: lineAA@1 ONLY.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnLineAABackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "lineAA@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached lineAA render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers, created lazily on first draw and reused.
  // Non-indexed path uploads the geometry's rect4 segment bytes directly; the
  // indexed (D26) path uploads a CPU-gathered scratch buffer of only the
  // selected segments.
  //
  // ENC-569: like the instanced rect backend (ENC-558), we stamp the source
  // buffer versions this instance buffer was built from and re-gather/re-upload
  // (growing instanceCount from the current buffer size) on a CpuBufferStore
  // version bump, so a streaming/growing thick-line buffer animates.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};   // per-instance segment records (a_rect)
    std::uint32_t instanceCount{0};  // segments drawn (gathered count when indexed)
    std::uint64_t vtxVersion{0};     // CpuBufferStore version of vertexBufferId
    std::uint64_t idxVersion{0};     // CpuBufferStore version of indexBufferId
    bool built{false};               // false until first (re)build
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  void buildGeoBuffers(GpuDevice& device, const Scene& scene,
                       CpuBufferStore& gpu, std::uint32_t geometryId,
                       GeoBuffers& gb);

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
