// ENC-509 (P5.0a) — DawnSceneRenderer implementation. The full scene-walk
// dispatcher on Dawn; the mirror of GL Renderer::render (core/src/gl/Renderer.cpp).
//
// ENC-511 (P5.0c) closed the two multi-pane gaps below (per-pane clear colors +
// D78 pane borders/separators). They are now reproduced for Dawn; see the
// fillRect/clearPane/drawPaneBorder/drawPaneSeparators helpers and the
// solidPipeline_ note in the header.
//
// HOW THE TWO GAPS ARE CLOSED (so ENC-510's parity harness knows what to expect):
//   * PER-PANE MID-PASS CLEAR (pane.hasClearColor): GL issues a scissored glClear
//     inside the pass. WebGPU has no scissored mid-pass clear (clears are a
//     render-pass load op), so we draw a full-pane clear-QUAD with the pane
//     scissor active — clearPane(). Visually identical: the scissor box bounds
//     the quad to the pane, just like GL's scissored glClear bounds the clear.
//     (GL also clears STENCIL here; these multi-pane scenes don't clip, so the
//     color-only quad matches the visible output.)
//   * PANE BORDERS / SEPARATORS (D78): GL draws them with inline pos2 GL_LINES
//     geometry (Renderer::drawPaneBorder / drawPaneSeparators). On Dawn there is
//     no wide-line primitive, so each border edge / separator is a thin FILLED
//     rect (two triangles) of the configured pixel width via solidPipeline_,
//     drawn AFTER the scene content (GL's order) so they sit on top. Pixel widths
//     are converted to clip-space half-extents per axis.
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

#include <algorithm>
#include <cmath>

namespace dc {

namespace {

const float kIdentityMat3[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};

// ENC-511 — WGSL for the self-contained solid-fill pipeline used by per-pane
// clear-quads + D78 border/separator edge rects. A pos2 + mat3 transform + vec4
// color shader, byte-identical in shape to DawnTriSolidBackend's WGSL (same
// 64-byte uniform packing, same NDC y-flip so synthetic quads land in the same
// screen region as the scene + scissor).
const char* kSolidFillWgsl = R"WGSL(
struct Uniforms {
  c0    : vec4<f32>,
  c1    : vec4<f32>,
  c2    : vec4<f32>,
  color : vec4<f32>,
};
@group(0) @binding(0) var<uniform> u : Uniforms;

@vertex
fn vs_main(@location(0) a_pos : vec2<f32>) -> @builtin(position) vec4<f32> {
  let m = mat3x3<f32>(u.c0.xyz, u.c1.xyz, u.c2.xyz);
  let p = m * vec3<f32>(a_pos, 1.0);
  return vec4<f32>(p.x, -p.y, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
  return u.color;
}
)WGSL";

}  // namespace

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
  if (!reg(instRectColor_, "instancedRectColor@1")) return false;  // ENC-608
  if (!reg(instCandle_, "instancedCandle@1")) return false;

  // textSDF@1 / texturedQuad@1 only when their resource was supplied.
  if (textSdf_ && !reg(*textSdf_, "textSDF@1")) return false;
  if (texturedQuad_ && !reg(*texturedQuad_, "texturedQuad@1")) return false;

  // Pick backend (separate from the registry — it owns its own pipelines).
  if (!pick_.init(*device_)) {
    errorMessage_ = "DawnPickBackend::init failed";
    return false;
  }

  // ENC-511 — the solid-fill pipeline behind per-pane clear-quads + D78
  // border/separator edge rects. One pos2 Float32x2 attribute, identity-ready
  // mat3 + color uniform (the device's 64-byte base layout). Triangles.
  {
    VertexAttribute attr;
    attr.location = 0;
    attr.componentCount = 2;
    attr.type = VertexComponentType::Float32;
    attr.offsetBytes = 0;

    VertexBufferLayout layout;
    layout.strideBytes = 8;  // pos2
    layout.stepInstance = false;
    layout.attributes = &attr;
    layout.attributeCount = 1;

    PipelineDesc desc;
    desc.debugName = "dawnSceneRenderer.solidFill";
    desc.vertexSource = kSolidFillWgsl;
    desc.fragmentSource = nullptr;
    desc.vertexBuffers = &layout;
    desc.vertexBufferCount = 1;
    desc.topology = PrimitiveTopology::Triangles;
    desc.blend = DeviceBlendMode::Normal;
    desc.clip = ClipMode::None;

    solidPipeline_ = device_->createPipeline(desc);
    if (!solidPipeline_.valid()) {
      errorMessage_ = "DawnSceneRenderer: solid-fill pipeline build failed";
      return false;
    }
  }

  inited_ = true;
  return true;
}

// ENC-511 — fill a clip-space rectangle in a flat color via solidPipeline_. The
// caller has already set the scissor (per-pane clear) or expanded it (borders).
// Drives the device directly (no Scene): build a 6-vertex pos2 quad buffer, a
// bind group with identity transform + color, bind + draw. Mirrors
// DawnTriSolidBackend's draw shape; the per-call buffer/bind-group are reclaimed
// at device teardown (same lifetime note as ENC-485 there).
void DawnSceneRenderer::fillRect(float x0, float y0, float x1, float y1,
                                 const float rgba[4]) {
  if (!solidPipeline_.valid()) return;
  if (rgba[3] <= 0.0f &&
      rgba[0] <= 0.0f && rgba[1] <= 0.0f && rgba[2] <= 0.0f) {
    // Fully transparent black: nothing to paint (matches GL's zero-color edges
    // contributing nothing, and avoids a needless draw for default styles).
  }

  const float verts[] = {
      x0, y0,  x1, y0,  x1, y1,  // tri 1
      x0, y0,  x1, y1,  x0, y1,  // tri 2
  };
  BufferHandle vbo = device_->createBuffer(sizeof(verts), verts, sizeof(verts));
  if (!vbo.valid()) return;

  UniformBinding uniforms[2];
  uniforms[0].kind = UniformBinding::Kind::Mat3;
  uniforms[0].name = "u_transform";
  uniforms[0].data = kIdentityMat3;
  uniforms[1].kind = UniformBinding::Kind::Vec4;
  uniforms[1].name = "u_color";
  uniforms[1].data = rgba;

  BindGroupDesc bg;
  bg.pipeline = solidPipeline_;
  bg.vertexBuffers = &vbo;
  bg.vertexBufferCount = 1;
  bg.uniforms = uniforms;
  bg.uniformCount = 2;

  BindGroupHandle group = device_->createBindGroup(bg);
  if (!group.valid()) return;

  // The solid-fill draws are always Normal blend, no clip — reset device state
  // so a prior per-item blend/clip doesn't leak into the synthetic draw.
  device_->setBlendMode(DeviceBlendMode::Normal);
  device_->setClipState(ClipMode::None);
  device_->bindPipeline(solidPipeline_);

  DrawParams params;
  params.vertexCount = 6;
  params.indexCount = 0;
  params.firstVertex = 0;
  device_->draw(group, params);
}

// ENC-511 — per-pane scissored clear: paint a full-pane quad of the clear color
// (the pane scissor, set by the caller in render(), bounds it to the region).
void DawnSceneRenderer::clearPane(const Pane& pane, const float rgba[4]) {
  const auto& r = pane.region;
  fillRect(r.clipXMin, r.clipYMin, r.clipXMax, r.clipYMax, rgba);
}

// ENC-511 — D78 pane border: four thin filled edge rects around the pane region.
// GL draws a GL_LINES loop at paneBorderWidth (clamped by the driver); on Dawn we
// fill rects whose thickness is the pixel width mapped to clip units per axis
// (clip spans 2.0 over `view` pixels => clipPerPx = 2/view). The edges are placed
// flush to the region boundary (matching GL's loop on the clip rect).
void DawnSceneRenderer::drawPaneBorder(const Pane& pane, int viewW, int viewH) {
  if (renderStyle_.paneBorderWidth <= 0.0f) return;
  const auto& r = pane.region;
  const float* c = renderStyle_.paneBorderColor;

  const float wx = renderStyle_.paneBorderWidth * (2.0f / std::max(1, viewW));
  const float wy = renderStyle_.paneBorderWidth * (2.0f / std::max(1, viewH));

  // bottom + top (full width), then left + right (between them).
  fillRect(r.clipXMin, r.clipYMin, r.clipXMax, r.clipYMin + wy, c);  // bottom
  fillRect(r.clipXMin, r.clipYMax - wy, r.clipXMax, r.clipYMax, c);  // top
  fillRect(r.clipXMin, r.clipYMin, r.clipXMin + wx, r.clipYMax, c);  // left
  fillRect(r.clipXMax - wx, r.clipYMin, r.clipXMax, r.clipYMax, c);  // right
}

// ENC-511 — D78 separators: a thin filled rect at the boundary between each pair
// of consecutive panes (GL draws a horizontal GL_LINES segment there). The
// boundary y is the midpoint of the upper pane's clipYMin and the lower pane's
// clipYMax (verbatim from GL Renderer::drawPaneSeparators); the rect spans the
// union x-extent and is centered on that y at separatorWidth pixels.
void DawnSceneRenderer::drawPaneSeparators(const Scene& scene, int viewW,
                                           int viewH) {
  (void)viewW;
  if (renderStyle_.separatorWidth <= 0.0f) return;
  auto pIds = scene.paneIds();
  if (pIds.size() < 2) return;

  const float* c = renderStyle_.separatorColor;
  const float wy = renderStyle_.separatorWidth * (2.0f / std::max(1, viewH));

  for (std::size_t i = 0; i + 1 < pIds.size(); ++i) {
    const Pane* upper = scene.getPane(pIds[i]);
    const Pane* lower = scene.getPane(pIds[i + 1]);
    if (!upper || !lower) continue;
    float sepY = (upper->region.clipYMin + lower->region.clipYMax) * 0.5f;
    float xMin = std::min(upper->region.clipXMin, lower->region.clipXMin);
    float xMax = std::max(upper->region.clipXMax, lower->region.clipXMax);
    fillRect(xMin, sepY - wy * 0.5f, xMax, sepY + wy * 0.5f, c);
  }
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

    // ENC-511 — per-pane clear color (D10.4). GL issues a scissored glClear here;
    // WebGPU has no scissored mid-pass clear, so we paint a full-pane clear-quad
    // with the pane scissor (set above) bounding it to the region.
    if (pane->hasClearColor) {
      clearPane(*pane, pane->clearColor);
    }

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

  // ENC-511 — D78 pane borders + separators, drawn AFTER all content (GL's draw
  // order) so they sit on top. Borders are scissored to the pane (expanded 1px so
  // the edge rect isn't clipped at the boundary, matching GL's bsx/bsy expand);
  // separators are full-frame (scissor reset).
  if (renderStyle_.paneBorderWidth > 0.0f) {
    for (Id paneId : scene.paneIds()) {
      const Pane* pane = scene.getPane(paneId);
      if (!pane) continue;
      // Expand the pane scissor 1px so the edge rect isn't clipped at the
      // boundary (matches GL's bsx/bsy expand), then CLAMP to [0,view] — WebGPU
      // SetScissorRect treats x+width>target / y+height>target as a validation
      // error (which would abort the whole pass), unlike GL's silent clamp.
      int bsx = static_cast<int>(std::round((pane->region.clipXMin + 1.0f) / 2.0f * viewW)) - 1;
      int bsy = static_cast<int>(std::round((pane->region.clipYMin + 1.0f) / 2.0f * viewH)) - 1;
      int bsx2 = static_cast<int>(std::round((pane->region.clipXMax + 1.0f) / 2.0f * viewW)) + 1;
      int bsy2 = static_cast<int>(std::round((pane->region.clipYMax + 1.0f) / 2.0f * viewH)) + 1;
      bsx = std::max(0, bsx);   bsy = std::max(0, bsy);
      bsx2 = std::min(viewW, bsx2); bsy2 = std::min(viewH, bsy2);
      device_->setScissorRect({bsx, bsy, bsx2 - bsx, bsy2 - bsy});
      drawPaneBorder(*pane, viewW, viewH);
    }
  }
  if (renderStyle_.separatorWidth > 0.0f) {
    device_->setScissorRect({0, 0, viewW, viewH});
    drawPaneSeparators(scene, viewW, viewH);
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
