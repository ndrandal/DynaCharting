#pragma once
#include "dc/gl/ShaderProgram.hpp"
#include "dc/gl/GpuBufferManager.hpp"
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
  GLuint vao_{0};
  GLuint atlasTexture_{0};
  bool inited_{false};

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
  GLuint pickFbo_{0};
  GLuint pickRbo_{0};
  int pickW_{0}, pickH_{0};

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
  void applyBlendMode(BlendMode mode);
  void ensurePickFbo(int w, int h);
  void drawPick(const DrawItem& di, const Scene& scene,
                GpuBufferManager& gpuBufs, int viewW, int viewH,
                float pickR, float pickG, float pickB);

  // D78: pane border/separator rendering
  RenderStyle renderStyle_;
  void drawPaneBorder(const Pane& pane, int viewW, int viewH);
  void drawPaneSeparators(const Scene& scene, int viewW, int viewH);
};

} // namespace dc
