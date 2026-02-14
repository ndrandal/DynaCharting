#include "dc/selection/SelectionState.hpp"
#include <algorithm>

namespace dc {

void SelectionState::select(const SelectionKey& key) {
  if (mode_ == SelectionMode::Single) {
    selected_.clear();
  }
  if (!containsKey(key)) {
    selected_.push_back(key);
  }
}

void SelectionState::deselect(const SelectionKey& key) {
  removeKey(key);
}

void SelectionState::toggle(const SelectionKey& key) {
  if (containsKey(key)) {
    removeKey(key);
  } else {
    if (mode_ == SelectionMode::Single) {
      selected_.clear();
    }
    selected_.push_back(key);
  }
}

void SelectionState::clear() {
  selected_.clear();
}

bool SelectionState::isSelected(const SelectionKey& key) const {
  return containsKey(key);
}

bool SelectionState::hasSelection() const {
  return !selected_.empty();
}

std::vector<SelectionKey> SelectionState::selectedKeys() const {
  return selected_;
}

void SelectionState::setRecordCount(Id drawItemId, std::uint32_t count) {
  recordCounts_[drawItemId] = count;
}

bool SelectionState::selectNext() {
  if (selected_.empty()) return false;
  auto& cur = selected_.back();
  auto it = recordCounts_.find(cur.drawItemId);
  if (it == recordCounts_.end()) return false;
  if (cur.recordIndex + 1 >= it->second) return false;
  // Move to next
  SelectionKey next{cur.drawItemId, cur.recordIndex + 1};
  selected_.clear();
  selected_.push_back(next);
  return true;
}

bool SelectionState::selectPrevious() {
  if (selected_.empty()) return false;
  auto& cur = selected_.back();
  if (cur.recordIndex == 0) return false;
  SelectionKey prev{cur.drawItemId, cur.recordIndex - 1};
  selected_.clear();
  selected_.push_back(prev);
  return true;
}

SelectionKey SelectionState::current() const {
  if (selected_.empty()) return {};
  return selected_.back();
}

void SelectionState::removeKey(const SelectionKey& key) {
  selected_.erase(
    std::remove_if(selected_.begin(), selected_.end(),
      [&](const SelectionKey& k) { return k == key; }),
    selected_.end());
}

bool SelectionState::containsKey(const SelectionKey& key) const {
  for (auto& k : selected_) {
    if (k == key) return true;
  }
  return false;
}

} // namespace dc
