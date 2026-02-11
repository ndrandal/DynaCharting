#pragma once
#include <cstdint>
#include <vector>

namespace dc {

class GlContext {
public:
  virtual ~GlContext() = default;

  virtual bool init(int width, int height) = 0;
  virtual void swapBuffers() = 0;

  virtual int width() const = 0;
  virtual int height() const = 0;

  // Read back RGBA pixels from the framebuffer.
  virtual std::vector<std::uint8_t> readPixels() const = 0;
};

} // namespace dc
