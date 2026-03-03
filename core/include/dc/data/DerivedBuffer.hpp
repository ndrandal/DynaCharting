#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

class IngestProcessor;

enum class DeriveMode : std::uint8_t {
  IndexedFilter,  // output = source[indices[i]]
  Range           // output = source[start..end]
};

struct DerivedBufferConfig {
  Id sourceBufferId{0};
  Id outputBufferId{0};
  std::uint32_t recordStride{0};
  DeriveMode mode{DeriveMode::IndexedFilter};
};

class DerivedBufferManager {
public:
  void add(Id id, const DerivedBufferConfig& config);
  void remove(Id id);

  void setIndices(Id id, const std::vector<std::uint32_t>& indices);
  void setRange(Id id, std::uint32_t start, std::uint32_t end);

  std::vector<Id> rebuild(const std::vector<Id>& touchedSourceIds,
                          IngestProcessor& ingest);

  std::size_t count() const { return configs_.size(); }

private:
  struct Entry {
    DerivedBufferConfig config;
    std::vector<std::uint32_t> indices;
    std::uint32_t rangeStart{0};
    std::uint32_t rangeEnd{0};
  };

  std::unordered_map<Id, Entry> configs_;

  void rebuildOne(Entry& entry, IngestProcessor& ingest);
};

} // namespace dc
