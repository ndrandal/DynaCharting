#include "dc/interaction/FocusManager.hpp"
#include "dc/viewport/InputState.hpp"
#include <algorithm>

namespace dc {

void FocusManager::addFocusable(const FocusableItem& item) {
  for (auto& existing : items_) {
    if (existing.id == item.id) {
      existing = item;
      sortItems();
      return;
    }
  }
  items_.push_back(item);
  sortItems();
}

void FocusManager::removeFocusable(Id id) {
  if (focusedId_ == id) focusedId_ = 0;
  items_.erase(
    std::remove_if(items_.begin(), items_.end(),
      [id](const FocusableItem& fi) { return fi.id == id; }),
    items_.end());
}

void FocusManager::setFocus(Id id) {
  for (const auto& item : items_) {
    if (item.id == id) {
      focusedId_ = id;
      return;
    }
  }
}

void FocusManager::clearFocus() {
  focusedId_ = 0;
}

void FocusManager::focusNext() {
  if (items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) {
    focusedId_ = items_[0].id;
  } else {
    idx = (idx + 1) % static_cast<int>(items_.size());
    focusedId_ = items_[idx].id;
  }
}

void FocusManager::focusPrevious() {
  if (items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) {
    focusedId_ = items_.back().id;
  } else {
    idx = (idx - 1 + static_cast<int>(items_.size())) % static_cast<int>(items_.size());
    focusedId_ = items_[idx].id;
  }
}

void FocusManager::navigateLeft() {
  if (focusedId_ == 0 || items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) return;
  Id groupId = items_[idx].groupId;

  for (int i = idx - 1; i >= 0; i--) {
    if (items_[i].groupId == groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
  for (int i = static_cast<int>(items_.size()) - 1; i > idx; i--) {
    if (items_[i].groupId == groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
}

void FocusManager::navigateRight() {
  if (focusedId_ == 0 || items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) return;
  Id groupId = items_[idx].groupId;

  for (int i = idx + 1; i < static_cast<int>(items_.size()); i++) {
    if (items_[i].groupId == groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
  for (int i = 0; i < idx; i++) {
    if (items_[i].groupId == groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
}

void FocusManager::navigateUp() {
  if (focusedId_ == 0 || items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) return;
  Id groupId = items_[idx].groupId;

  for (int i = idx - 1; i >= 0; i--) {
    if (items_[i].groupId != groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
}

void FocusManager::navigateDown() {
  if (focusedId_ == 0 || items_.empty()) return;
  int idx = focusedIndex();
  if (idx < 0) return;
  Id groupId = items_[idx].groupId;

  for (int i = idx + 1; i < static_cast<int>(items_.size()); i++) {
    if (items_[i].groupId != groupId) {
      focusedId_ = items_[i].id;
      return;
    }
  }
}

bool FocusManager::processKey(KeyCode key) {
  switch (key) {
    case KeyCode::Tab:
      focusNext();
      return true;
    case KeyCode::Escape:
      clearFocus();
      return true;
    case KeyCode::Left:
      navigateLeft();
      return true;
    case KeyCode::Right:
      navigateRight();
      return true;
    case KeyCode::Up:
      navigateUp();
      return true;
    case KeyCode::Down:
      navigateDown();
      return true;
    default:
      return false;
  }
}

int FocusManager::focusedIndex() const {
  for (int i = 0; i < static_cast<int>(items_.size()); i++) {
    if (items_[i].id == focusedId_) return i;
  }
  return -1;
}

void FocusManager::sortItems() {
  std::sort(items_.begin(), items_.end(),
    [](const FocusableItem& a, const FocusableItem& b) {
      if (a.tabOrder != b.tabOrder) return a.tabOrder < b.tabOrder;
      return a.id < b.id;
    });
}

} // namespace dc
