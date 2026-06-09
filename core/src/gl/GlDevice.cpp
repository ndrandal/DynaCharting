// ENC-482 (P1.2) — GlDevice implementation. See GlDevice.hpp for scope.
//
// Every GL call here is moved verbatim (same args, same order) from the
// frame/pass-level code that used to live inline in Renderer.cpp, so the
// rendered output is unchanged. The per-pipeline draw helpers are NOT here —
// they stay in Renderer.cpp until ENC-484.
#include "dc/gl/GlDevice.hpp"

namespace dc {

GlDevice::~GlDevice() {
  for (auto& b : buffers_) {
    if (b.vbo) glDeleteBuffers(1, &b.vbo);
  }
  for (auto& t : textures_) {
    if (t.tex) glDeleteTextures(1, &t.tex);
  }
  if (pickRbo_) glDeleteRenderbuffers(1, &pickRbo_);
  if (pickFbo_) glDeleteFramebuffers(1, &pickFbo_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
}

// --- lifecycle -------------------------------------------------------------

bool GlDevice::init() {
  if (inited_) return true;
  glGenVertexArrays(1, &vao_);
  glEnable(GL_PROGRAM_POINT_SIZE);
  inited_ = true;
  return true;
}

// --- buffer resources ------------------------------------------------------

BufferHandle GlDevice::createBuffer(std::size_t capacityBytes,
                                    const void* initData,
                                    std::size_t initBytes) {
  GLuint vbo = 0;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(capacityBytes),
               nullptr, GL_STREAM_DRAW);
  if (initData && initBytes > 0) {
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(initBytes), initData);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  buffers_.push_back({vbo, capacityBytes});
  BufferHandle h;
  h.id = static_cast<std::uint32_t>(buffers_.size());  // 1-based
  return h;
}

void GlDevice::updateBuffer(BufferHandle buf, const void* data,
                            std::size_t bytes) {
  if (!buf.valid() || buf.id > buffers_.size()) return;
  BufferEntry& e = buffers_[buf.id - 1];
  glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
  if (bytes > e.capacity) {
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(bytes),
                 data, GL_STREAM_DRAW);
    e.capacity = bytes;
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), data);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlDevice::writeBufferRange(BufferHandle buf, std::size_t offsetBytes,
                                const void* data, std::size_t bytes) {
  if (!buf.valid() || buf.id > buffers_.size()) return;
  BufferEntry& e = buffers_[buf.id - 1];
  glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
  glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offsetBytes),
                  static_cast<GLsizeiptr>(bytes), data);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GlDevice::destroyBuffer(BufferHandle buf) {
  if (!buf.valid() || buf.id > buffers_.size()) return;
  BufferEntry& e = buffers_[buf.id - 1];
  if (e.vbo) {
    glDeleteBuffers(1, &e.vbo);
    e.vbo = 0;
    e.capacity = 0;
  }
}

// --- texture resources -----------------------------------------------------

TextureHandle GlDevice::createTexture(const TextureDesc& desc) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  const GLint filter = (desc.filter == TextureFilter::Nearest)
                           ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (desc.format == TextureFormat::R8) {
    // Single-channel coverage (SDF glyph atlas). Matches the old
    // Renderer::uploadAtlasIfDirty pixel-store + R8/RED path exactly.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 static_cast<GLsizei>(desc.width),
                 static_cast<GLsizei>(desc.height), 0,
                 GL_RED, GL_UNSIGNED_BYTE, desc.data);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(desc.width),
                 static_cast<GLsizei>(desc.height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, desc.data);
  }
  glBindTexture(GL_TEXTURE_2D, 0);

  textures_.push_back({tex, desc.width, desc.height, desc.format});
  TextureHandle h;
  h.id = static_cast<std::uint32_t>(textures_.size());  // 1-based
  return h;
}

void GlDevice::updateTexture(TextureHandle tex, const std::uint8_t* data) {
  if (!tex.valid() || tex.id > textures_.size()) return;
  TextureEntry& e = textures_[tex.id - 1];
  glBindTexture(GL_TEXTURE_2D, e.tex);
  // Preserve the old atlas behaviour: a full glTexImage2D re-upload (with the
  // same filter/wrap params re-applied) rather than glTexSubImage2D.
  if (e.format == TextureFormat::R8) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 static_cast<GLsizei>(e.width),
                 static_cast<GLsizei>(e.height), 0,
                 GL_RED, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(e.width),
                 static_cast<GLsizei>(e.height), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

void GlDevice::destroyTexture(TextureHandle tex) {
  if (!tex.valid() || tex.id > textures_.size()) return;
  TextureEntry& e = textures_[tex.id - 1];
  if (e.tex) {
    glDeleteTextures(1, &e.tex);
    e.tex = 0;
  }
}

GLuint GlDevice::glTexture(TextureHandle tex) const {
  if (!tex.valid() || tex.id > textures_.size()) return 0;
  return textures_[tex.id - 1].tex;
}

// --- pipelines & bind groups (ENC-484 stubs) -------------------------------
// The GL pipelines are still the ShaderPrograms owned by Renderer, and the
// draw helpers bind buffers/uniforms directly. These are no-ops until those
// helpers migrate onto IRendererBackend (ENC-484).

PipelineHandle GlDevice::createPipeline(const PipelineDesc&) { return {}; }
void GlDevice::destroyPipeline(PipelineHandle) {}
BindGroupHandle GlDevice::createBindGroup(const BindGroupDesc&) { return {}; }
void GlDevice::destroyBindGroup(BindGroupHandle) {}
void GlDevice::bindPipeline(PipelineHandle) {}
DeviceDrawStats GlDevice::draw(BindGroupHandle, const DrawParams&) { return {}; }
DeviceDrawStats GlDevice::drawInstanced(BindGroupHandle,
                                        const DrawInstancedParams&) { return {}; }

// --- render pass -----------------------------------------------------------

void GlDevice::ensurePickTarget(int w, int h) {
  if (pickFbo_ && pickW_ == w && pickH_ == h) return;

  if (pickRbo_) glDeleteRenderbuffers(1, &pickRbo_);
  if (pickFbo_) glDeleteFramebuffers(1, &pickFbo_);

  glGenFramebuffers(1, &pickFbo_);
  glGenRenderbuffers(1, &pickRbo_);

  glBindRenderbuffer(GL_RENDERBUFFER, pickRbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);

  glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_RENDERBUFFER, pickRbo_);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  pickW_ = w;
  pickH_ = h;
}

void GlDevice::beginRenderPass(const RenderPassDesc& desc) {
  const int w = static_cast<int>(desc.viewportWidth);
  const int h = static_cast<int>(desc.viewportHeight);

  // Bind target: id 0 == backbuffer, id 1 == offscreen pick FBO.
  inPickPass_ = desc.target.valid();
  if (inPickPass_) {
    ensurePickTarget(w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, pickFbo_);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  glViewport(0, 0, w, h);

  if (desc.clear) {
    glClearColor(desc.clearColor[0], desc.clearColor[1],
                 desc.clearColor[2], desc.clearColor[3]);
    GLbitfield mask = GL_COLOR_BUFFER_BIT;
    if (desc.clearStencil) mask |= GL_STENCIL_BUFFER_BIT;
    glClear(mask);
  }

  glBindVertexArray(vao_);
}

void GlDevice::endRenderPass() {
  glBindVertexArray(0);
  if (inPickPass_) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    inPickPass_ = false;
  } else {
    glFlush();
  }
}

void GlDevice::setViewport(std::uint32_t width, std::uint32_t height) {
  glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void GlDevice::setScissorRect(const ScissorRect& rect) {
  glScissor(rect.x, rect.y, rect.width, rect.height);
}

void GlDevice::setScissorTestEnabled(bool enabled) {
  if (enabled) glEnable(GL_SCISSOR_TEST);
  else         glDisable(GL_SCISSOR_TEST);
}

// --- per-draw mutable state ------------------------------------------------

void GlDevice::setBlendMode(DeviceBlendMode mode) {
  switch (mode) {
    case DeviceBlendMode::Normal:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case DeviceBlendMode::Additive:
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
      break;
    case DeviceBlendMode::Multiply:
      glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      break;
    case DeviceBlendMode::Screen:
      glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_COLOR, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      break;
  }
}

void GlDevice::setClipState(ClipMode mode) {
  switch (mode) {
    case ClipMode::None:
      glDisable(GL_STENCIL_TEST);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      break;
    case ClipMode::WriteMask:
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_ALWAYS, 1, 0xFF);
      glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      break;
    case ClipMode::UseMask:
      glEnable(GL_STENCIL_TEST);
      glStencilFunc(GL_EQUAL, 1, 0xFF);
      glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
      glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      break;
  }
}

void GlDevice::restoreColorMask() {
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

// --- readback --------------------------------------------------------------

void GlDevice::readPixel(std::int32_t x, std::int32_t y,
                         std::uint8_t* outRgba) {
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outRgba);
}

}  // namespace dc
