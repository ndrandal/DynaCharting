// ENC-482 (P1.2) — GlDevice: the OpenGL implementation of the GpuDevice seam.
//
// This is the concrete `GpuDevice` over OpenGL 3.3. It owns the device-level
// (frame / render-pass) GL state that the Renderer previously poked directly:
//   * the shared VAO + GL_PROGRAM_POINT_SIZE setup (init)
//   * viewport / clear / per-pane scissor (beginRenderPass / setViewport /
//     setScissorRect)
//   * blend-mode application (setBlendMode -> glBlendFunc*)
//   * the two-pass stencil clip state (setClipState -> glStencil*/glColorMask)
//   * the offscreen pick FBO/RBO target and glReadPixels (createTexture for the
//     pick target is emulated via the FBO path; beginRenderPass binds it)
//   * buffer / texture resources (buffers delegate to a GL VBO it owns;
//     textures wrap glGenTextures + glTexImage2D, used for the SDF glyph atlas)
//
// SCOPE NOTE (ENC-482): the 10 per-pipeline draw helpers in Renderer.cpp
// (drawPos2, drawInstancedRect/Candle, drawTextSdf, drawLineAA, drawTriAA/
// Gradient, drawTexturedQuad) and the pick draw variants stay inline and keep
// calling GL directly for now — they are abstracted in ENC-484+. As a result
// createPipeline / createBindGroup / bindPipeline / draw / drawInstanced are
// implemented as minimal stubs here (the GL pipelines are still the
// ShaderPrograms owned by Renderer). They will be filled in when the draw
// helpers migrate onto IRendererBackend (ENC-484).
#pragma once

#include "dc/render/GpuDevice.hpp"
#include <glad/gl.h>
#include <cstdint>
#include <vector>

namespace dc {

class GlDevice final : public GpuDevice {
public:
  GlDevice() = default;
  ~GlDevice() override;

  // --- lifecycle ----------------------------------------------------------
  bool init() override;

  // --- buffer resources ---------------------------------------------------
  BufferHandle createBuffer(std::size_t capacityBytes, const void* initData,
                            std::size_t initBytes) override;
  void updateBuffer(BufferHandle buf, const void* data,
                    std::size_t bytes) override;
  void writeBufferRange(BufferHandle buf, std::size_t offsetBytes,
                        const void* data, std::size_t bytes) override;
  void destroyBuffer(BufferHandle buf) override;

  // --- texture resources --------------------------------------------------
  TextureHandle createTexture(const TextureDesc& desc) override;
  void updateTexture(TextureHandle tex, const std::uint8_t* data) override;
  void destroyTexture(TextureHandle tex) override;

  // --- pipelines & bind groups (ENC-484 stubs) ----------------------------
  PipelineHandle createPipeline(const PipelineDesc& desc) override;
  void destroyPipeline(PipelineHandle pipe) override;
  BindGroupHandle createBindGroup(const BindGroupDesc& desc) override;
  void destroyBindGroup(BindGroupHandle group) override;

  // --- render pass --------------------------------------------------------
  void beginRenderPass(const RenderPassDesc& desc) override;
  void endRenderPass() override;
  void setViewport(std::uint32_t width, std::uint32_t height) override;
  void setScissorRect(const ScissorRect& rect) override;

  // --- per-draw mutable state (transition affordance) ---------------------
  void setBlendMode(DeviceBlendMode mode) override;
  void setClipState(ClipMode mode) override;

  // --- draw (ENC-484 stubs) -----------------------------------------------
  void bindPipeline(PipelineHandle pipe) override;
  DeviceDrawStats draw(BindGroupHandle group,
                       const DrawParams& params) override;
  DeviceDrawStats drawInstanced(BindGroupHandle group,
                                const DrawInstancedParams& params) override;

  // --- readback -----------------------------------------------------------
  void readPixel(std::int32_t x, std::int32_t y,
                 std::uint8_t* outRgba) override;

  // --- GL-specific transition affordances (ENC-482) -----------------------
  // These let the not-yet-migrated draw helpers in Renderer.cpp keep working
  // against the GL objects this device owns, without exposing GL through the
  // device-agnostic GpuDevice base. Removed once the draw helpers migrate to
  // bind groups / pipelines (ENC-484).

  /// The shared VAO this device created in init(). Bound at pass scope.
  GLuint vao() const { return vao_; }

  /// Raw GL texture name behind a TextureHandle (for glBindTexture in the
  /// textSDF draw helper until it routes through a bind group). 0 if invalid.
  GLuint glTexture(TextureHandle tex) const;

  /// Enable/disable scissor test across the active pass. GL keeps this as a
  /// global flag spanning all per-pane setScissorRect calls.
  void setScissorTestEnabled(bool enabled);

  /// Restore the full color write mask (used after a clip-source draw, which
  /// masked color off via setClipState(WriteMask)).
  void restoreColorMask();

private:
  struct BufferEntry {
    GLuint vbo{0};
    std::size_t capacity{0};
  };
  struct TextureEntry {
    GLuint tex{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    TextureFormat format{TextureFormat::RGBA8};
  };

  static GLenum glBlendNeedsSeparate(DeviceBlendMode mode);

  // Ensure the offscreen pick target FBO/RBO exists at (w,h). Mirrors the old
  // Renderer::ensurePickFbo. Used by beginRenderPass when target.id != 0.
  void ensurePickTarget(int w, int h);

  bool inited_{false};
  GLuint vao_{0};

  std::vector<BufferEntry> buffers_;    // index = handle.id - 1
  std::vector<TextureEntry> textures_;  // index = handle.id - 1

  // Offscreen pick render target (D29.3). RenderTargetHandle id 1 == pick.
  GLuint pickFbo_{0};
  GLuint pickRbo_{0};
  int pickW_{0};
  int pickH_{0};
  bool inPickPass_{false};
};

}  // namespace dc
