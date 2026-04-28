#pragma once
#include "dc/ids/Id.hpp"
#include <glad/gl.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

class GpuBufferManager {
public:
  // D81: upload statistics for the last uploadDirty() invocation. Useful for
  // tests asserting that incremental writes don't devolve to full rewrites.
  struct UploadStats {
    std::uint32_t fullUploads{0};      // glBufferData calls
    std::uint32_t subUploads{0};       // glBufferSubData calls
    std::uint32_t rangesCoalesced{0};  // total dirty ranges merged into subUploads
    std::uint64_t bytesUploaded{0};
  };

  ~GpuBufferManager();

  // Full-buffer replace. If the new size differs from the current GPU capacity,
  // triggers a full glBufferData on next uploadDirty(); otherwise degrades to a
  // single full-range glBufferSubData.
  void setCpuData(Id bufferId, const void* data, std::uint32_t bytes);

  // D81: reserve/grow the CPU buffer to `totalBytes` without marking dirty.
  // Used by callers that will follow up with writeRange() for exact regions.
  // If `totalBytes` is smaller than the current CPU size the buffer shrinks
  // and a full reupload is forced on next uploadDirty().
  void reserve(Id bufferId, std::uint32_t totalBytes);

  // D81: write `bytes` at `offset`, marking [offset, offset+bytes) dirty.
  // Grows the CPU buffer if needed (zero-fills any gap). Coalesces with
  // adjacent / overlapping pending dirty ranges. Typical live-tick workloads
  // (append at tail) collapse to a single dirty range.
  void writeRange(Id bufferId, std::uint32_t offset,
                  const void* data, std::uint32_t bytes);

  // Upload any dirty buffers to GL VBOs. Returns total bytes uploaded.
  std::uint64_t uploadDirty();

  // Stats from the most recent uploadDirty() call.
  const UploadStats& lastUploadStats() const { return stats_; }

  GLuint getGlBuffer(Id bufferId) const;
  const std::uint8_t* getCpuData(Id bufferId) const;
  std::uint32_t getCpuDataSize(Id bufferId) const;

private:
  struct DirtyRange {
    std::uint32_t offset{0};
    std::uint32_t length{0};
  };
  struct Entry {
    std::vector<std::uint8_t> cpuData;
    GLuint vbo{0};
    std::uint32_t gpuCapacity{0};
    bool needsFullUpload{false};
    std::vector<DirtyRange> dirty;     // sorted ascending, non-overlapping
  };

  void addDirtyRange(Entry& e, std::uint32_t offset, std::uint32_t length);

  std::unordered_map<Id, Entry> entries_;
  UploadStats stats_{};
};

} // namespace dc
