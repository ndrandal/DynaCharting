// ENC-487 (P2.4) — Dawn/WebGPU backend for the `triGradient@1` pipeline.
//
// The Dawn mirror of the GL triGradient path (Renderer::drawTriGradient / the
// kTriGradientVert+kTriGradientFrag GLSL, D28.3). triGradient renders triangles
// with a per-vertex RGBA color that the rasterizer interpolates across the face
// (the classic "gradient fill"). It implements the same IRendererBackend seam as
// DawnTriSolidBackend, registered under DeviceKind::Dawn for "triGradient@1".
//
// BEHAVIOURAL PARITY with the GL triGradient path (Renderer.cpp):
//   * vertex format  — pos2_color4: Float32x2 pos at offset 0 (location 0) +
//                      Float32x4 color at offset 8 (location 1), stride 24
//                      (strideOf(VertexFormat::Pos2Color4)). Two attributes off
//                      one buffer.
//   * uniforms       — mat3 transform only (DrawItem's transformId, or identity).
//                      No color uniform: the color comes per-vertex. We still
//                      reuse DawnDevice's 64-byte mat3+color uniform layout; the
//                      color slot is left zero (the fragment shader ignores it).
//   * fragment       — the interpolated per-vertex color, unmodified.
//   * draw           — TriangleList; indexed when the geometry has an index
//                      buffer, else arrays. NDC y-flip (same as triSolid) so the
//                      WebGPU top-left framebuffer matches the GL readback.
//
// SCOPE: triGradient@1 ONLY. Static vertex/index upload (no streaming),
// mirroring DawnTriSolidBackend.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnTriGradientBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "triGradient@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  PipelineHandle pipeline_{};

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
