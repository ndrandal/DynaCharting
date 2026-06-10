// ENC-486 (P2.3) — Dawn/WebGPU backend for the `line2d@1` pipeline.
//
// The Dawn mirror of the GL `line2d@1` path (Renderer::drawPos2 with GL_LINES).
// It implements the same IRendererBackend seam as DawnTriSolidBackend (ENC-484),
// registered under DeviceKind::Dawn for "line2d@1", so the registry/dispatcher
// can route a line2d DrawItem to either device kind.
//
// BEHAVIOURAL PARITY with the GL line2d path:
//   * vertex format  — pos2 (vec2 float), Float32x2 @ location 0.
//   * uniforms       — mat3 transform (DrawItem's transformId, or identity) +
//                      vec4 color (DrawItem.color). Same packed 64-byte uniform
//                      as triSolid; the pos2 WGSL module is reused verbatim.
//   * draw           — LineList; indexed (DrawElements) when the geometry has an
//                      index buffer, else arrays (DrawArrays).
//
// 1px LIMITATION — IMPORTANT:
//   WebGPU has NO line-width control: a LineList renders 1px-wide lines. The GL
//   path applies glLineWidth(di.lineWidth); that has NO native WebGPU equivalent.
//   For lineWidth > 1 the correct fix is quad-expansion (a thick-line geometry
//   pass), which is DEFERRED to lineAA (ENC-490). This backend therefore renders
//   basic 1px lines and ignores di.lineWidth — see TODO(ENC-490) in the .cpp.
//
// Everything else (uniform packing, NDC Y-flip in the WGSL, static geometry
// upload) is identical to DawnTriSolidBackend; the ONLY difference is the
// PrimitiveTopology (Lines) passed to DawnDevice::createPipeline.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnLine2dBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "line2d@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached line2d render pipeline (created once in init(); never rebuilt
  // per draw — same lifetime model as DawnTriSolidBackend).
  PipelineHandle pipeline_{};

  // Static per-geometry GPU buffers, created lazily on first draw of a geometry
  // and reused thereafter (matches triSolid's static-upload approach; live
  // streaming lands per-pipeline later). Keyed by geometryId.
  struct GeoBuffers {
    BufferHandle vertexBuffer{};
    BufferHandle indexBuffer{};
    std::uint32_t vertexCount{0};
    std::uint32_t indexCount{0};
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
