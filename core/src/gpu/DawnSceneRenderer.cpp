// ENC-509 (P5.0a) — DawnSceneRenderer implementation. The full scene-walk
// dispatcher on Dawn; the mirror of GL Renderer::render (core/src/gl/Renderer.cpp).
//
// DIFFERENCES vs GL Renderer::render (so ENC-510's parity harness knows what to
// expect):
//   * PANE BORDERS / SEPARATORS are NOT drawn. GL draws them with inline pos2
//     line geometry built on the fly (Renderer::drawPaneBorder /
//     drawPaneSeparators) — there is no Scene DrawItem and no Dawn backend for
//     them. Reproducing them would mean a synthetic line2d backend draw per pane
//     edge; out of scope for the keystone scene walk (the RenderStyle defaults to
//     zero-width borders/separators anyway, so the common case is unaffected).
//   * PER-PANE MID-PASS CLEAR (pane.hasClearColor): GL issues a scissored
//     glClear inside the pass. WebGPU has no scissored mid-pass clear; clears are
//     a render-pass load op. We approximate by drawing a full-pane clear quad is
//     NOT done here either — instead the per-pane clear color is left to the
//     frame-level clear (the common single-pane case matches GL since GL's pane
//     region == full viewport). Multi-pane scenes with distinct per-pane clear
//     colors are a known parity gap (TODO: model as a scoped sub-pass).
//   * SCISSOR is set per pane exactly like GL (NDC clip region -> pixel rect).
//     DawnDevice::setScissorRect flips y to top-left internally; the GL path is
//     bottom-left. Both land the pane content in the same screen region for the
//     readback convention the backends already use (the WGSL NDC y-flip).
//   * BLEND + CLIP + FRUSTUM CULL + TRANSFORM are handled identically to GL: the
//     same toDeviceBlend mapping, the same WriteMask/UseMask/None clip selection,
//     the same cull math against the pane clip region, and the transform is
//     resolved by each backend from di.transformId (as in GL).
#include "dc/gpu/DawnSceneRenderer.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Types.hpp"
#include "dc/scene/Geometry.hpp"

#include <cmath>

namespace dc {

// ENC-482: map the scene-level BlendMode onto the device-neutral enum (verbatim
// from core/src/gl/Renderer.cpp's toDeviceBlend so blend parity is exact).
static DeviceBlendMode toDeviceBlend(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal:   return DeviceBlendMode::Normal;
    case BlendMode::Additive: return DeviceBlendMode::Additive;
    case BlendMode::Multiply: return DeviceBlendMode::Multiply;
    case BlendMode::Screen:   return DeviceBlendMode::Screen;
  }
  return DeviceBlendMode::Normal;
}

DawnSceneRenderer::DawnSceneRenderer(GlyphAtlas* atlas,
                                     const TextureSource* textures)
    : ownedDevice_(std::make_unique<DawnDevice>()),
      device_(ownedDevice_.get()),
      ownsDevice_(true),
      atlas_(atlas),
      textures_(textures) {
  if (atlas_) {
    textSdf_ = std::make_unique<DawnTextSdfBackend>(atlas_);
  }
  if (textures_) {
    texturedQuad_ = std::make_unique<DawnTexturedQuadBackend>(textures_);
  }
}

DawnSceneRenderer::DawnSceneRenderer(DawnDevice& device, GlyphAtlas* atlas,
                                     const TextureSource* textures)
    : device_(&device),
      ownsDevice_(false),
      atlas_(atlas),
      textures_(textures) {
  if (atlas_) {
    textSdf_ = std::make_unique<DawnTextSdfBackend>(atlas_);
  }
  if (textures_) {
    texturedQuad_ = std::make_unique<DawnTexturedQuadBackend>(textures_);
  }
}

DawnSceneRenderer::~DawnSceneRenderer() = default;

bool DawnSceneRenderer::init() {
  if (inited_) return true;

  // Bring up the device only when we own it (the borrow ctor's device is assumed
  // already init()'d by the caller).
  if (ownsDevice_) {
    if (!device_->init()) {
      errorMessage_ = "DawnDevice::init failed: " + device_->errorMessage();
      return false;
    }
  }

  // init() + register every backend. Each backend builds its immutable pipeline
  // (and bind-group layout) once here; renderDrawItem then never rebuilds it.
  auto reg = [&](IRendererBackend& b, const char* name) -> bool {
    if (!b.init(*device_)) {
      errorMessage_ = std::string("backend init failed: ") + name;
      return false;
    }
    backends_.registerBackend(DeviceKind::Dawn, &b);
    return true;
  };

  if (!reg(triSolid_, "triSolid@1")) return false;
  if (!reg(triGradient_, "triGradient@1")) return false;
  if (!reg(triAA_, "triAA@1")) return false;
  if (!reg(line2d_, "line2d@1")) return false;
  if (!reg(lineAA_, "lineAA@1")) return false;
  if (!reg(points_, "points@1")) return false;
  if (!reg(instRect_, "instancedRect@1")) return false;
  if (!reg(instCandle_, "instancedCandle@1")) return false;

  // textSDF@1 / texturedQuad@1 only when their resource was supplied.
  if (textSdf_ && !reg(*textSdf_, "textSDF@1")) return false;
  if (texturedQuad_ && !reg(*texturedQuad_, "texturedQuad@1")) return false;

  // Pick backend (separate from the registry — it owns its own pipelines).
  if (!pick_.init(*device_)) {
    errorMessage_ = "DawnPickBackend::init failed";
    return false;
  }

  inited_ = true;
  return true;
}

Stats DawnSceneRenderer::render(const Scene& scene, CpuBufferStore& store,
                                int viewW, int viewH) {
  Stats stats{};
  if (!inited_) return stats;

  // Frame-level begin pass: bind the main target (id 0), set viewport, clear
  // color + stencil. Mirrors GL Renderer::render's beginRenderPass(pass) with
  // pass.clear + pass.clearStencil. (The default black clear matches GL.)
  RenderPassDesc pass;
  pass.target = {};  // default / main target (id 0)
  pass.viewportWidth = static_cast<std::uint32_t>(viewW);
  pass.viewportHeight = static_cast<std::uint32_t>(viewH);
  pass.clear = true;
  pass.clearColor[0] = 0.0f; pass.clearColor[1] = 0.0f;
  pass.clearColor[2] = 0.0f; pass.clearColor[3] = 1.0f;
  pass.clearStencil = true;
  device_->beginRenderPass(pass);

  // Walk all draw items: pane (scissor) -> layer -> drawItem, in scene order.
  for (Id paneId : scene.paneIds()) {
    const Pane* pane = scene.getPane(paneId);
    if (!pane) continue;

    // Convert the pane clip region to a pixel scissor rect (identical math to GL
    // Renderer::render; DawnDevice flips y to top-left internally).
    int sx = static_cast<int>(std::round((pane->region.clipXMin + 1.0f) / 2.0f * viewW));
    int sy = static_cast<int>(std::round((pane->region.clipYMin + 1.0f) / 2.0f * viewH));
    int sx2 = static_cast<int>(std::round((pane->region.clipXMax + 1.0f) / 2.0f * viewW));
    int sy2 = static_cast<int>(std::round((pane->region.clipYMax + 1.0f) / 2.0f * viewH));
    device_->setScissorRect({sx, sy, sx2 - sx, sy2 - sy});

    // NOTE: GL does a scissored mid-pass clear here for pane.hasClearColor; see
    // the DIFFERENCES note at the top of this file (WebGPU has no scissored
    // mid-pass clear — known parity gap for multi-pane distinct clear colors).

    for (Id layerId : scene.layerIds()) {
      const Layer* layer = scene.getLayer(layerId);
      if (!layer || layer->paneId != paneId) continue;

      for (Id diId : scene.drawItemIds()) {
        const DrawItem* di = scene.getDrawItem(diId);
        if (!di || di->layerId != layerId) continue;
        if (di->pipeline.empty()) continue;
        if (!di->visible) continue;  // D14.2: skip invisible DrawItems

        // Frustum culling (D10.5) — identical to GL: project the geometry bounds
        // through the transform's scale + translation and reject if fully outside
        // the pane clip region.
        const Geometry* cullGeo = scene.getGeometry(di->geometryId);
        if (cullGeo && cullGeo->boundsValid && di->transformId != 0) {
          const Transform* xf = scene.getTransform(di->transformId);
          if (xf) {
            float cMinX = xf->mat3[0] * cullGeo->boundsMin[0] + xf->mat3[6];
            float cMinY = xf->mat3[4] * cullGeo->boundsMin[1] + xf->mat3[7];
            float cMaxX = xf->mat3[0] * cullGeo->boundsMax[0] + xf->mat3[6];
            float cMaxY = xf->mat3[4] * cullGeo->boundsMax[1] + xf->mat3[7];
            if (cMaxX < pane->region.clipXMin || cMinX > pane->region.clipXMax ||
                cMaxY < pane->region.clipYMin || cMinY > pane->region.clipYMax) {
              stats.culledDrawCalls++;
              continue;
            }
          }
        }

        // D29.1: per-DrawItem blend mode. On Dawn this selects the matching
        // immutable pipeline variant inside bindPipeline (ENC-493), not a mutable
        // glBlendFunc — but the dispatcher call site is identical to GL.
        device_->setBlendMode(toDeviceBlend(di->blendMode));

        // D29.2: stencil-based clipping (two-pass write/use mask). On Dawn this
        // selects the WriteMask / UseMask / None pipeline variant (ENC-494).
        if (di->isClipSource) {
          device_->setClipState(ClipMode::WriteMask);
        } else if (di->useClipMask) {
          device_->setClipState(ClipMode::UseMask);
        } else {
          device_->setClipState(ClipMode::None);
        }

        // Pipeline selection + per-pipeline draw dispatch via the backend
        // registry, keyed (DeviceKind::Dawn, di->pipeline) — exactly the GL
        // dispatch shape. A pipeline with no registered backend (e.g. textSDF@1
        // when no atlas was supplied) is silently skipped, matching GL's
        // null-atlas/null-texMgr early-outs.
        if (IRendererBackend* backend =
                backends_.find(DeviceKind::Dawn, di->pipeline)) {
          BackendStats bs = backend->renderDrawItem(*device_, scene, store,
                                                    *di, viewW, viewH);
          stats.drawCalls += bs.drawCalls;
        }
      }
    }
  }

  // Restore default device state (matches GL's end-of-pass restore), then end the
  // pass (DawnDevice submits the encoder here).
  device_->setBlendMode(DeviceBlendMode::Normal);
  device_->setClipState(ClipMode::None);
  device_->endRenderPass();
  return stats;
}

DawnPickResult DawnSceneRenderer::renderPick(const Scene& scene,
                                             CpuBufferStore& store, int viewW,
                                             int viewH, int pickX, int pickY,
                                             EventBus* bus) {
  if (!inited_) return DawnPickResult{};
  return pick_.renderPick(*device_, scene, store, viewW, viewH, pickX, pickY,
                          bus);
}

}  // namespace dc
