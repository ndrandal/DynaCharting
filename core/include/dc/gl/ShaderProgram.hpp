#pragma once
#include <glad/gl.h>
#include <string>

namespace dc {

class ShaderProgram {
public:
  ShaderProgram() = default;
  ~ShaderProgram();

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;

  // Compile and link. Returns false on failure (errors go to stderr).
  bool build(const char* vertSrc, const char* fragSrc);

  void use() const;

  GLint attribLocation(const char* name) const;
  GLint uniformLocation(const char* name) const;

  void setUniformMat3(GLint loc, const float* data) const;
  void setUniformVec4(GLint loc, float x, float y, float z, float w) const;
  void setUniformFloat(GLint loc, float v) const;

  GLuint id() const { return program_; }

private:
  GLuint program_{0};
};

} // namespace dc
