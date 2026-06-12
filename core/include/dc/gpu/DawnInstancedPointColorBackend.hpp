// ENC-609 (P2.2) — Dawn/WebGPU backend for the `instancedPointColor@1` pipeline:
// the per-POINT color + size scatter mark.
//
// WHY THIS EXISTS (RESEARCH §4.3)
// -------------------------------
// `points@1` renders 1px points with ONE uniform color per draw item (and no
// size control — WebGPU PointList is always 1px). A scatter where each point has
// its OWN color (a category / sequential-scale output) and its OWN size (a size
// channel) cannot be expressed there. This is the per-POINT sibling of the ENC-608
// keystone rect: instead of a 1px PointList, each point expands to an instanced
// quad (6 verts from @builtin(vertex_index) % 6) centered on the point, sized in
// PIXELS (so the dot scales with the viewport, not the affine transform), and
// filled from a per-instance packed RGBA8 attribute.
//
// THE FORMAT — Point4Color (16B), modeled on Rect4Color
// -----------------------------------------------------
//   offset  0 : vec2 f32  position (x,y)  — Float32x2 @ location 0
//   offset  8 : RGBA8      color           — Unorm8x4  @ location 1
//   offset 12 : f32        size (diameter, — Float32   @ location 2
//               pixels)
// Position rides the mat3 transform (data->clip) like every mark; the half-extent
// quad is then offset in pixels (size/2) via the viewport, so a point of size N
// covers an N-pixel square regardless of zoom.
//
// THE RESERVED ROW-ID LANE (ENC-601 picking note)
// -----------------------------------------------
// Like Rect4Color, the size lane doubles as the room for a future per-instance
// picking path: today the shader reads size; ENC-594's row id would ride a
// parallel lane (the encode pass carries instanceRowIds out-of-band, exactly as
// for the rect). No new format is needed when picking lands.
//
// RELATION to DawnInstancedRectColorBackend (the model)
// -----------------------------------------------------
// Same unit-quad expansion, same per-instance packed RGBA8 color, same D26
// indexed-gather + ENC-558 streaming-grow caching, same 80-byte uniform block
// (mat3 + viewport + an unused cornerRadius slot). The ONLY differences: the
// instance record is Point4Color (16B), the quad is built from a center + a
// PIXEL half-extent (size/2 mapped through the viewport) rather than two corners,
// and the fill is a flat disc (no rounded-corner SDF).
//
// SCOPE: instancedPointColor@1 ONLY.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnInstancedPointColorBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override {
    return "instancedPointColor@1";
  }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached instancedPointColor render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers (lazily built, reused). Non-indexed uploads the
  // geometry's Point4Color bytes directly; the indexed (D26) path uploads a
  // CPU-gathered scratch buffer of only the selected instances. ENC-558: source
  // CpuBufferStore versions are remembered so a streaming grow re-uploads while
  // static geometry stays a pure cache hit.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};   // per-instance Point4Color records
    std::uint32_t instanceCount{0};  // points drawn (gathered count when indexed)
    std::size_t bufferCapacity{0};   // byte size of instanceBuffer
    std::uint64_t vtxVersion{0};     // CpuBufferStore version of vertexBufferId
    std::uint64_t idxVersion{0};     // CpuBufferStore version of indexBufferId
    bool built{false};               // false until first successful (re)build
  };
  std::vector<std::pair<std::uint32_t, GeoBuffers>> geoBuffers_;

  void buildGeoBuffers(GpuDevice& device, const Scene& scene,
                       CpuBufferStore& gpu, std::uint32_t geometryId,
                       GeoBuffers& gb);

  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, std::uint32_t geometryId);
};

}  // namespace dc
