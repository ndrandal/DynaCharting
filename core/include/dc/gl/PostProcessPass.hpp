#pragma once
// D47: Post-processing pipeline — fullscreen shader passes
#include "dc/gl/ShaderProgram.hpp"
#include <glad/gl.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace dc {

class PostProcessPass {
public:
  ~PostProcessPass();

  bool init(const std::string& fragmentSrc);
  void setUniform(const std::string& name, float value);
  void apply(GLuint inputTexture, int w, int h);
  GLuint outputTexture() const { return outputTex_; }

private:
  ShaderProgram prog_;
  GLuint fbo_{0}, outputTex_{0};
  GLuint quadVao_{0}, quadVbo_{0};
  int texW_{0}, texH_{0};
  std::unordered_map<std::string, float> uniforms_;
  void ensureFbo(int w, int h);
  void ensureQuad();
};

class PostProcessStack {
public:
  ~PostProcessStack();
  bool init(int w, int h);
  void addPass(const std::string& name, const std::string& fragSrc);
  void setPassEnabled(const std::string& name, bool enabled);
  void setPassUniform(const std::string& name, const std::string& uniform, float value);
  GLuint apply(GLuint sceneTexture, int w, int h);
  GLuint sceneFbo() const { return sceneFbo_; }
  GLuint sceneTexture() const { return sceneTex_; }
  std::size_t passCount() const;

private:
  struct PassEntry { std::string name; std::unique_ptr<PostProcessPass> pass; bool enabled{true}; };
  std::vector<PassEntry> passes_;
  GLuint sceneFbo_{0}, sceneTex_{0}, sceneDepth_{0};
  int w_{0}, h_{0};
};

} // namespace dc
