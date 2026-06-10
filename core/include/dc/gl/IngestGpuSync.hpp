#pragma once
// ENC-485: this helper is now backend-AGNOSTIC. It feeds an IngestResult into a
// dc::CpuBufferStore (the shared CPU-side streaming buffer manager), which both
// the GL GpuBufferManager and the Dawn path derive from / use. The include and
// the parameter type moved from the GL GpuBufferManager to CpuBufferStore; GL
// callers passing a GpuBufferManager still work (it is-a CpuBufferStore). The
// file keeps its dc/gl/ path for source compatibility with existing includes.
#include "dc/render/CpuBufferStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"

namespace dc {

// D81: sync an IngestResult to GPU using per-write range uploads.
// Typical call pattern after ingest.processBatch():
//   syncIngestWritesToGpu(result, ingest, gpuBufs);
// Uses reserve() to grow the GPU side to the current CPU size, then
// writeRange() for each IngestWrite so dirty ranges coalesce on uploadDirty().
inline void syncIngestWritesToGpu(const IngestResult& result,
                                   const IngestProcessor& ingest,
                                   CpuBufferStore& gpuBufs) {
  // Grow / shrink GPU side to match CPU, once per touched buffer.
  for (Id bid : result.touchedBufferIds) {
    gpuBufs.reserve(bid, ingest.getBufferSize(bid));
  }
  for (const auto& w : result.writes) {
    const auto* base = ingest.getBufferData(w.bufferId);
    if (!base || w.length == 0) continue;
    gpuBufs.writeRange(w.bufferId, w.offset, base + w.offset, w.length);
  }
}

} // namespace dc
