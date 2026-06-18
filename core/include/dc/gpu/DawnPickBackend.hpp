// ENC-495 (P3.3) — Dawn/WebGPU GPU color-ID picking (D29.3).
//
// The Dawn mirror of GL's renderPick/drawPick + the kPick* shaders
// (core/src/gl/Renderer.cpp). Pixel-perfect hit testing: each DrawItem's id is
// encoded as a unique 24-bit RGB color (R=id&0xFF, G=(id>>8)&0xFF,
// B=(id>>16)&0xFF, each /255) and rendered into a SEPARATE offscreen pick target
// (RenderTargetHandle id 1, via the DawnDevice multi-target support). The pixel
// under the cursor is read back and decoded to recover the id; id 0 == no hit.
//
// PICK-PIPELINE APPROACH (mirrors GL's dedicated pick shaders, one class):
// GL uses four dedicated pick programs (pickFlat, pickInstRect, pickInstCandle,
// pickLineAA) — the SAME geometry as the visible draw, but a FLAT-id fragment
// color (no AA fringe, no gradient, no rounding, no candle up/down split). This
// backend reproduces that on Dawn as one class owning four pick pipelines built
// on the (Normal blend, no clip) variant: id writes are OPAQUE, so no blend, and
// picking never clips. The id color is supplied per-DrawItem via the existing
// `u_color` uniform (float index 12 in the shared uniform block — see
// DawnDevice::createBindGroup), so no scene state is mutated.
//
// PICKABLE PIPELINES (matches the GL pickable set exactly):
//   pickFlat       — triSolid@1, line2d@1, points@1, triAA@1, triGradient@1
//   pickInstRect   — instancedRect@1, texturedQuad@1
//   pickInstCandle — instancedCandle@1
//   pickLineAA     — lineAA@1
//   textSDF@1 is NOT pickable (matches GL — text has no pick variant).
//
// ENC-652 (C2a) adds the per-instance-color marks (the ones that need picking most
// — a treemap/correlation grid or a scatter is one DrawItem of many instances):
//   pickInstRectColor  — instancedRectColor@1   (Rect4Color, 24B; rect4 lane only)
//   pickInstPointColor — instancedPointColor@1  (Point4Color, 16B; pos2 + size)
//
// The geometry gather mirrors each visible backend (D26 indexed-instance gather
// into a scratch buffer for the instanced pipelines; per-vertex stride skipping
// for the flat pos2 attribute of triAA/triGradient).
#pragma once

#include "dc/render/GpuDevice.hpp"
#include "dc/event/EventBus.hpp"
#include "dc/ids/Id.hpp"
#include "dc/render/PickInstanceTable.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

class Scene;
class CpuBufferStore;
struct DrawItem;

/// PickResult mirrors dc::PickResult in the GL Renderer: the decoded DrawItem id
/// under the probed pixel (0 == no hit / background).
///
/// ENC-627 (C1): per-instance fields. `instanceIndex` is the hit instance within
/// the DrawItem (-1 = unknown; the ENC-628 shader change supplies it for instanced
/// marks), and `rowId` is the durable source row id resolved from the
/// PickInstanceTable (-1 = none). Until ENC-628 both stay -1 and picking behaves
/// exactly as before (DrawItem-level only).
struct DawnPickResult {
  std::uint32_t drawItemId{0};
  std::int32_t instanceIndex{-1};
  std::int32_t rowId{-1};
};

class DawnPickBackend {
 public:
  /// Build the four pick pipelines (flat / instRect / instCandle / lineAA) on the
  /// (Normal, None) variant. Returns false if any pipeline failed to compile.
  bool init(GpuDevice& device);

  /// ENC-627 (C1): the per-DrawItem row-id side table. Register a DrawItem's
  /// EncodeResult::instanceRowIds here so a per-instance pick (ENC-628) resolves
  /// to a durable source row id. Borrowed/owned here; the caller updates it when
  /// it (re)compiles a mark.
  PickInstanceTable& instanceTable() { return instanceTable_; }
  const PickInstanceTable& instanceTable() const { return instanceTable_; }
  void setInstanceRowIds(Id drawItemId, std::vector<std::int32_t> rowIds) {
    instanceTable_.setInstanceRowIds(drawItemId, std::move(rowIds));
  }

  /// Walk the scene, render every pickable DrawItem's id-as-RGB into the pick
  /// target, read back the pixel at (pickX, pickY) and decode the id. Mirrors
  /// GL Renderer::renderPick: a dedicated render pass into RenderTargetHandle 1
  /// (the pick buffer), cleared to transparent/black so unhit pixels decode to 0.
  /// When `bus` is non-null and a hit is found, emits a GeometryClicked event
  /// (targetId = id, payload[0/1] = pickX/pickY), matching GL's D42 behaviour.
  DawnPickResult renderPick(GpuDevice& device, const Scene& scene,
                            CpuBufferStore& gpu, int viewW, int viewH,
                            int pickX, int pickY, EventBus* bus = nullptr);

 private:
  // The pick render-target id (distinct from the main target 0). Reused so the
  // pick pass never clobbers the visible framebuffer.
  static constexpr std::uint32_t kPickTargetId = 1;

  // Draw one DrawItem's geometry with the supplied flat id color into the
  // currently-active (pick) render pass. Dispatches on di.pipeline to the right
  // pick pipeline; skips textSDF@1 and unknown pipelines (no-op).
  void drawPickItem(GpuDevice& device, const Scene& scene, CpuBufferStore& gpu,
                    const DrawItem& di, int viewW, int viewH,
                    const float idColor[4]);

  // ENC-628 (C2b): draw ONLY `di`'s instances into the active (2nd pick) pass with
  // each fragment emitting (instance_index + 1) as 24-bit RGB. Dispatches on
  // di.pipeline to the matching idx* pipeline; a no-op for non-instanced or
  // not-yet-supported pipelines (instanceIndex then stays -1, DrawItem-level pick
  // only). Returns true if it issued an instance-index draw.
  bool drawInstanceIndexItem(GpuDevice& device, const Scene& scene,
                             CpuBufferStore& gpu, const DrawItem& di, int viewW,
                             int viewH);

  // One pick pipeline per geometry kind (see header comment).
  PipelineHandle pickFlat_{};
  PipelineHandle pickInstRect_{};
  PipelineHandle pickInstCandle_{};
  PipelineHandle pickLineAA_{};
  // ENC-652 (C2a): per-instance-color marks (treemap/correlation rects, scatter
  // points). Same geometry as pickInstRect_/the point scatter but a 24B Rect4Color
  // / 16B Point4Color instance stride (the color + reserved lanes are skipped), so
  // they need their own pipelines (distinct vertex strides) even though the rect
  // pick shader is shared with pickInstRect_.
  PipelineHandle pickInstRectColor_{};
  PipelineHandle pickInstPointColor_{};

  // ENC-628 (C2b): per-instance-INDEX pipelines. Same vertex geometry as the
  // matching pick pipeline above, but the fragment emits the (instance_index + 1)
  // encoded as 24-bit RGB instead of the DrawItem id color. A second pick pass
  // renders ONLY the hit DrawItem's instances through these, so reading the same
  // pixel decodes the hit instance index (the +1 keeps instance 0 distinct from the
  // cleared-to-zero background). rect16 serves instancedRect@1 / texturedQuad@1;
  // rect24 serves instancedRectColor@1; point serves instancedPointColor@1.
  PipelineHandle idxInstRect_{};       // stride 16 (rect4 / pos2uv4 lead)
  PipelineHandle idxInstRectColor_{};  // stride 24 (Rect4Color)
  PipelineHandle idxInstPointColor_{}; // stride 16 (Point4Color: pos2 + size)

  // Per-geometry scratch GPU buffers for the pick draw, keyed by geometryId.
  // The flat path uploads the raw pos2-strided vertex buffer; the instanced
  // paths upload the gathered (D26) instance subset. Created lazily and reused.
  struct GeoBuffers {
    BufferHandle vertexBuffer{};   // flat: pos2 vertices; inst: instance records
    BufferHandle indexBuffer{};    // flat indexed draw only (instanced gathers CPU-side)
    std::uint32_t vertexCount{0};
    std::uint32_t indexCount{0};
    std::uint32_t instanceCount{0};
  };
  std::vector<std::pair<Id, GeoBuffers>> geoBuffers_;
  GeoBuffers& ensureGeoBuffers(GpuDevice& device, const Scene& scene,
                               CpuBufferStore& gpu, const DrawItem& di);

  // ENC-627 (C1): per-DrawItem instance row ids, for per-instance pick resolution.
  PickInstanceTable instanceTable_;
};

}  // namespace dc
