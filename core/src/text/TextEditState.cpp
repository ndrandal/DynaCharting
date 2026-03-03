#include "dc/text/TextEditState.hpp"

#include <cctype>

namespace dc {

// ---- Content ----

void TextEditState::setText(const std::string& text) {
  text_ = text;
  caret_ = clamp(caret_);
  selStart_ = clamp(selStart_);
  selEnd_ = clamp(selEnd_);
}

const std::string& TextEditState::text() const { return text_; }

bool TextEditState::empty() const { return text_.empty(); }

std::size_t TextEditState::length() const { return text_.size(); }

// ---- Caret ----

void TextEditState::setCaret(std::size_t pos) {
  caret_ = clamp(pos);
  selStart_ = caret_;
  selEnd_ = caret_;
}

std::size_t TextEditState::caret() const { return caret_; }

// ---- Selection ----

void TextEditState::setSelection(std::size_t start, std::size_t end) {
  selStart_ = clamp(start);
  selEnd_ = clamp(end);
  caret_ = selEnd_;
}

void TextEditState::selectAll() {
  selStart_ = 0;
  selEnd_ = text_.size();
  caret_ = selEnd_;
}

void TextEditState::clearSelection() {
  selStart_ = caret_;
  selEnd_ = caret_;
}

TextSelection TextEditState::selection() const {
  return {selStart_, selEnd_};
}

std::string TextEditState::selectedText() const {
  if (selStart_ == selEnd_) return {};
  auto lo = selStart_ < selEnd_ ? selStart_ : selEnd_;
  auto hi = selStart_ > selEnd_ ? selStart_ : selEnd_;
  return text_.substr(lo, hi - lo);
}

// ---- Editing operations ----

void TextEditState::insertChar(char ch) {
  if (selStart_ != selEnd_) {
    deleteSelection();
  }
  text_.insert(text_.begin() + static_cast<std::ptrdiff_t>(caret_), ch);
  ++caret_;
  selStart_ = caret_;
  selEnd_ = caret_;
}

void TextEditState::insertText(const std::string& str) {
  if (selStart_ != selEnd_) {
    deleteSelection();
  }
  text_.insert(caret_, str);
  caret_ += str.size();
  selStart_ = caret_;
  selEnd_ = caret_;
}

void TextEditState::deleteBackward() {
  if (selStart_ != selEnd_) {
    deleteSelection();
    return;
  }
  if (caret_ == 0) return;
  text_.erase(caret_ - 1, 1);
  --caret_;
  selStart_ = caret_;
  selEnd_ = caret_;
}

void TextEditState::deleteForward() {
  if (selStart_ != selEnd_) {
    deleteSelection();
    return;
  }
  if (caret_ >= text_.size()) return;
  text_.erase(caret_, 1);
  selStart_ = caret_;
  selEnd_ = caret_;
}

void TextEditState::deleteSelection() {
  if (selStart_ == selEnd_) return;
  auto lo = selStart_ < selEnd_ ? selStart_ : selEnd_;
  auto hi = selStart_ > selEnd_ ? selStart_ : selEnd_;
  text_.erase(lo, hi - lo);
  caret_ = lo;
  selStart_ = lo;
  selEnd_ = lo;
}

// ---- Navigation ----

void TextEditState::moveLeft(bool extendSelection) {
  if (extendSelection) {
    if (caret_ > 0) {
      --caret_;
      selEnd_ = caret_;
    }
  } else {
    if (selStart_ != selEnd_) {
      // Collapse selection to the left edge
      caret_ = selStart_ < selEnd_ ? selStart_ : selEnd_;
    } else if (caret_ > 0) {
      --caret_;
    }
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

void TextEditState::moveRight(bool extendSelection) {
  if (extendSelection) {
    if (caret_ < text_.size()) {
      ++caret_;
      selEnd_ = caret_;
    }
  } else {
    if (selStart_ != selEnd_) {
      // Collapse selection to the right edge
      caret_ = selStart_ > selEnd_ ? selStart_ : selEnd_;
    } else if (caret_ < text_.size()) {
      ++caret_;
    }
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

void TextEditState::moveToStart(bool extendSelection) {
  caret_ = 0;
  if (extendSelection) {
    selEnd_ = caret_;
  } else {
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

void TextEditState::moveToEnd(bool extendSelection) {
  caret_ = text_.size();
  if (extendSelection) {
    selEnd_ = caret_;
  } else {
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

void TextEditState::moveWordLeft(bool extendSelection) {
  if (caret_ == 0) return;

  std::size_t pos = caret_;

  // Skip any separators immediately before caret
  while (pos > 0 && isWordSeparator(text_[pos - 1])) {
    --pos;
  }
  // Skip word characters to reach the start of the word
  while (pos > 0 && !isWordSeparator(text_[pos - 1])) {
    --pos;
  }

  caret_ = pos;
  if (extendSelection) {
    selEnd_ = caret_;
  } else {
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

void TextEditState::moveWordRight(bool extendSelection) {
  if (caret_ >= text_.size()) return;

  std::size_t pos = caret_;

  // Skip word characters to reach end of current word
  while (pos < text_.size() && !isWordSeparator(text_[pos])) {
    ++pos;
  }
  // Skip separators to reach start of next word
  while (pos < text_.size() && isWordSeparator(text_[pos])) {
    ++pos;
  }

  caret_ = pos;
  if (extendSelection) {
    selEnd_ = caret_;
  } else {
    selStart_ = caret_;
    selEnd_ = caret_;
  }
}

// ---- Clipboard helpers ----

std::string TextEditState::cut() {
  std::string result = selectedText();
  if (!result.empty()) {
    deleteSelection();
  }
  return result;
}

std::string TextEditState::copy() const {
  return selectedText();
}

void TextEditState::paste(const std::string& content) {
  insertText(content);
}

// ---- State ----

bool TextEditState::isEditing() const { return editing_; }

void TextEditState::beginEditing() { editing_ = true; }

void TextEditState::endEditing() {
  editing_ = false;
  clearSelection();
}

// ---- Private helpers ----

std::size_t TextEditState::clamp(std::size_t pos) const {
  return pos > text_.size() ? text_.size() : pos;
}

bool TextEditState::isWordSeparator(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) ||
         std::ispunct(static_cast<unsigned char>(ch));
}

} // namespace dc
