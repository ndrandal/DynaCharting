// ENC-485 (P2.2) — GpuBufferManager: the GL backend of the streaming buffer
// manager. The CPU-side bookkeeping (storage, dirty coalescing, UploadStats)
// lives in the base CpuBufferStore (pure dc); this file keeps only the GL VBO
// ownership + the glBufferData/glBufferSubData upload, behaviourally identical
// to the pre-ENC-485 monolithic class.
#include "dc/gl/GpuBufferManager.hpp"

namespace dc {

GpuBufferManager::~GpuBufferManager() {
  for (auto& [id, vbo] : vbos_) {
    if (vbo) {
      glDeleteBuffers(1, &vbo);
    }
  }
}

std::uint64_t GpuBufferManager::uploadDirty() {
  stats_ = UploadStats{};
  std::uint64_t totalBytes = 0;

  for (auto& [id, e] : entries_) {
    GLuint& vbo = vbos_[id];
    bool fullReupload = needsFull(e, vbo != 0);
    bool hasDirty = !e.dirty.empty() || fullReupload;
    if (!hasDirty) continue;

    if (!vbo) {
      glGenBuffers(1, &vbo);
    }
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    if (fullReupload) {
      glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(e.cpuData.size()),
                   e.cpuData.empty() ? nullptr : e.cpuData.data(),
                   GL_DYNAMIC_DRAW);
      e.gpuCapacity = static_cast<std::uint32_t>(e.cpuData.size());
      e.needsFullUpload = false;
      stats_.fullUploads++;
      stats_.bytesUploaded += e.cpuData.size();
      totalBytes += e.cpuData.size();
      e.dirty.clear();
    } else {
      for (const auto& r : e.dirty) {
        if (r.offset + r.length > e.cpuData.size()) continue;
        glBufferSubData(GL_ARRAY_BUFFER,
                        static_cast<GLintptr>(r.offset),
                        static_cast<GLsizeiptr>(r.length),
                        e.cpuData.data() + r.offset);
        stats_.subUploads++;
        stats_.bytesUploaded += r.length;
        totalBytes += r.length;
      }
      stats_.rangesCoalesced += static_cast<std::uint32_t>(e.dirty.size());
      e.dirty.clear();
    }
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return totalBytes;
}

GLuint GpuBufferManager::getGlBuffer(Id bufferId) const {
  auto it = vbos_.find(bufferId);
  if (it == vbos_.end()) return 0;
  return it->second;
}

} // namespace dc
