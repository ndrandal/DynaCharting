// ENC-627 (C1) — PickInstanceTable: the per-DrawItem row-id side table that turns
// a (drawItemId, instanceIndex) pick into a durable source ROW id.
//
// WHY
// ---
// Today picking returns only a 24-bit per-DrawItem id (DawnPickBackend): an
// instanced mark (a treemap, a scatter — one DrawItem, many instances) collapses
// to ONE id, so "which datum did the user touch?" is unanswerable (RESEARCH §7.3).
// The encode pass already produces, out of band, one durable row id per emitted
// instance (`EncodeResult::instanceRowIds`, ENC-594). This table holds that array
// keyed by DrawItem id, so once the pick pass identifies the hit INSTANCE (the
// ENC-628 shader change), the row id is a direct lookup — no GPU format change.
//
// C1 is the plumbing: the table + the result fields + the renderPick wiring. The
// table is pure `dc` (no GPU), unit-tested in the default build; ENC-628 supplies
// the instance index that makes rowIdForInstance() return a real id.
#pragma once

#include "dc/ids/Id.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

class PickInstanceTable {
 public:
  // Register (or replace) the per-instance row ids for `drawItemId`, in instance
  // order — exactly EncodeResult::instanceRowIds for the mark that produced this
  // DrawItem. An empty vector clears the entry.
  void setInstanceRowIds(Id drawItemId, std::vector<std::int32_t> rowIds) {
    if (rowIds.empty()) {
      table_.erase(drawItemId);
    } else {
      table_[drawItemId] = std::move(rowIds);
    }
  }

  // Drop a DrawItem's ids (e.g. on dispose).
  void remove(Id drawItemId) { table_.erase(drawItemId); }
  void clear() { table_.clear(); }

  bool has(Id drawItemId) const { return table_.count(drawItemId) > 0; }
  std::size_t size() const { return table_.size(); }

  // The durable row id for instance `instanceIndex` of `drawItemId`, or -1 if the
  // DrawItem has no registered ids or the index is out of range (the sentinel the
  // pick result uses for "no per-instance id").
  std::int32_t rowIdForInstance(Id drawItemId, std::int32_t instanceIndex) const {
    if (instanceIndex < 0) return -1;
    auto it = table_.find(drawItemId);
    if (it == table_.end()) return -1;
    const auto idx = static_cast<std::size_t>(instanceIndex);
    if (idx >= it->second.size()) return -1;
    return it->second[idx];
  }

  // Read-only view of a DrawItem's ids (empty if unregistered).
  const std::vector<std::int32_t>& instanceRowIds(Id drawItemId) const {
    static const std::vector<std::int32_t> kEmpty;
    auto it = table_.find(drawItemId);
    return it == table_.end() ? kEmpty : it->second;
  }

 private:
  std::unordered_map<Id, std::vector<std::int32_t>> table_;
};

}  // namespace dc
