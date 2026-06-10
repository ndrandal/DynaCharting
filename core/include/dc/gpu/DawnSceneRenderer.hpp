// ENC-509 (P5.0a) — DawnSceneRenderer: full scene-walk dispatch on Dawn.
//
// GOAL
// ----
// The Dawn equivalent of GL `Renderer::render` (core/src/gl/Renderer.cpp). Until
// now every Dawn render test manually constructed a DawnDevice, registered the
// one or two backends it needed, and hand-walked the scene (begin pass / dispatch
// one DrawItem / end pass). DawnSceneRenderer makes that reusable: it owns a
// DawnDevice (or borrows one), registers ALL 10 Dawn pipeline backends once under
// DeviceKind::Dawn, and exposes `render(scene, store, viewW, viewH)` which walks
// a whole Scene exactly like GL Renderer::render — clear, per-pane scissor,
// per-item frustum-cull + blend + clip + transform, dispatch via
// backends.find(DeviceKind::Dawn, di->pipeline). This is the keystone for
// comprehensive conformance (ENC-510) and the cutover (ENC-500).
//
// MIRRORS GL Renderer::render (see that function for the reference walk):
//   * frame-level begin pass: bind main target 0, set viewport, clear color +
//     stencil (matches GL beginRenderPass with pass.clear / clearStencil).
//   * scissor test stays "enabled" across the pass; each pane sets the scissor
//     rect from its clip region (NDC -> pixels). On WebGPU SetScissorRect is a
//     pass call (no separate enable toggle — see DawnDevice::setScissorRect).
//   * for each pane (scene order) -> each layer in that pane -> each DrawItem in
//     that layer: skip empty-pipeline / invisible items; frustum-cull against the
//     pane clip region (identical math to GL); apply per-item blend mode (device
//     setBlendMode) and clip state (setClipState — WriteMask source / UseMask
//     content / None); dispatch the draw through the registered Dawn backend.
//   * pane borders / separators are NOT reproduced (see the DIFFERENCES note in
//     the .cpp — GL draws them with inline pos2 line geometry that has no Dawn
//     backend; ENC-510's parity harness should expect the Dawn output to omit
//     them, or supply them as explicit line2d DrawItems).
//
// ATLAS / TEXTURE INPUTS
// ----------------------
// Two of the 10 pipelines need extra resources that are not in the Scene:
//   * textSDF@1     needs a GlyphAtlas (R8 SDF bitmap + glyph UV rects).
//   * texturedQuad@1 needs CPU pixels for a logical textureId, via a
//                    TextureSource provider (DawnTexturedQuadBackend's seam).
// Both are OPTIONAL and supplied at construction. When null, the renderer simply
// does not register that backend, so a scene using textSDF@1 / texturedQuad@1
// without the resource silently skips those items (find() returns nullptr) —
// exactly like GL's drawTextSdf/ drawTexturedQuad early-out when atlas_/texMgr_
// is null. The other 8 pipelines always work.
//
// OWNERSHIP: the renderer owns one instance of each of the 10 backends (members)
// and a BackendRegistry pointing at them. The DawnDevice is either owned (default
// ctor) or borrowed (the borrow ctor, for tests that already hold one). All
// backends + the registry outlive every render() call.
#pragma once

#include "dc/gpu/DawnDevice.hpp"

#include "dc/gpu/DawnTriSolidBackend.hpp"
#include "dc/gpu/DawnTriGradientBackend.hpp"
#include "dc/gpu/DawnTriAABackend.hpp"
#include "dc/gpu/DawnLine2dBackend.hpp"
#include "dc/gpu/DawnLineAABackend.hpp"
#include "dc/gpu/DawnPointsBackend.hpp"
#include "dc/gpu/DawnInstancedRectBackend.hpp"
#include "dc/gpu/DawnInstancedCandleBackend.hpp"
#include "dc/gpu/DawnTextSdfBackend.hpp"
#include "dc/gpu/DawnTexturedQuadBackend.hpp"
#include "dc/gpu/DawnPickBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/debug/Stats.hpp"

#include <memory>

namespace dc {

class Scene;
class CpuBufferStore;
class GlyphAtlas;
class EventBus;
struct Pane;

// ENC-511 (P5.0c) — pane border + separator style. A POD mirror of GL's
// `dc::RenderStyle` (core/include/dc/gl/Renderer.hpp): the DawnSceneRenderer
// can't include the dc_gl header (it lives in a different library), so the
// struct is duplicated field-for-field. setRenderStyle() drives the same D78
// border/separator pass GL's Renderer::render does. Defaults are zero-width =
// nothing drawn, so the common single-pane case is byte-identical to ENC-509.
struct DawnRenderStyle {
  float paneBorderColor[4] = {0, 0, 0, 0};
  float paneBorderWidth{0.0f};  // pixels, 0 = no border
  float separatorColor[4] = {0, 0, 0, 0};
  float separatorWidth{0.0f};   // pixels, 0 = no separator
};

class DawnSceneRenderer {
 public:
  // Own a freshly-created DawnDevice. `atlas` (textSDF@1) and `textures`
  // (texturedQuad@1) are optional; pass null to skip those pipelines. The
  // backends are constructed but NOT yet initialised — call init().
  explicit DawnSceneRenderer(GlyphAtlas* atlas = nullptr,
                             const TextureSource* textures = nullptr);

  // Borrow an already-created (and init()'d) DawnDevice — for tests/callers that
  // own the device themselves. The renderer will NOT init() or destroy it.
  DawnSceneRenderer(DawnDevice& device, GlyphAtlas* atlas = nullptr,
                    const TextureSource* textures = nullptr);

  ~DawnSceneRenderer();

  DawnSceneRenderer(const DawnSceneRenderer&) = delete;
  DawnSceneRenderer& operator=(const DawnSceneRenderer&) = delete;

  // Bring up the device (only when owned — the borrow ctor's device is assumed
  // already up) and init() + register every Dawn backend whose resources are
  // available. Returns false if device init or any backend pipeline build fails;
  // errorMessage() then carries the reason.
  bool init();

  const std::string& errorMessage() const { return errorMessage_; }

  // ENC-511 (P5.0c) — D78 pane borders + separators. Mirrors GL
  // Renderer::setRenderStyle. Drawn on top of pane content (after the scene
  // walk), matching GL's draw order. Defaults to zero widths == nothing drawn.
  void setRenderStyle(const DawnRenderStyle& style) { renderStyle_ = style; }
  const DawnRenderStyle& renderStyle() const { return renderStyle_; }

  // The underlying device (so a caller can readPixel() the offscreen target after
  // render(), or share it with another renderer).
  DawnDevice& device() { return *device_; }

  // Walk the whole scene and render it through the Dawn backend registry into the
  // main offscreen target (id 0). The Dawn mirror of GL Renderer::render. Returns
  // the same Stats (drawCalls + culledDrawCalls) the GL renderer returns.
  Stats render(const Scene& scene, CpuBufferStore& store, int viewW, int viewH);

  // GPU color-ID picking entry — the Dawn mirror of GL Renderer::renderPick. Owns
  // the DawnPickBackend; renders pickable items' ids into the pick target (id 1)
  // and decodes the pixel under (pickX, pickY). 0 == no hit. Cheap to expose since
  // DawnPickBackend already exists (ENC-495).
  DawnPickResult renderPick(const Scene& scene, CpuBufferStore& store,
                            int viewW, int viewH, int pickX, int pickY,
                            EventBus* bus = nullptr);

 private:
  // The device: owned (ownedDevice_ set) or borrowed (device_ points at the
  // caller's). device_ always points at the live device.
  std::unique_ptr<DawnDevice> ownedDevice_;
  DawnDevice* device_{nullptr};
  bool ownsDevice_{true};

  GlyphAtlas* atlas_{nullptr};
  const TextureSource* textures_{nullptr};

  // The 10 pipeline backends + pick. Owned here; registered under DeviceKind::Dawn
  // once in init(). textSDF / texturedQuad are only registered when their resource
  // (atlas / textures) is non-null.
  DawnTriSolidBackend triSolid_;
  DawnTriGradientBackend triGradient_;
  DawnTriAABackend triAA_;
  DawnLine2dBackend line2d_;
  DawnLineAABackend lineAA_;
  DawnPointsBackend points_;
  DawnInstancedRectBackend instRect_;
  DawnInstancedCandleBackend instCandle_;
  std::unique_ptr<DawnTextSdfBackend> textSdf_;           // only if atlas_
  std::unique_ptr<DawnTexturedQuadBackend> texturedQuad_; // only if textures_
  DawnPickBackend pick_;

  BackendRegistry backends_;
  bool inited_{false};
  std::string errorMessage_;

  // ENC-511 (P5.0c) — D78 pane border/separator style + per-pane clear.
  DawnRenderStyle renderStyle_;

  // A self-contained solid-fill pipeline (a tiny pos2 triSolid clone) used to
  // paint clip-space quads in a flat color: per-pane clear-quads (the Dawn
  // analogue of GL's scissored mid-pass glClear) and the border/separator edge
  // rects. It's separate from the registry's triSolid_ backend because those
  // draws are synthetic (no Scene DrawItem / Geometry) — we drive the device
  // directly (createBuffer + createBindGroup + bindPipeline + draw), exactly the
  // shape DawnTriSolidBackend uses but without a Scene lookup.
  PipelineHandle solidPipeline_;

  // Draw a filled clip-space rectangle [x0,x1]x[y0,y1] in `rgba`, scissor as set
  // by the caller. Goes through solidPipeline_ (which y-flips like every backend
  // so it lands in the same screen region as the scene + scissor).
  void fillRect(float x0, float y0, float x1, float y1, const float rgba[4]);

  // ENC-511: per-pane scissored clear. GL issues a scissored glClear inside the
  // pass for pane.hasClearColor; WebGPU has no scissored mid-pass clear, so we
  // draw a full-pane clear-quad (with the pane scissor active) instead.
  void clearPane(const Pane& pane, const float rgba[4]);

  // ENC-511: D78 border (4 thin edge rects around the pane region) + separators
  // (thin rects at the boundary between consecutive panes). Pixel widths from
  // renderStyle_ are converted to clip-space half-extents per axis.
  void drawPaneBorder(const Pane& pane, int viewW, int viewH);
  void drawPaneSeparators(const Scene& scene, int viewW, int viewH);
};

}  // namespace dc
