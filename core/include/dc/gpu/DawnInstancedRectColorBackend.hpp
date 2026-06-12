// ENC-608 (P2.1) — Dawn/WebGPU backend for the `instancedRectColor@1` pipeline:
// THE KEYSTONE per-instance-color rect.
//
// WHY THIS IS THE KEYSTONE (RESEARCH §4.3)
// ----------------------------------------
// Every mark except candle collapses to ONE uniform color per draw item
// (Types.hpp DrawItem.color) — which is exactly why the showcase RASTERIZES the
// weather-radar / correlation / footprint / pie views to textures: their cells
// need a DIFFERENT color PER INSTANCE, and the single-uniform instancedRect@1
// cannot express that. Candle6 (Types.hpp / DawnInstancedCandleBackend) is the
// existing PROOF the engine can encode per-instance multi-channel data in the
// instance record; this backend GENERALIZES that proof: it reads a per-instance
// packed RGBA8 color straight out of the instance buffer, so those ~4 walled
// views render NATIVELY with ZERO compute.
//
// RELATION to DawnInstancedRectBackend (the model)
// ------------------------------------------------
// This is the per-instance-color sibling of DawnInstancedRectBackend. Same unit
// quad (6 verts from @builtin(vertex_index) % 6), same mat3 transform + viewport
// uniforms, same D26 indexed-gather + ENC-558 streaming-grow caching. The ONLY
// differences:
//   * The instance record is Rect4Color (24B), not Rect4 (16B):
//       offset  0 : vec4 f32  rect (x0,y0,x1,y1)   — Float32x4 @ location 0
//       offset 16 : RGBA8      color               — Unorm8x4  @ location 1
//       offset 20 : f32        scalar lane         — Float32   @ location 2
//   * The fill color comes from the per-instance attribute (location 1), NOT the
//     u_color uniform — so there is no u_color binding at all.
//   * Rounded corners (D28.2) are supported identically (cornerRadius uniform).
//
// THE RESERVED LANE (ENC-601 finding + design note)
// -------------------------------------------------
// Rect4Color is 24B precisely so it has ROOM. The last 4 bytes (offset 20) are a
// general per-instance scalar lane today (currently unused by the shader; the
// encode pass packs 0). It is DELIBERATELY reserved so a future per-instance ROW
// ID for picking (ENC-594's rowIdColumn -> rendered instance -> source row) can
// ride this lane WITHOUT a new format: reinterpret it as a Uint32 attribute when
// picking lands (ENC-609+). Picking is NOT built here — the lane is just left
// open and documented.
//
// SCOPE: instancedRectColor@1 ONLY. instancedPointColor (ENC-609), color scales
// (ENC-610/611) and polar (ENC-613) are separate tickets.
#pragma once

#include "dc/render/IRendererBackend.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class DawnInstancedRectColorBackend final : public IRendererBackend {
 public:
  std::string_view pipelineId() const override { return "instancedRectColor@1"; }

  bool init(GpuDevice& device) override;

  BackendStats renderDrawItem(GpuDevice& device, const Scene& scene,
                              CpuBufferStore& gpu, const DrawItem& item,
                              int viewW, int viewH) override;

 private:
  // The cached instancedRectColor render pipeline (created once in init()).
  PipelineHandle pipeline_{};

  // Per-geometry instance buffers (lazily built, reused). The non-indexed path
  // uploads the geometry's Rect4Color bytes directly; the indexed (D26) path
  // uploads a CPU-gathered scratch buffer of only the selected instances.
  // ENC-558: source CpuBufferStore versions are remembered so a streaming grow /
  // in-place edit re-uploads, while static geometry stays a pure cache hit.
  struct GeoBuffers {
    BufferHandle instanceBuffer{};   // per-instance Rect4Color records
    std::uint32_t instanceCount{0};  // rects drawn (gathered count when indexed)
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
