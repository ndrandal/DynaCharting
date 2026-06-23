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

  // Custom copy (std::atomic is not copyable by default)
  ResourceRegistry(const ResourceRegistry& other);
  ResourceRegistry& operator=(const ResourceRegistry& other);

  // std::atomic is not movable either, so the move ops can't be defaulted
  // (that is ill-formed under a strict libc++ — the Emscripten/wasm build).
  // Mirror the copy ops: load the counter, move the map.
  ResourceRegistry(ResourceRegistry&& other) noexcept;
  ResourceRegistry& operator=(ResourceRegistry&& other) noexcept;

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
