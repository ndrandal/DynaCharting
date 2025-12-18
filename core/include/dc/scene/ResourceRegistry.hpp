#pragma once
#include "dc/scene/Types.hpp"
#include <unordered_map>
#include <atomic>
#include <vector>

namespace dc {

// Tracks existence + kind for IDs (and generates IDs if client doesn’t provide one).
class ResourceRegistry {
public:
  ResourceRegistry();

  Id allocate(ResourceKind kind);               // auto-id
  bool reserve(Id id, ResourceKind kind);       // client-provided id (fails if taken)

  bool exists(Id id) const;
  ResourceKind kindOf(Id id) const;

  bool release(Id id);                          // removes from registry

  std::vector<Id> list(ResourceKind kind) const;

private:
  std::atomic<Id> next_{1};
  std::unordered_map<Id, ResourceKind> kinds_;
};

} // namespace dc
