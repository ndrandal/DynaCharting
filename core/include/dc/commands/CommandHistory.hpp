#pragma once
#include <functional>
#include <string>
#include <vector>

namespace dc {

// D19.1: Generic undo/redo stack for user-facing actions.
// NOT tied to CommandProcessor — this tracks higher-level operations
// (e.g. adding/removing drawings) that the user may want to undo.

struct UndoableAction {
  std::string description;             // human-readable description
  std::function<void()> execute;       // do / redo
  std::function<void()> undo;          // undo
};

class CommandHistory {
public:
  // Execute an action and push it onto the undo stack.
  // Clears the redo stack (new action invalidates redo branch).
  void execute(UndoableAction action);

  // Undo the last action. Returns true if an action was undone.
  bool undo();

  // Redo the last undone action. Returns true if an action was redone.
  bool redo();

  // Can undo/redo?
  bool canUndo() const;
  bool canRedo() const;

  // Stack sizes (useful for UI display)
  std::size_t undoCount() const;
  std::size_t redoCount() const;

  // Clear all history
  void clear();

  // Get description of the next undo/redo action.
  // Returns empty string if the respective stack is empty.
  const std::string& undoDescription() const;
  const std::string& redoDescription() const;

private:
  std::vector<UndoableAction> undoStack_;
  std::vector<UndoableAction> redoStack_;
  static const std::string empty_;
};

} // namespace dc
