#include "dc/scene/ResourceRegistry.hpp"
#include <stdexcept>

namespace dc {

ResourceRegistry::ResourceRegistry() = default;

Id ResourceRegistry::allocate(ResourceKind kind) {
  // NOTE: not lock-free due to unordered_map; D1.1 is single-threaded test harness.
  // Later: shard/lock or use a flat_hash_map with mutex.
  for (;;) {
    Id id = next_.fetch_add(1, std::memory_order_relaxed);
    if (id == kInvalidId) continue;
    if (kinds_.find(id) != kinds_.end()) continue;
    kinds_[id] = kind;
    return id;
  }
}

bool ResourceRegistry::reserve(Id id, ResourceKind kind) {
  if (id == kInvalidId) return false;
  auto [it, inserted] = kinds_.emplace(id, kind);
  return inserted;
}

bool ResourceRegistry::exists(Id id) const {
  return kinds_.find(id) != kinds_.end();
}

ResourceKind ResourceRegistry::kindOf(Id id) const {
  auto it = kinds_.find(id);
  if (it == kinds_.end()) throw std::runtime_error("ResourceRegistry: unknown id");
  return it->second;
}

bool ResourceRegistry::release(Id id) {
  return kinds_.erase(id) > 0;
}

std::vector<Id> ResourceRegistry::list(ResourceKind kind) const {
  std::vector<Id> out;
  out.reserve(64);
  for (auto& kv : kinds_) {
    if (kv.second == kind) out.push_back(kv.first);
  }
  return out;
}

} // namespace dc
