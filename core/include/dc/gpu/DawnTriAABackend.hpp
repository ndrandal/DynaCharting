// ENC-487 (P2.4) — Dawn/WebGPU backend for the `triAA@1` pipeline.
//
// The Dawn mirror of GlTriAA's inline draw path (Renderer::drawTriAA / the
// kTriAAVert+kTriAAFrag GLSL). triAA renders AA-fringe triangles whose edge
// vertices carry a per-vertex alpha (0 at the feather edge, 1 in the body), and
// shades them as u_color.rgb with alpha = u_color.a * v_alpha. It implements the
// same IRendererBackend seam as DawnTriSolidBackend, registered under
// DeviceKind::Dawn for "triAA@1".
//
// BEHAVIOURAL PARITY with the GL triAA path (Renderer.cpp):
//   * vertex format  — pos2 + alpha, packed as a single 12-byte vertex
//                      (GL: one vec3 `a_pos_alpha`, stride 12). On Dawn we feed
//                      it as TWO attributes off the SAME buffer: Float32x2 pos at
//                      offset 0 (location 0) + Float32 alpha at offset 8
//                      (location 1). Same bytes, same stride — just split so the
//                      WGSL declares pos:vec2 and alpha:f32 cleanly.
//   * uniforms       — mat3 transform (DrawItem's transformId, or identity) +
//                      vec4 color (DrawItem.color). Reuses DawnDevice's 64-byte
//                      mat3+color uniform layout (same as triSolid).
//   * fragment       — vec4(u_color.rgb, u_color.a * v_alpha).
//   * draw           — TriangleList; indexed when the geometry has an index
//                      buffer, else arrays. NDC y-flip (same as triSolid) so the
//                      WebGPU top-left framebuffer matches the GL readback.
//
// SCOPE: triAA@1 ONLY. Static vertex/index upload (no streaming), mirroring
// DawnTriSolidBackend. Blend state: triAA's alpha-fringe shading writes a
// premultiplied-looking value into RGBA8; the offscreen target is opaque so the
// interior (v_alpha == 1) reads back as the solid fill — see the test.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnTriAABackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "triAA@1"; }

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
