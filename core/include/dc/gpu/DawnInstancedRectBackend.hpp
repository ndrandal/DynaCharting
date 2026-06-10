// ENC-488 (P2.5) — Dawn/WebGPU backend for the `instancedRect@1` pipeline.
//
// The Dawn mirror of Renderer::drawInstancedRect (kInstRectVert/kInstRectFrag in
// core/src/gl/Renderer.cpp). This is the FIRST instanced pipeline on Dawn, so it
// lands on top of the shared `drawInstanced` foundation in DawnDevice
// (per-instance step-mode vertex buffers + the instanced draw) that the
// remaining instanced pipelines (ENC-489 candle / ENC-490 lineAA / ENC-491
// textured) reuse.
//
// BEHAVIOURAL PARITY with the GL drawInstancedRect path:
//   * geometry      — a unit quad (6 vertices, two triangles) generated in the
//                     vertex shader from @builtin(vertex_index) % 6; NO per-vertex
//                     buffer (matches the GL gl_VertexID % 6 quad).
//   * per-instance  — one rect4 attribute a_rect = vec4(x0,y0,x1,y1) at location
//                     0, VertexStepMode::Instance (GL divisor 1). Stride 16B
//                     (4-byte aligned, ENC-485).
//   * uniforms      — mat3 transform + vec4 color + vec2 viewportSize +
//                     f32 cornerRadius (the 80-byte uniform block; see
//                     DawnDevice::createBindGroup name-driven packing).
//   * rounded corner SDF (D28.2) — reproduced verbatim in the fragment shader:
//                     compute the rect half-size in pixels in the VS, then a
//                     rounded-box signed-distance with smoothstep AA in the FS.
//   * D26 indexed gather — when the geometry carries an index/order buffer, the
//                     visible instances are GATHERED CPU-side into a scratch
//                     per-instance buffer (the rect4 records the indices select),
//                     and only that subset is drawn. Mirrors the GL scratch-VBO
//                     gather.
//
// WGSL NOTES (vs the GLSL):
//   * NDC Y-FLIP: clip.y is negated (same convention as triSolid/triGradient) so
//     the WebGPU top-left framebuffer matches the GL bottom-left readback.
//   * Uniform packing: see DawnDevice::createBindGroup — the host fills a flat
//     80-byte block (three mat3 columns padded to vec4 + color vec4 + viewport
//     vec2 + cornerRadius f32 + pad).
//
// SCOPE: instancedRect@1 ONLY (no candle/lineAA/textured — those are
// ENC-489/490/491, which reuse DawnDevice::drawInstanced + this layout pattern).
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnInstancedRectBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "instancedRect@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached instancedRect render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers, created lazily on first draw and reused.
  // The non-indexed path uploads the geometry's rect4 vertex bytes directly;
  // the indexed (D26) path uploads a CPU-gathered scratch buffer of only the
  // selected instances and re-uploads it when the index set changes.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};   // per-instance rect4 records (a_rect)
    std::uint32_t instanceCount{0};  // rects drawn (gathered count when indexed)
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
