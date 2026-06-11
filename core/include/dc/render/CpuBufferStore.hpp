// ENC-485 (P2.2) — CpuBufferStore: the backend-AGNOSTIC CPU-side of the
// streaming buffer manager.
//
// HISTORY / WHY
// -------------
// This is the device-neutral half of what used to live entirely in
// core/include/dc/gl/GpuBufferManager.hpp. That class interleaved two concerns:
//   (a) per-id CPU byte storage + dirty-range coalescing + UploadStats — pure
//       bookkeeping, no graphics API involved; and
//   (b) the actual GPU upload (glBufferData / glBufferSubData on a VBO) — GL.
//
// ENC-485 splits (a) out into THIS class, which lives in the pure `dc` core and
// includes NO GL/GLAD or Dawn/WebGPU headers. The GL backend (`GpuBufferManager`
// in dc_gl) now derives from CpuBufferStore and keeps only the GL VBO + the GL
// upload loop; the Dawn backend (dc_gpu) uses CpuBufferStore directly with a
// `GpuDevice` upload, so dc_gpu no longer needs the GL header. The coalescing /
// dirty-range / stats logic is shared by both, written exactly once here.
//
// UPLOAD MODEL
// ------------
// Callers stream data in with reserve()/writeRange() (incremental tail-appends,
// the live-tick hot path) or setCpuData() (full replace). Adjacent/overlapping
// writeRange()s coalesce into one dirty range. The upload is then driven by a
// backend:
//   * GL:   GpuBufferManager::uploadDirty() walks the dirty ranges and issues
//           glBufferData (full) / glBufferSubData (per coalesced range).
//   * Dawn: uploadDirty(GpuDevice&, BufferHandleResolver&) routes the same
//           full/sub decisions through GpuDevice::updateBuffer /
//           writeBufferRange (queue.writeBuffer under the hood).
// Both paths produce identical UploadStats so behaviour is verifiable from a
// test without a graphics API (the coalescing is asserted on the stats).
#pragma once

#include "dc/ids/Id.hpp"
#include "dc/render/GpuDevice.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

// Resolves a logical buffer Id to the device BufferHandle that backs it, lazily
// (re)creating the device buffer at the required capacity. Used by the Dawn
// upload path: CpuBufferStore owns the CPU bytes + dirty tracking but not the
// device handles, so it asks the resolver to (a) hand back the current handle
// for a partial upload, or (b) (re)create a buffer of `capacityBytes` when a
// full (re)upload / grow is needed. The GL backend overrides uploadDirty()
// directly and does not use this seam.
class BufferHandleResolver {
 public:
  virtual ~BufferHandleResolver() = default;

  // Return the device buffer currently backing `bufferId`, or an invalid handle
  // if none exists yet.
  virtual BufferHandle handleFor(Id bufferId) const = 0;

  // (Re)create the device buffer for `bufferId` with at least `capacityBytes`
  // capacity and return its handle. Called on a full (re)upload or a grow past
  // capacity. The previous handle (if any) may be destroyed by the resolver.
  virtual BufferHandle ensureCapacity(Id bufferId,
                                      std::size_t capacityBytes) = 0;
};

// A ready-made BufferHandleResolver over any GpuDevice: it owns an id->handle
// map and creates/recreates device buffers via GpuDevice::createBuffer /
// destroyBuffer. This is the generic bridge the Dawn streaming path uses (and
// is reusable by the future instanced pipelines, ENC-488/489/490, which also
// stream per-instance buffers through CpuBufferStore). Pure dc: no GL/Dawn
// types — it only ever sees GpuDevice + opaque BufferHandle.
class DeviceBufferResolver : public BufferHandleResolver {
 public:
  explicit DeviceBufferResolver(GpuDevice& device) : device_(device) {}
  ~DeviceBufferResolver() override;

  BufferHandle handleFor(Id bufferId) const override;
  BufferHandle ensureCapacity(Id bufferId, std::size_t capacityBytes) override;

 private:
  GpuDevice& device_;
  std::unordered_map<Id, BufferHandle> handles_;
};

class CpuBufferStore {
 public:
  // D81: upload statistics for the last uploadDirty() invocation. Useful for
  // tests asserting that incremental writes don't devolve to full rewrites.
  // Identical fields/semantics to the original GL UploadStats:
  //   fullUploads    — full (re)allocating uploads (GL glBufferData / Dawn
  //                    updateBuffer): the buffer was created or grew.
  //   subUploads     — partial uploads, one per coalesced dirty range (GL
  //                    glBufferSubData / Dawn writeBufferRange).
  //   rangesCoalesced— total dirty ranges merged into the subUploads this call.
  //   bytesUploaded  — total bytes pushed to the device this call.
  struct UploadStats {
    std::uint32_t fullUploads{0};
    std::uint32_t subUploads{0};
    std::uint32_t rangesCoalesced{0};
    std::uint64_t bytesUploaded{0};
  };

  virtual ~CpuBufferStore() = default;

  // Full-buffer replace. If the new size differs from the current GPU capacity,
  // triggers a full (re)upload on next uploadDirty(); otherwise degrades to a
  // single full-range partial upload.
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

  // Device-agnostic upload: push dirty buffers to the device via `resolver`
  // (ensureCapacity for full/grown buffers, handleFor for partial ranges) and
  // GpuDevice (updateBuffer for full, writeBufferRange per coalesced range).
  // Returns total bytes uploaded. The Dawn backend uses this; the GL backend
  // overrides with its own VBO-based uploadDirty().
  std::uint64_t uploadDirty(GpuDevice& device, BufferHandleResolver& resolver);

  // Stats from the most recent uploadDirty() call (either backend).
  const UploadStats& lastUploadStats() const { return stats_; }

  // CPU-side reads (backend-agnostic; used by Dawn's static upload path and by
  // the GL renderer for index/vertex CPU copies).
  const std::uint8_t* getCpuData(Id bufferId) const;
  std::uint32_t getCpuDataSize(Id bufferId) const;

  // ENC-558: monotonically increasing per-buffer version. Bumps on EVERY CPU
  // mutation (setCpuData / reserve / writeRange), regardless of whether the size
  // changed. A backend that caches a derived GPU buffer (e.g. the instanced
  // candle/rect gather buffers) can stash the version it last built from and
  // cheaply detect "the CPU bytes changed since I last looked" — including
  // same-size in-place edits that a size check alone would miss — to know when to
  // re-upload/grow. Returns 0 for an unknown buffer id (and the first real
  // mutation produces version 1, so a cached value of 0 always looks stale).
  std::uint64_t getCpuDataVersion(Id bufferId) const;

 protected:
  struct DirtyRange {
    std::uint32_t offset{0};
    std::uint32_t length{0};
  };
  struct Entry {
    std::vector<std::uint8_t> cpuData;
    std::uint32_t gpuCapacity{0};
    bool needsFullUpload{false};
    std::vector<DirtyRange> dirty;     // sorted ascending, non-overlapping
    std::uint64_t version{0};          // ENC-558: bumps on every CPU mutation
  };

  // Coalesce [offset, offset+length) into e.dirty (merging adjacent/overlapping
  // ranges). Shared by every backend.
  static void addDirtyRange(Entry& e, std::uint32_t offset,
                            std::uint32_t length);

  // Decide whether `e` needs a full (re)upload this tick. `hasDeviceBuffer` is
  // true if the backend already holds a live device buffer for this id (a GL
  // VBO != 0, or a valid Dawn handle): a missing buffer always forces full.
  static bool needsFull(const Entry& e, bool hasDeviceBuffer) {
    return e.needsFullUpload || !hasDeviceBuffer ||
           e.cpuData.size() > e.gpuCapacity;
  }

  std::unordered_map<Id, Entry> entries_;
  UploadStats stats_{};
};

}  // namespace dc
