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
};

}  // namespace dc
