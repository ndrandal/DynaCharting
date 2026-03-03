#pragma once
#include "dc/commands/CommandHistory.hpp"
#include <memory>
#include <string>
#include <vector>

namespace dc {

// D61: Groups multiple UndoableActions into a single compound action.
// Execute applies all sub-actions in order; undo reverts them in reverse.
class UndoGroup {
public:
  explicit UndoGroup(const std::string& description)
    : description_(description) {}

  void addAction(UndoableAction action) {
    actions_.push_back(std::move(action));
  }

  // Consume the group into a single UndoableAction.
  // After this call the group is empty.
  UndoableAction toAction() {
    UndoableAction compound;
    compound.description = description_;
    // Shared ownership so that both execute and undo can access the list.
    auto shared = std::make_shared<std::vector<UndoableAction>>(std::move(actions_));
    compound.execute = [shared]() {
      for (auto& a : *shared) a.execute();
    };
    compound.undo = [shared]() {
      // Undo in reverse order.
      for (auto it = shared->rbegin(); it != shared->rend(); ++it) {
        it->undo();
      }
    };
    return compound;
  }

private:
  std::string description_;
  std::vector<UndoableAction> actions_;
};

} // namespace dc
