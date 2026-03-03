#pragma once
#include "dc/ids/Id.hpp"
#include <cstdint>
#include <vector>

namespace dc {

enum class KeyCode : std::uint8_t;

struct FocusableItem {
  Id id{0};
  int tabOrder{0};
  Id groupId{0};
};

class FocusManager {
public:
  void addFocusable(const FocusableItem& item);
  void removeFocusable(Id id);

  void setFocus(Id id);
  void clearFocus();
  Id focusedId() const { return focusedId_; }

  void focusNext();
  void focusPrevious();

  void navigateLeft();
  void navigateRight();
  void navigateUp();
  void navigateDown();

  bool processKey(KeyCode key);

  std::size_t count() const { return items_.size(); }

private:
  std::vector<FocusableItem> items_;
  Id focusedId_{0};

  int focusedIndex() const;
  void sortItems();
};

} // namespace dc
