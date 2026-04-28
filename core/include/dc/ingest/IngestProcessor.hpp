#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

// D81: per-write range produced by processBatch. A single record usually
// produces one IngestWrite; when an append triggers a ring-buffer eviction,
// a full-range write is emitted instead so the downstream uploader knows the
// entire buffer shifted.
struct IngestWrite {
  Id bufferId{0};
  std::uint32_t offset{0};
  std::uint32_t length{0};
};

struct IngestResult {
  std::vector<Id> touchedBufferIds;
  std::vector<IngestWrite> writes;   // D81: exact byte ranges mutated
  std::uint32_t payloadBytes{0};
  std::uint32_t droppedBytes{0};
};

class IngestProcessor {
public:
  IngestResult processBatch(const std::uint8_t* data, std::uint32_t len);

  const std::uint8_t* getBufferData(Id id) const;
  std::uint32_t getBufferSize(Id id) const;

  void ensureBuffer(Id id);
  void setBufferData(Id id, const std::uint8_t* data, std::uint32_t len);
  void syncBufferLengths(Scene& scene) const;

  // Cache / eviction (D2.5)
  void setMaxBytes(Id id, std::uint32_t maxBytes);
  std::uint32_t getMaxBytes(Id id) const;
  void evictFront(Id id, std::uint32_t bytes);
  void keepLast(Id id, std::uint32_t bytes);

  static constexpr std::uint32_t DEFAULT_MAX_BYTES = 4 * 1024 * 1024;

private:
  static constexpr std::uint8_t OP_APPEND = 1;
  static constexpr std::uint8_t OP_UPDATE_RANGE = 2;
  static constexpr std::uint32_t HEADER_SIZE = 13; // 1 + 4 + 4 + 4

  struct CpuBuffer {
    Id id{0};
    std::vector<std::uint8_t> data;
    std::uint32_t maxBytes{DEFAULT_MAX_BYTES};
  };

  std::unordered_map<Id, CpuBuffer> buffers_;

  void enforceCap(CpuBuffer& buf);
};

} // namespace dc
