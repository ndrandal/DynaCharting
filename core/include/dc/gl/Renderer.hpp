#pragma once
#include "dc/gl/ShaderProgram.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/debug/Stats.hpp"
#include <glad/gl.h>

namespace dc {

class GlyphAtlas;

class Renderer {
public:
  Renderer() = default;
  ~Renderer();

  // Compile shaders, create VAO. Call once after GL context is current.
  bool init();

  // Optional: set glyph atlas for textSDF@1 rendering.
  void setGlyphAtlas(GlyphAtlas* atlas);

  // Walk the scene and issue draw calls.
  Stats render(const Scene& scene, GpuBufferManager& gpuBufs, int viewW, int viewH);

private:
  ShaderProgram pos2Prog_;        // triSolid + line2d + points
  ShaderProgram instRectProg_;    // instancedRect@1
  ShaderProgram instCandleProg_;  // instancedCandle@1
  ShaderProgram textSdfProg_;     // textSDF@1
  GLuint vao_{0};
  GLuint atlasTexture_{0};
  bool inited_{false};

  GlyphAtlas* atlas_{nullptr};

  void drawPos2(const DrawItem& di, const Scene& scene,
                GpuBufferManager& gpuBufs, GLenum mode, Stats& stats);
  void drawInstancedRect(const DrawItem& di, const Scene& scene,
                         GpuBufferManager& gpuBufs, Stats& stats);
  void drawInstancedCandle(const DrawItem& di, const Scene& scene,
                           GpuBufferManager& gpuBufs, Stats& stats);
  void drawTextSdf(const DrawItem& di, const Scene& scene,
                   GpuBufferManager& gpuBufs, Stats& stats);

  void uploadAtlasIfDirty();
};

} // namespace dc
