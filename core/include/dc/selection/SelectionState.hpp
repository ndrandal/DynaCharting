#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dc {

struct SelectionKey {
  Id drawItemId{0};
  std::uint32_t recordIndex{0};
  bool operator==(const SelectionKey& o) const {
    return drawItemId == o.drawItemId && recordIndex == o.recordIndex;
  }
  bool operator!=(const SelectionKey& o) const { return !(*this == o); }
};

enum class SelectionMode : std::uint8_t { Single, Toggle };

class SelectionState {
public:
  void setMode(SelectionMode mode) { mode_ = mode; }
  SelectionMode mode() const { return mode_; }

  void select(const SelectionKey& key);
  void deselect(const SelectionKey& key);
  void toggle(const SelectionKey& key);
  void clear();

  bool isSelected(const SelectionKey& key) const;
  bool hasSelection() const;
  std::vector<SelectionKey> selectedKeys() const;

  // Navigation within same drawItemId
  void setRecordCount(Id drawItemId, std::uint32_t count);
  bool selectNext();
  bool selectPrevious();
  SelectionKey current() const;

private:
  SelectionMode mode_{SelectionMode::Single};
  std::vector<SelectionKey> selected_;
  std::unordered_map<Id, std::uint32_t> recordCounts_;

  void removeKey(const SelectionKey& key);
  bool containsKey(const SelectionKey& key) const;
};

} // namespace dc
