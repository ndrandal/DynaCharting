#pragma once
#include "dc/ids/Id.hpp"
#include <glad/gl.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

class GpuBufferManager {
public:
  ~GpuBufferManager();

  // Store CPU-side bytes for a buffer ID.
  void setCpuData(Id bufferId, const void* data, std::uint32_t bytes);

  // Upload any dirty buffers to GL VBOs. Returns total bytes uploaded.
  std::uint64_t uploadDirty();

  // Get the GL buffer name for a given ID (0 if not uploaded yet).
  GLuint getGlBuffer(Id bufferId) const;

private:
  struct Entry {
    std::vector<std::uint8_t> cpuData;
    GLuint vbo{0};
    bool dirty{false};
  };
  std::unordered_map<Id, Entry> entries_;
};

} // namespace dc
