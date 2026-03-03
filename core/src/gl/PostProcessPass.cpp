// D47: Post-processing pipeline — fullscreen shader passes
#include "dc/gl/PostProcessPass.hpp"
#include <cstdio>
#include <cstring>

namespace dc {

// ---- Fullscreen quad vertex shader (shared by all post-process passes) ----
static const char* kPostProcessVert = R"GLSL(
#version 330 core
out vec2 v_uv;
void main() {
    // Fullscreen triangle trick: 3 vertices cover [-1,1] x [-1,1]
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_uv = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
}
)GLSL";

// ---- PostProcessPass ----

PostProcessPass::~PostProcessPass() {
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
  if (outputTex_) glDeleteTextures(1, &outputTex_);
  if (quadVao_) glDeleteVertexArrays(1, &quadVao_);
  if (quadVbo_) glDeleteBuffers(1, &quadVbo_);
}

bool PostProcessPass::init(const std::string& fragmentSrc) {
  if (!prog_.build(kPostProcessVert, fragmentSrc.c_str())) {
    std::fprintf(stderr, "PostProcessPass::init: shader build failed\n");
    return false;
  }
  return true;
}

void PostProcessPass::setUniform(const std::string& name, float value) {
  uniforms_[name] = value;
}

void PostProcessPass::ensureQuad() {
  if (quadVao_) return;
  glGenVertexArrays(1, &quadVao_);
}

void PostProcessPass::ensureFbo(int w, int h) {
  if (fbo_ && texW_ == w && texH_ == h) return;

  if (!fbo_) glGenFramebuffers(1, &fbo_);
  if (!outputTex_) glGenTextures(1, &outputTex_);

  glBindTexture(GL_TEXTURE_2D, outputTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTex_, 0);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr, "PostProcessPass::ensureFbo: framebuffer incomplete (0x%X)\n", status);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  texW_ = w;
  texH_ = h;
}

void PostProcessPass::apply(GLuint inputTexture, int w, int h) {
  ensureFbo(w, h);
  ensureQuad();

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, w, h);

  prog_.use();

  // Bind input texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, inputTexture);
  GLint texLoc = prog_.uniformLocation("u_texture");
  if (texLoc >= 0) glUniform1i(texLoc, 0);

  // Set resolution uniform if present
  GLint resLoc = prog_.uniformLocation("u_resolution");
  if (resLoc >= 0) {
    glUniform2f(resLoc, static_cast<float>(w), static_cast<float>(h));
  }

  // Apply custom uniforms
  for (const auto& [name, value] : uniforms_) {
    GLint loc = prog_.uniformLocation(name.c_str());
    if (loc >= 0) {
      glUniform1f(loc, value);
    }
  }

  // Draw fullscreen triangle
  glBindVertexArray(quadVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---- PostProcessStack ----

PostProcessStack::~PostProcessStack() {
  if (sceneTex_) glDeleteTextures(1, &sceneTex_);
  if (sceneDepth_) glDeleteRenderbuffers(1, &sceneDepth_);
  if (sceneFbo_) glDeleteFramebuffers(1, &sceneFbo_);
}

bool PostProcessStack::init(int w, int h) {
  w_ = w;
  h_ = h;

  // Create scene FBO
  glGenFramebuffers(1, &sceneFbo_);
  glGenTextures(1, &sceneTex_);
  glGenRenderbuffers(1, &sceneDepth_);

  glBindTexture(GL_TEXTURE_2D, sceneTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

  glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTex_, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth_);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return status == GL_FRAMEBUFFER_COMPLETE;
}

void PostProcessStack::addPass(const std::string& name, const std::string& fragSrc) {
  PassEntry entry;
  entry.name = name;
  entry.enabled = true;
  entry.pass = std::make_unique<PostProcessPass>();
  entry.pass->init(fragSrc);
  passes_.push_back(std::move(entry));
}

void PostProcessStack::setPassEnabled(const std::string& name, bool enabled) {
  for (auto& entry : passes_) {
    if (entry.name == name) {
      entry.enabled = enabled;
      return;
    }
  }
}

void PostProcessStack::setPassUniform(const std::string& name,
                                       const std::string& uniform, float value) {
  for (auto& entry : passes_) {
    if (entry.name == name) {
      entry.pass->setUniform(uniform, value);
      return;
    }
  }
}

GLuint PostProcessStack::apply(GLuint sceneTexture, int w, int h) {
  GLuint currentTex = sceneTexture;
  for (auto& entry : passes_) {
    if (!entry.enabled) continue;
    entry.pass->apply(currentTex, w, h);
    currentTex = entry.pass->outputTexture();
  }
  return currentTex;
}

std::size_t PostProcessStack::passCount() const {
  return passes_.size();
}

} // namespace dc
