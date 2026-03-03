#include "dc/data/DerivedBuffer.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include <algorithm>

namespace dc {

void DerivedBufferManager::add(Id id, const DerivedBufferConfig& config) {
  Entry e;
  e.config = config;
  configs_[id] = std::move(e);
}

void DerivedBufferManager::remove(Id id) {
  configs_.erase(id);
}

void DerivedBufferManager::setIndices(Id id, const std::vector<std::uint32_t>& indices) {
  auto it = configs_.find(id);
  if (it != configs_.end()) {
    it->second.indices = indices;
  }
}

void DerivedBufferManager::setRange(Id id, std::uint32_t start, std::uint32_t end) {
  auto it = configs_.find(id);
  if (it != configs_.end()) {
    it->second.rangeStart = start;
    it->second.rangeEnd = end;
  }
}

std::vector<Id> DerivedBufferManager::rebuild(
    const std::vector<Id>& touchedSourceIds,
    IngestProcessor& ingest) {
  std::vector<Id> touched;

  for (auto& [id, entry] : configs_) {
    bool sourceWasTouched = false;
    for (Id srcId : touchedSourceIds) {
      if (srcId == entry.config.sourceBufferId) {
        sourceWasTouched = true;
        break;
      }
    }
    if (!sourceWasTouched) continue;

    rebuildOne(entry, ingest);
    touched.push_back(entry.config.outputBufferId);
  }

  return touched;
}

void DerivedBufferManager::rebuildOne(Entry& entry, IngestProcessor& ingest) {
  const auto& cfg = entry.config;
  const std::uint8_t* srcData = ingest.getBufferData(cfg.sourceBufferId);
  std::uint32_t srcSize = ingest.getBufferSize(cfg.sourceBufferId);

  ingest.ensureBuffer(cfg.outputBufferId);

  if (!srcData || srcSize == 0 || cfg.recordStride == 0) {
    ingest.setBufferData(cfg.outputBufferId, nullptr, 0);
    return;
  }

  std::uint32_t totalRecords = srcSize / cfg.recordStride;

  if (cfg.mode == DeriveMode::IndexedFilter) {
    std::vector<std::uint8_t> output;
    output.reserve(entry.indices.size() * cfg.recordStride);
    for (std::uint32_t idx : entry.indices) {
      if (idx < totalRecords) {
        const std::uint8_t* rec = srcData + idx * cfg.recordStride;
        output.insert(output.end(), rec, rec + cfg.recordStride);
      }
    }
    ingest.setBufferData(cfg.outputBufferId, output.data(),
                         static_cast<std::uint32_t>(output.size()));
  } else {
    std::uint32_t start = std::min(entry.rangeStart, totalRecords);
    std::uint32_t end = std::min(entry.rangeEnd, totalRecords);
    if (end <= start) {
      ingest.setBufferData(cfg.outputBufferId, nullptr, 0);
      return;
    }
    std::uint32_t bytes = (end - start) * cfg.recordStride;
    ingest.setBufferData(cfg.outputBufferId,
                         srcData + start * cfg.recordStride, bytes);
  }
}

} // namespace dc
