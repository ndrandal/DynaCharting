// ENC-485 (P2.2) — CpuBufferStore implementation (backend-agnostic CPU side).
//
// The CPU bookkeeping here is moved VERBATIM in behaviour from the original
// GL GpuBufferManager (core/src/gl/GpuBufferManager.cpp): same setCpuData /
// reserve / writeRange / addDirtyRange semantics, same full-vs-sub upload
// decision, same UploadStats accounting. Only the actual upload calls differ —
// the GL VBO path stays in GpuBufferManager (overriding uploadDirty()); the
// device-agnostic uploadDirty(GpuDevice&, ...) here routes through GpuDevice for
// Dawn (and any future backend).
#include "dc/render/CpuBufferStore.hpp"

#include <algorithm>
#include <cstring>

namespace dc {

// --- DeviceBufferResolver --------------------------------------------------

DeviceBufferResolver::~DeviceBufferResolver() {
  for (auto& [id, h] : handles_) {
    if (h.valid()) device_.destroyBuffer(h);
  }
}

BufferHandle DeviceBufferResolver::handleFor(Id bufferId) const {
  auto it = handles_.find(bufferId);
  if (it == handles_.end()) return {};
  return it->second;
}

BufferHandle DeviceBufferResolver::ensureCapacity(Id bufferId,
                                                  std::size_t capacityBytes) {
  BufferHandle& h = handles_[bufferId];
  // (Re)create on grow / first use. The CpuBufferStore uploads the bytes right
  // after via GpuDevice::updateBuffer, so create empty (no init data) here.
  if (h.valid()) device_.destroyBuffer(h);
  h = device_.createBuffer(capacityBytes, nullptr, 0);
  return h;
}

void CpuBufferStore::setCpuData(Id bufferId, const void* data,
                                std::uint32_t bytes) {
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

void CpuBufferStore::reserve(Id bufferId, std::uint32_t totalBytes) {
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

void CpuBufferStore::writeRange(Id bufferId, std::uint32_t offset,
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

void CpuBufferStore::addDirtyRange(Entry& e, std::uint32_t offset,
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

std::uint64_t CpuBufferStore::uploadDirty(GpuDevice& device,
                                          BufferHandleResolver& resolver) {
  stats_ = UploadStats{};
  std::uint64_t totalBytes = 0;

  for (auto& [id, e] : entries_) {
    BufferHandle handle = resolver.handleFor(id);
    const bool hasBuffer = handle.valid();
    const bool fullReupload = needsFull(e, hasBuffer);
    const bool hasDirty = !e.dirty.empty() || fullReupload;
    if (!hasDirty) continue;

    if (fullReupload) {
      // (Re)create/grow the device buffer to the current CPU size, then push the
      // whole thing. Mirrors GL's glBufferData(size, data) realloc.
      handle = resolver.ensureCapacity(id, e.cpuData.size());
      if (handle.valid() && !e.cpuData.empty()) {
        device.updateBuffer(handle, e.cpuData.data(), e.cpuData.size());
      }
      e.gpuCapacity = static_cast<std::uint32_t>(e.cpuData.size());
      e.needsFullUpload = false;
      stats_.fullUploads++;
      stats_.bytesUploaded += e.cpuData.size();
      totalBytes += e.cpuData.size();
      e.dirty.clear();
    } else {
      // Partial: one writeBufferRange per coalesced dirty range. Mirrors GL's
      // per-range glBufferSubData.
      for (const auto& r : e.dirty) {
        if (r.offset + r.length > e.cpuData.size()) continue;
        device.writeBufferRange(handle, r.offset, e.cpuData.data() + r.offset,
                                r.length);
        stats_.subUploads++;
        stats_.bytesUploaded += r.length;
        totalBytes += r.length;
      }
      stats_.rangesCoalesced += static_cast<std::uint32_t>(e.dirty.size());
      e.dirty.clear();
    }
  }
  return totalBytes;
}

const std::uint8_t* CpuBufferStore::getCpuData(Id bufferId) const {
  auto it = entries_.find(bufferId);
  if (it == entries_.end() || it->second.cpuData.empty()) return nullptr;
  return it->second.cpuData.data();
}

std::uint32_t CpuBufferStore::getCpuDataSize(Id bufferId) const {
  auto it = entries_.find(bufferId);
  if (it == entries_.end()) return 0;
  return static_cast<std::uint32_t>(it->second.cpuData.size());
}

}  // namespace dc
