#include "dc/gl/ShaderProgram.hpp"
#include <cstdio>
#include <vector>

namespace dc {

ShaderProgram::~ShaderProgram() {
  if (program_) {
    glDeleteProgram(program_);
  }
}

static GLuint compileShader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);

  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<std::size_t>(len));
    glGetShaderInfoLog(s, len, nullptr, log.data());
    std::fprintf(stderr, "Shader compile error:\n%s\n", log.data());
    glDeleteShader(s);
    return 0;
  }
  return s;
}

bool ShaderProgram::build(const char* vertSrc, const char* fragSrc) {
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
  if (!vs) return false;

  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
  if (!fs) { glDeleteShader(vs); return false; }

  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);

  // Shaders can be deleted after linking.
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint ok = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint len = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<std::size_t>(len));
    glGetProgramInfoLog(program_, len, nullptr, log.data());
    std::fprintf(stderr, "Program link error:\n%s\n", log.data());
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }
  return true;
}

void ShaderProgram::use() const {
  glUseProgram(program_);
}

GLint ShaderProgram::attribLocation(const char* name) const {
  return glGetAttribLocation(program_, name);
}

GLint ShaderProgram::uniformLocation(const char* name) const {
  return glGetUniformLocation(program_, name);
}

void ShaderProgram::setUniformMat3(GLint loc, const float* data) const {
  glUniformMatrix3fv(loc, 1, GL_FALSE, data);
}

void ShaderProgram::setUniformVec4(GLint loc, float x, float y, float z, float w) const {
  glUniform4f(loc, x, y, z, w);
}

void ShaderProgram::setUniformFloat(GLint loc, float v) const {
  glUniform1f(loc, v);
}

} // namespace dc
