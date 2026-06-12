// ENC-595 (P1.4) — Generic reactive dirty/recompute mechanism.
// See ReactiveGraph.hpp for the design + the genericity contract.
#include "dc/data/ReactiveGraph.hpp"

#include <algorithm>

namespace dc {

void ReactiveGraph::addDependency(DependentId dependent, const InputNode& input) {
  inputs_[input].insert(dependent);
}

void ReactiveGraph::removeDependent(DependentId dependent) {
  for (auto it = inputs_.begin(); it != inputs_.end();) {
    it->second.erase(dependent);
    if (it->second.empty()) {
      it = inputs_.erase(it);
    } else {
      ++it;
    }
  }
  pending_.erase(dependent);
}

std::size_t ReactiveGraph::markDirty(const InputNode& input) {
  auto it = inputs_.find(input);
  if (it == inputs_.end()) return 0;  // no dependents on this input — inert
  std::size_t scheduled = 0;
  for (DependentId d : it->second) {
    if (pending_.insert(d).second) ++scheduled;
  }
  return scheduled;
}

std::size_t ReactiveGraph::markDataBuffersDirty(
    const std::vector<Id>& bufferIds) {
  std::size_t scheduled = 0;
  for (Id id : bufferIds) {
    scheduled += markDirty(dataInput(id));
  }
  return scheduled;
}

std::size_t ReactiveGraph::syncTableVersions(
    const std::vector<std::pair<Id, std::uint64_t>>& tableVersions) {
  std::size_t scheduled = 0;
  for (const auto& [tableId, version] : tableVersions) {
    auto seenIt = tableVersionSeen_.find(tableId);
    const bool changed =
        seenIt == tableVersionSeen_.end() || seenIt->second != version;
    tableVersionSeen_[tableId] = version;
    if (changed) {
      scheduled += markDirty(dataInput(tableId));
    }
  }
  return scheduled;
}

std::vector<DependentId> ReactiveGraph::drain() {
  std::vector<DependentId> out(pending_.begin(), pending_.end());
  std::sort(out.begin(), out.end());
  pending_.clear();
  return out;
}

bool ReactiveGraph::dependsOn(DependentId dependent,
                              const InputNode& input) const {
  auto it = inputs_.find(input);
  if (it == inputs_.end()) return false;
  return it->second.count(dependent) > 0;
}

}  // namespace dc
