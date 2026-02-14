#include "dc/commands/CommandHistory.hpp"

namespace dc {

const std::string CommandHistory::empty_;

void CommandHistory::execute(UndoableAction action) {
  action.execute();
  undoStack_.push_back(std::move(action));
  redoStack_.clear();
}

bool CommandHistory::undo() {
  if (undoStack_.empty()) return false;
  auto action = std::move(undoStack_.back());
  undoStack_.pop_back();
  action.undo();
  redoStack_.push_back(std::move(action));
  return true;
}

bool CommandHistory::redo() {
  if (redoStack_.empty()) return false;
  auto action = std::move(redoStack_.back());
  redoStack_.pop_back();
  action.execute();
  undoStack_.push_back(std::move(action));
  return true;
}

bool CommandHistory::canUndo() const { return !undoStack_.empty(); }
bool CommandHistory::canRedo() const { return !redoStack_.empty(); }

std::size_t CommandHistory::undoCount() const { return undoStack_.size(); }
std::size_t CommandHistory::redoCount() const { return redoStack_.size(); }

void CommandHistory::clear() {
  undoStack_.clear();
  redoStack_.clear();
}

const std::string& CommandHistory::undoDescription() const {
  return undoStack_.empty() ? empty_ : undoStack_.back().description;
}

const std::string& CommandHistory::redoDescription() const {
  return redoStack_.empty() ? empty_ : redoStack_.back().description;
}

} // namespace dc
