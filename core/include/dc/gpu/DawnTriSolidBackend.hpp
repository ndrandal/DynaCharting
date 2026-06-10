// ENC-484 (P2.1) — Dawn/WebGPU backend for the `triSolid@1` pipeline.
//
// This is the FIRST real WebGPU geometry render in the DynaCharting migration:
// the Dawn mirror of GlTriSolidBackend (ENC-483). It implements the same
// IRendererBackend seam, registered under DeviceKind::Dawn for "triSolid@1", so
// the registry/dispatcher can route a triSolid DrawItem to either device kind.
//
// BEHAVIOURAL PARITY with GlTriSolidBackend:
//   * vertex format  — pos2 (vec2 float), Float32x2 @ location 0.
//   * uniforms       — mat3 transform (DrawItem's transformId, or identity) +
//                      vec4 color (DrawItem.color). u_pointSize is unused by
//                      triSolid (triangles), so it is dropped from the WGSL.
//   * draw           — TriangleList; indexed (DrawElements) when the geometry
//                      has an index buffer, else arrays (DrawArrays).
//
// WGSL NOTES (the parts that differ from the GLSL pos2 shader):
//   * The uniform is packed as four vec4<f32> (three mat3 columns padded to
//     vec4 + the color) to sidestep WGSL's 16-byte mat3x3 column alignment —
//     the host fills a flat 64-byte struct with no implicit padding. The vertex
//     shader rebuilds mat3x3(c0.xyz, c1.xyz, c2.xyz). See DawnDevice::createBindGroup.
//   * NDC Y-FLIP: GL clip space is read back bottom-left origin (OSMesa); the
//     WebGPU offscreen framebuffer is top-left origin. We negate clip-space y in
//     the vertex shader so the rendered triangle lands in the same screen
//     position as the GL baseline for an equivalent readback. (clip.z=0, w=1.)
//
// SCOPE: triSolid@1 ONLY (no blend/clip variants — TODO(ENC-493); no streaming —
// the vertex/index buffers are uploaded statically here, TODO(ENC-485)). The
// remaining pipelines are ENC-486..492.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnTriSolidBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "triSolid@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              GpuBufferManager& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached triSolid render pipeline (created once in init(); never rebuilt
  // per draw — see GpuDevice / ENC-493 cache note).
  PipelineHandle pipeline_{};

  // Static per-geometry GPU buffers, created lazily on first draw of a geometry
  // and reused thereafter. ENC-484 keeps this minimal/static; the streaming +
  // GpuBufferManager-backed buffer model is TODO(ENC-485). Keyed by geometryId.
  struct GeoBuffers {
    BufferHandle vertexBuffer{};
    BufferHandle indexBuffer{};
    std::uint32_t vertexCount{0};
    std::uint32_t indexCount{0};
  };
  // Tiny inline map (geometry count is small in the first-render slice).
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               GpuBufferManager& gpu, std::uint32_t geometryId);
};

}  // namespace dc
