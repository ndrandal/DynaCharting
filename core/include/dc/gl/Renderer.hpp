#pragma once
#include "dc/gl/ShaderProgram.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/GlDevice.hpp"
#include "dc/gl/GlTriSolidBackend.hpp"
#include "dc/render/BackendRegistry.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/debug/Stats.hpp"
#include <glad/gl.h>
#include <vector>
#include <cstdint>

namespace dc {

class GlyphAtlas;
class TextureManager;
class EventBus;

// D29.3: GPU picking result
struct PickResult {
  Id drawItemId{0};  // 0 = background (no hit)
};

// D78: Render style for pane borders and separators
struct RenderStyle {
  float paneBorderColor[4] = {0,0,0,0};
  float paneBorderWidth{0.0f};  // pixels, 0 = no border
  float separatorColor[4] = {0,0,0,0};
  float separatorWidth{0.0f};   // pixels, 0 = no separator
};

class Renderer {
public:
  Renderer() = default;
  ~Renderer();

  // Compile shaders, create VAO. Call once after GL context is current.
  bool init();

  // Optional: set glyph atlas for textSDF@1 rendering.
  void setGlyphAtlas(GlyphAtlas* atlas);

  // D41: set texture manager for texturedQuad@1 rendering.
  void setTextureManager(TextureManager* mgr);

  // D42: set event bus for event emission.
  void setEventBus(EventBus* bus);

  // D78: set render style for pane borders and separators.
  void setRenderStyle(const RenderStyle& style);

  // Walk the scene and issue draw calls.
  Stats render(const Scene& scene, GpuBufferManager& gpuBufs, int viewW, int viewH);

  // D29.3: GPU picking — render to offscreen FBO, read pixel at (pickX, pickY).
  PickResult renderPick(const Scene& scene, GpuBufferManager& gpuBufs,
                        int viewW, int viewH, int pickX, int pickY);

private:
  ShaderProgram pos2Prog_;        // triSolid + line2d + points
  ShaderProgram instRectProg_;    // instancedRect@1
  ShaderProgram instCandleProg_;  // instancedCandle@1
  ShaderProgram textSdfProg_;     // textSDF@1
  ShaderProgram lineAAProg_;      // lineAA@1
  ShaderProgram triAAProg_;       // triAA@1
  ShaderProgram triGradientProg_; // triGradient@1
  ShaderProgram texQuadProg_;     // D41: texturedQuad@1
  // ENC-482: device-level GL state (VAO, atlas texture, pick FBO, frame/pass
  // state) now lives behind the GpuDevice seam.
  GlDevice device_;
  TextureHandle atlasTex_{};  // SDF glyph atlas, created/uploaded via device_
  bool inited_{false};

  // ENC-483 (P1.3): pipeline dispatch is keyed through a registry. The Renderer
  // owns the GL backends it registers; the dispatch loop looks up di->pipeline
  // and routes ported pipelines through their IRendererBackend. Un-ported
  // pipelines (the other 9) still fall through to the legacy inline draw helpers
  // below — see the dispatch loop in Renderer::render and TODO(ENC-486..492).
  BackendRegistry backends_;
  GlTriSolidBackend triSolidBackend_;  // triSolid@1 — the first ported backend

  GlyphAtlas* atlas_{nullptr};
  TextureManager* texMgr_{nullptr};
  EventBus* eventBus_{nullptr};

  // Scratch VBO for instanced indexed gather (D26)
  GLuint scratchVbo_{0};
  std::vector<std::uint8_t> scratchData_;

  // D29.3: GPU picking resources
  ShaderProgram pickFlatProg_;       // non-instanced pick
  ShaderProgram pickInstRectProg_;   // instancedRect pick
  ShaderProgram pickInstCandleProg_; // instancedCandle pick
  ShaderProgram pickLineAAProg_;     // lineAA pick
  // ENC-482: the pick FBO/RBO target now lives inside GlDevice (the offscreen
  // RenderTargetHandle); renderPick() drives it via beginRenderPass/readPixel.

  void drawPos2(const DrawItem& di, const Scene& scene,
                GpuBufferManager& gpuBufs, GLenum mode, Stats& stats);
  void drawInstancedRect(const DrawItem& di, const Scene& scene,
                         GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats);
  void drawInstancedCandle(const DrawItem& di, const Scene& scene,
                           GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats);
  void drawTextSdf(const DrawItem& di, const Scene& scene,
                   GpuBufferManager& gpuBufs, Stats& stats);
  void drawLineAA(const DrawItem& di, const Scene& scene,
                  GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats);
  void drawTriAA(const DrawItem& di, const Scene& scene,
                 GpuBufferManager& gpuBufs, Stats& stats);
  void drawTriGradient(const DrawItem& di, const Scene& scene,
                       GpuBufferManager& gpuBufs, Stats& stats);
  void drawTexturedQuad(const DrawItem& di, const Scene& scene,
                        GpuBufferManager& gpuBufs, int viewW, int viewH, Stats& stats);

  void uploadAtlasIfDirty();
  void drawPick(const DrawItem& di, const Scene& scene,
                GpuBufferManager& gpuBufs, int viewW, int viewH,
                float pickR, float pickG, float pickB);

  // D78: pane border/separator rendering
  RenderStyle renderStyle_;
  void drawPaneBorder(const Pane& pane, int viewW, int viewH);
  void drawPaneSeparators(const Scene& scene, int viewW, int viewH);
};

} // namespace dc
