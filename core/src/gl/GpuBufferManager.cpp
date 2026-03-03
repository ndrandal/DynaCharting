#include "dc/gl/GpuBufferManager.hpp"
#include <cstring>

namespace dc {

GpuBufferManager::~GpuBufferManager() {
  for (auto& [id, e] : entries_) {
    if (e.vbo) {
      glDeleteBuffers(1, &e.vbo);
    }
  }
}

void GpuBufferManager::setCpuData(Id bufferId, const void* data, std::uint32_t bytes) {
  auto& e = entries_[bufferId];
  if (data && bytes > 0) {
    e.cpuData.resize(bytes);
    std::memcpy(e.cpuData.data(), data, bytes);
  } else {
    e.cpuData.clear();
  }
  e.dirty = true;
}

std::uint64_t GpuBufferManager::uploadDirty() {
  std::uint64_t uploaded = 0;
  for (auto& [id, e] : entries_) {
    if (!e.dirty) continue;
    if (!e.vbo) {
      glGenBuffers(1, &e.vbo);
    }
    glBindBuffer(GL_ARRAY_BUFFER, e.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(e.cpuData.size()),
                 e.cpuData.data(),
                 GL_DYNAMIC_DRAW);
    uploaded += e.cpuData.size();
    e.dirty = false;
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return uploaded;
}

GLuint GpuBufferManager::getGlBuffer(Id bufferId) const {
  auto it = entries_.find(bufferId);
  if (it == entries_.end()) return 0;
  return it->second.vbo;
}

const std::uint8_t* GpuBufferManager::getCpuData(Id bufferId) const {
  auto it = entries_.find(bufferId);
  if (it == entries_.end() || it->second.cpuData.empty()) return nullptr;
  return it->second.cpuData.data();
}

std::uint32_t GpuBufferManager::getCpuDataSize(Id bufferId) const {
  auto it = entries_.find(bufferId);
  if (it == entries_.end()) return 0;
  return static_cast<std::uint32_t>(it->second.cpuData.size());
}

} // namespace dc
