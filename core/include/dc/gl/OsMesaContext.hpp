#pragma once
#include "dc/gl/GlContext.hpp"
#include <glad/gl.h>    // GLAD must precede osmesa.h (guards GL/gl.h)

// OSMesa header uses GLAPI and APIENTRY from GL/gl.h, which GLAD suppresses.
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef GLAPI
#define GLAPI extern
#endif

#include <GL/osmesa.h>
#include <vector>

namespace dc {

class OsMesaContext : public GlContext {
public:
  OsMesaContext();
  ~OsMesaContext() override;

  OsMesaContext(const OsMesaContext&) = delete;
  OsMesaContext& operator=(const OsMesaContext&) = delete;

  bool init(int width, int height) override;
  void swapBuffers() override;

  int width() const override { return width_; }
  int height() const override { return height_; }

  std::vector<std::uint8_t> readPixels() const override;

private:
  OSMesaContext ctx_{nullptr};
  int width_{0};
  int height_{0};
  std::vector<std::uint8_t> framebuf_;
};

} // namespace dc
