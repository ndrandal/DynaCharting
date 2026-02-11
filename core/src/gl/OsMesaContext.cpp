#include "dc/gl/OsMesaContext.hpp"
#include <cstdio>

namespace dc {

OsMesaContext::OsMesaContext() = default;

OsMesaContext::~OsMesaContext() {
  if (ctx_) {
    OSMesaDestroyContext(ctx_);
  }
}

bool OsMesaContext::init(int width, int height) {
  static const int attribs[] = {
    OSMESA_FORMAT,            OSMESA_RGBA,
    OSMESA_DEPTH_BITS,        24,
    OSMESA_STENCIL_BITS,      8,
    OSMESA_PROFILE,           OSMESA_CORE_PROFILE,
    OSMESA_CONTEXT_MAJOR_VERSION, 3,
    OSMESA_CONTEXT_MINOR_VERSION, 3,
    0
  };

  ctx_ = OSMesaCreateContextAttribs(attribs, nullptr);
  if (!ctx_) {
    std::fprintf(stderr, "OsMesaContext: OSMesaCreateContextAttribs failed\n");
    return false;
  }

  width_  = width;
  height_ = height;
  framebuf_.resize(static_cast<std::size_t>(width) * height * 4, 0);

  if (!OSMesaMakeCurrent(ctx_, framebuf_.data(), GL_UNSIGNED_BYTE, width, height)) {
    std::fprintf(stderr, "OsMesaContext: OSMesaMakeCurrent failed\n");
    OSMesaDestroyContext(ctx_);
    ctx_ = nullptr;
    return false;
  }

  // Load GL function pointers via GLAD, using OSMesa's loader.
  int version = gladLoadGL((GLADloadfunc)OSMesaGetProcAddress);
  if (!version) {
    std::fprintf(stderr, "OsMesaContext: gladLoadGL failed\n");
    OSMesaDestroyContext(ctx_);
    ctx_ = nullptr;
    return false;
  }

  return true;
}

void OsMesaContext::swapBuffers() {
  glFinish();
}

std::vector<std::uint8_t> OsMesaContext::readPixels() const {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width_) * height_ * 4);
  glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

} // namespace dc
