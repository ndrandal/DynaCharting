#pragma once

#include <cstddef>
#include <string>

namespace dc {

struct TextSelection {
  std::size_t start{0};  // caret position or selection start
  std::size_t end{0};    // same as start if no selection
  bool hasSelection() const { return start != end; }
  std::size_t min() const { return start < end ? start : end; }
  std::size_t max() const { return start > end ? start : end; }
};

class TextEditState {
public:
  // Content
  void setText(const std::string& text);
  const std::string& text() const;
  bool empty() const;
  std::size_t length() const;

  // Caret
  void setCaret(std::size_t pos);
  std::size_t caret() const;

  // Selection
  void setSelection(std::size_t start, std::size_t end);
  void selectAll();
  void clearSelection();
  TextSelection selection() const;
  std::string selectedText() const;

  // Editing operations
  void insertChar(char ch);
  void insertText(const std::string& str);
  void deleteBackward();   // backspace
  void deleteForward();    // delete key
  void deleteSelection();

  // Navigation
  void moveLeft(bool extendSelection = false);
  void moveRight(bool extendSelection = false);
  void moveToStart(bool extendSelection = false);
  void moveToEnd(bool extendSelection = false);
  void moveWordLeft(bool extendSelection = false);
  void moveWordRight(bool extendSelection = false);

  // Clipboard helpers (manipulate internal buffer; actual clipboard is host responsibility)
  std::string cut();
  std::string copy() const;
  void paste(const std::string& content);

  // State
  bool isEditing() const;
  void beginEditing();
  void endEditing();

private:
  std::string text_;
  std::size_t caret_{0};
  std::size_t selStart_{0};
  std::size_t selEnd_{0};
  bool editing_{false};

  // Clamp position to valid range [0, text_.size()]
  std::size_t clamp(std::size_t pos) const;

  // Check if character is a word boundary separator
  static bool isWordSeparator(char ch);
};

} // namespace dc
