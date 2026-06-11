// ENC-489 (P2.6) — Dawn/WebGPU backend for the `instancedCandle@1` pipeline.
//
// The Dawn mirror of Renderer::drawInstancedCandle (kInstCandleVert/
// kInstCandleFrag in core/src/gl/Renderer.cpp). It lands on top of the shared
// drawInstanced foundation (ENC-488: per-instance step-mode vertex buffers + the
// instanced draw) and follows the DawnInstancedRectBackend template.
//
// BEHAVIOURAL PARITY with the GL drawInstancedCandle path:
//   * geometry      — 12 vertices per instance (6 for the OHLC body quad +
//                     6 for the wick quad), generated in the vertex shader from
//                     @builtin(vertex_index) % 12; NO per-vertex buffer (matches
//                     the GL gl_VertexID % 12).
//   * per-instance  — one candle6 record split into two attributes:
//                       a_c0 = vec4(cx, open, high, low)  @ location 0
//                       a_c1 = vec2(close, halfWidth)     @ location 1
//                     VertexStepMode::Instance (GL divisor 1). Stride 24B (the
//                     Candle6 format: 6 floats), matching the GL attribute split.
//   * body          — a quad from (cx-hw, min(open,close)) to
//                     (cx+hw, max(open,close)), transformed by the mat3.
//   * wick          — a FIXED-PIXEL-WIDTH vertical line from low to high at cx:
//                     the center is transformed by the mat3 then offset by
//                     ±1px in clip space (1/viewport.x half-width), exactly like
//                     the GL kInstCandleVert wick branch.
//   * color         — up vs down per instance: close >= open selects u_colorUp,
//                     else u_colorDown (the GL v_isUp flat varying).
//   * D26 indexed gather — when the geometry carries an index buffer, the visible
//                     instances are GATHERED CPU-side into a scratch per-instance
//                     buffer; only that subset is drawn (mirrors the GL scratch
//                     VBO gather, same as DawnInstancedRectBackend).
//
// WGSL / UNIFORM NOTES (vs the GLSL):
//   * NDC Y-FLIP: clip.y is negated (same convention as triSolid/instancedRect)
//     so the WebGPU top-left framebuffer matches the GL bottom-left readback.
//   * Two colors: this pipeline needs BOTH up and down colors. u_colorUp reuses
//     the existing Vec4 color slot (bytes 48..63); u_colorDown is packed into the
//     tail of the 80-byte block (bytes 64..79) — see the name-driven `u_colorDown`
//     case added to DawnDevice::createBindGroup. The 80-byte uniform block is
//     declared via PipelineDesc::uniformBytes = 80, as for instancedRect.
//
// SCOPE: instancedCandle@1 ONLY (lineAA / textured are ENC-490 / ENC-491, which
// reuse the same drawInstanced + per-instance layout pattern).
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnInstancedCandleBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "instancedCandle@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached instancedCandle render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers, created lazily on first draw and reused. The
  // non-indexed path uploads the geometry's candle6 vertex bytes directly; the
  // indexed (D26) path uploads a CPU-gathered scratch buffer of only the selected
  // instances.
  //
  // ENC-558: we also remember the CpuBufferStore versions (+ byte sizes) of the
  // source vertex/index buffers this GPU buffer was last built from. On each
  // render ensureGeoBuffers() re-checks them; if the streaming ingest grew or
  // edited the CPU buffer (version bump / size change), we re-gather and
  // re-upload so live data keeps rendering past the first frame. Unchanged
  // geometry stays a pure cache hit (no per-frame re-upload).
  struct GeoBuffers {
    BufferHandle instanceBuffer{};   // per-instance candle6 records (a_c0/a_c1)
    std::uint32_t instanceCount{0};  // candles drawn (gathered count when indexed)
    std::size_t bufferCapacity{0};   // byte size of instanceBuffer
    std::uint64_t vtxVersion{0};     // CpuBufferStore version of vertexBufferId
    std::uint64_t idxVersion{0};     // CpuBufferStore version of indexBufferId
    bool built{false};               // false until first successful (re)build
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  // (Re)gather + (re)upload gb's instance buffer from the geometry's current CPU
  // bytes. Grows/recreates the device buffer when needed. Records the source
  // versions/sizes so a subsequent unchanged frame is a no-op.
  void buildGeoBuffers(GpuDevice& device, const Scene& scene,
                       CpuBufferStore& gpu, std::uint32_t geometryId,
                       GeoBuffers& gb);

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
