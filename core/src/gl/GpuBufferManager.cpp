#include "dc/gl/GpuBufferManager.hpp"
#include <algorithm>
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
  if (!data || bytes == 0) {
    e.cpuData.clear();
    e.dirty.clear();
    e.needsFullUpload = true;
    return;
  }
  if (e.cpuData.size() != bytes) {
    // Size change forces a full reupload on next uploadDirty().
    e.cpuData.resize(bytes);
    std::memcpy(e.cpuData.data(), data, bytes);
    e.dirty.clear();
    e.needsFullUpload = true;
  } else {
    // Same size: degrade to a single full-range sub-upload.
    std::memcpy(e.cpuData.data(), data, bytes);
    e.dirty.clear();
    addDirtyRange(e, 0, bytes);
  }
}

void GpuBufferManager::reserve(Id bufferId, std::uint32_t totalBytes) {
  auto& e = entries_[bufferId];
  if (totalBytes < e.cpuData.size()) {
    // Shrink: force full reupload, discard any pending dirty ranges.
    e.cpuData.resize(totalBytes);
    e.dirty.clear();
    e.needsFullUpload = true;
  } else if (totalBytes > e.cpuData.size()) {
    // Grow: zero-fill tail; writeRange callers will overwrite as needed.
    e.cpuData.resize(totalBytes, 0);
    // Don't mark dirty — reserve() is a capacity hint only.
  }
}

void GpuBufferManager::writeRange(Id bufferId, std::uint32_t offset,
                                   const void* data, std::uint32_t bytes) {
  if (bytes == 0) return;
  auto& e = entries_[bufferId];
  std::uint32_t needed = offset + bytes;
  if (needed > e.cpuData.size()) {
    e.cpuData.resize(needed, 0);
  }
  if (data) {
    std::memcpy(e.cpuData.data() + offset, data, bytes);
  }
  addDirtyRange(e, offset, bytes);
}

void GpuBufferManager::addDirtyRange(Entry& e,
                                     std::uint32_t offset,
                                     std::uint32_t length) {
  if (length == 0) return;
  DirtyRange incoming{offset, length};
  std::uint32_t start = incoming.offset;
  std::uint32_t endPos = incoming.offset + incoming.length;

  // Find overlapping/adjacent ranges; merge.
  auto it = e.dirty.begin();
  while (it != e.dirty.end()) {
    std::uint32_t rStart = it->offset;
    std::uint32_t rEnd = it->offset + it->length;
    if (rEnd < start) { ++it; continue; }
    if (rStart > endPos) break;           // past the merge zone
    // Merge with incoming.
    start = std::min(start, rStart);
    endPos = std::max(endPos, rEnd);
    it = e.dirty.erase(it);
  }
  DirtyRange merged{start, endPos - start};
  // Insert sorted by offset.
  auto insertIt = std::lower_bound(
      e.dirty.begin(), e.dirty.end(), merged,
      [](const DirtyRange& a, const DirtyRange& b) { return a.offset < b.offset; });
  e.dirty.insert(insertIt, merged);
}

std::uint64_t GpuBufferManager::uploadDirty() {
  stats_ = UploadStats{};
  std::uint64_t totalBytes = 0;

  for (auto& [id, e] : entries_) {
    bool fullReupload = e.needsFullUpload
                        || e.vbo == 0
                        || e.cpuData.size() > e.gpuCapacity;
    bool hasDirty = !e.dirty.empty() || fullReupload;
    if (!hasDirty) continue;

    if (!e.vbo) {
      glGenBuffers(1, &e.vbo);
    }
    glBindBuffer(GL_ARRAY_BUFFER, e.vbo);

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
