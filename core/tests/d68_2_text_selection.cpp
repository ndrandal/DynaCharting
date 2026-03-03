// D68.2 -- TextEditState: selection, cut/copy/paste, word navigation with extend

#include "dc/text/TextEditState.hpp"

#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D68.2 TextEditState Selection Tests ===\n");

  // Test 1: setSelection and selectedText
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(0, 5);
    check(st.selection().hasSelection(), "has selection");
    check(st.selectedText() == "Hello", "selectedText returns 'Hello'");
    check(st.selection().start == 0, "selection start is 0");
    check(st.selection().end == 5, "selection end is 5");
    check(st.selection().min() == 0, "selection min is 0");
    check(st.selection().max() == 5, "selection max is 5");
  }

  // Test 2: Reverse selection (end < start logically via anchor)
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(5, 0); // reverse: anchor at 5, extend to 0
    check(st.selection().hasSelection(), "reverse selection exists");
    check(st.selectedText() == "Hello", "reverse selectedText correct");
    check(st.selection().min() == 0, "reverse selection min");
    check(st.selection().max() == 5, "reverse selection max");
  }

  // Test 3: selectAll
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.selectAll();
    check(st.selectedText() == "Hello", "selectAll selects entire text");
    check(st.selection().start == 0, "selectAll start is 0");
    check(st.selection().end == 5, "selectAll end is length");
    check(st.caret() == 5, "caret at end after selectAll");
  }

  // Test 4: selectAll on empty text
  {
    dc::TextEditState st;
    st.selectAll();
    check(!st.selection().hasSelection(), "selectAll on empty has no selection");
    check(st.selectedText().empty(), "selectedText empty");
  }

  // Test 5: clearSelection
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.selectAll();
    check(st.selection().hasSelection(), "has selection before clear");
    st.clearSelection();
    check(!st.selection().hasSelection(), "no selection after clear");
  }

  // Test 6: deleteSelection
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(5, 11); // " World"
    st.deleteSelection();
    check(st.text() == "Hello", "deleteSelection removes selected range");
    check(st.caret() == 5, "caret at start of deleted range");
    check(!st.selection().hasSelection(), "no selection after delete");
  }

  // Test 7: deleteSelection reverse
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(11, 5); // reverse
    st.deleteSelection();
    check(st.text() == "Hello", "deleteSelection reverse removes correct range");
    check(st.caret() == 5, "caret at min of deleted range");
  }

  // Test 8: deleteSelection with no selection (no-op)
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setCaret(3);
    st.deleteSelection();
    check(st.text() == "Hello", "deleteSelection with no selection is no-op");
    check(st.caret() == 3, "caret unchanged");
  }

  // Test 9: cut
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(0, 5);
    std::string cut = st.cut();
    check(cut == "Hello", "cut returns selected text");
    check(st.text() == " World", "cut removes selected text");
    check(st.caret() == 0, "caret at start of cut range");
    check(!st.selection().hasSelection(), "no selection after cut");
  }

  // Test 10: cut with no selection
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setCaret(3);
    std::string cut = st.cut();
    check(cut.empty(), "cut returns empty with no selection");
    check(st.text() == "Hello", "text unchanged");
  }

  // Test 11: copy
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(6, 11);
    std::string copied = st.copy();
    check(copied == "World", "copy returns selected text");
    check(st.text() == "Hello World", "text unchanged after copy");
    check(st.selection().hasSelection(), "selection preserved after copy");
  }

  // Test 12: copy with no selection
  {
    dc::TextEditState st;
    st.setText("Hello");
    std::string copied = st.copy();
    check(copied.empty(), "copy returns empty with no selection");
  }

  // Test 13: paste
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setCaret(5);
    st.paste(" World");
    check(st.text() == "Hello World", "paste appends text");
    check(st.caret() == 11, "caret after pasted text");
  }

  // Test 14: paste replaces selection
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(6, 11); // "World"
    st.paste("Earth");
    check(st.text() == "Hello Earth", "paste replaces selection");
    check(st.caret() == 11, "caret after pasted text");
    check(!st.selection().hasSelection(), "no selection after paste");
  }

  // Test 15: paste empty string
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setSelection(0, 5);
    st.paste("");
    check(st.text().empty(), "paste empty replaces selection with nothing");
    check(st.caret() == 0, "caret at 0");
  }

  // Test 16: moveLeft with extendSelection
  {
    dc::TextEditState st;
    st.setText("ABCDE");
    st.setCaret(4);
    st.moveLeft(true);
    check(st.caret() == 3, "caret moved left");
    check(st.selection().hasSelection(), "selection extended");
    check(st.selection().start == 4, "selection start (anchor) at 4");
    check(st.selection().end == 3, "selection end at 3");
    check(st.selectedText() == "D", "selected 'D'");

    st.moveLeft(true);
    check(st.caret() == 2, "caret moved left again");
    check(st.selectedText() == "CD", "selected 'CD'");
  }

  // Test 17: moveRight with extendSelection
  {
    dc::TextEditState st;
    st.setText("ABCDE");
    st.setCaret(1);
    st.moveRight(true);
    check(st.caret() == 2, "caret moved right");
    check(st.selectedText() == "B", "selected 'B'");

    st.moveRight(true);
    check(st.selectedText() == "BC", "selected 'BC'");
  }

  // Test 18: moveToStart with extendSelection
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setCaret(3);
    st.moveToStart(true);
    check(st.caret() == 0, "caret at start");
    check(st.selectedText() == "Hel", "selected 'Hel'");
    check(st.selection().start == 3, "anchor at 3");
    check(st.selection().end == 0, "end at 0");
  }

  // Test 19: moveToEnd with extendSelection
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.setCaret(2);
    st.moveToEnd(true);
    check(st.caret() == 5, "caret at end");
    check(st.selectedText() == "llo", "selected 'llo'");
    check(st.selection().start == 2, "anchor at 2");
    check(st.selection().end == 5, "end at 5");
  }

  // Test 20: moveWordLeft with extendSelection
  {
    dc::TextEditState st;
    st.setText("one two three");
    st.setCaret(13); // end
    st.moveWordLeft(true);
    check(st.caret() == 8, "caret at start of 'three'");
    check(st.selectedText() == "three", "selected 'three'");
    check(st.selection().start == 13, "anchor at 13");
    check(st.selection().end == 8, "end at 8");
  }

  // Test 21: moveWordRight with extendSelection
  {
    dc::TextEditState st;
    st.setText("one two three");
    st.setCaret(0);
    st.moveWordRight(true);
    check(st.caret() == 4, "caret past 'one '");
    check(st.selectedText() == "one ", "selected 'one '");
    check(st.selection().start == 0, "anchor at 0");
    check(st.selection().end == 4, "end at 4");
  }

  // Test 22: endEditing clears selection
  {
    dc::TextEditState st;
    st.setText("Hello");
    st.beginEditing();
    st.selectAll();
    check(st.selection().hasSelection(), "selection exists before endEditing");
    st.endEditing();
    check(!st.selection().hasSelection(), "selection cleared after endEditing");
    check(!st.isEditing(), "not editing after endEditing");
  }

  // Test 23: Extend selection then collapse with non-extend move
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setCaret(0);
    st.moveRight(true);
    st.moveRight(true);
    st.moveRight(true);
    check(st.selectedText() == "Hel", "extended selection: 'Hel'");

    // Non-extend moveRight collapses to right edge
    st.moveRight(false);
    check(!st.selection().hasSelection(), "selection collapsed");
    check(st.caret() == 3, "caret at right edge of former selection");
  }

  // Test 24: Word navigation with multiple spaces
  {
    dc::TextEditState st;
    st.setText("hello   world");
    st.setCaret(0);
    st.moveWordRight();
    check(st.caret() == 8, "moveWordRight skips multiple spaces");

    st.moveWordLeft();
    check(st.caret() == 0, "moveWordLeft skips back over multiple spaces");
  }

  // Test 25: Word navigation with mixed punctuation
  {
    dc::TextEditState st;
    st.setText("foo.bar(baz)");
    st.setCaret(0);
    st.moveWordRight();
    check(st.caret() == 4, "moveWordRight stops after 'foo.'");
    st.moveWordRight();
    check(st.caret() == 8, "moveWordRight stops after 'bar('");
    st.moveWordRight();
    check(st.caret() == 12, "moveWordRight reaches end past 'baz)'");
  }

  // Test 26: cut/copy/paste round-trip
  {
    dc::TextEditState st;
    st.setText("ABCDEF");
    st.setSelection(2, 4); // "CD"
    std::string clip = st.copy();
    check(clip == "CD", "copy grabs 'CD'");

    st.setCaret(6);
    st.paste(clip);
    check(st.text() == "ABCDEFCD", "paste appends copied text");
    check(st.caret() == 8, "caret at end");
  }

  // Test 27: cut then paste elsewhere
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(0, 6); // "Hello "
    std::string clip = st.cut();
    check(st.text() == "World", "cut removes 'Hello '");
    check(clip == "Hello ", "clip is 'Hello '");

    st.setCaret(5); // end of "World"
    st.paste(clip);
    check(st.text() == "WorldHello ", "paste at end");
  }

  // Test 28: TextSelection struct helpers
  {
    dc::TextSelection sel;
    check(!sel.hasSelection(), "default TextSelection has no selection");
    check(sel.min() == 0, "min is 0");
    check(sel.max() == 0, "max is 0");

    sel.start = 10;
    sel.end = 3;
    check(sel.hasSelection(), "non-equal start/end has selection");
    check(sel.min() == 3, "min of (10,3) is 3");
    check(sel.max() == 10, "max of (10,3) is 10");
  }

  // Test 29: moveLeft extend at position 0 stays
  {
    dc::TextEditState st;
    st.setText("AB");
    st.setCaret(0);
    st.moveLeft(true);
    check(st.caret() == 0, "moveLeft extend at 0 stays");
    check(!st.selection().hasSelection(), "no selection created");
  }

  // Test 30: moveRight extend at end stays
  {
    dc::TextEditState st;
    st.setText("AB");
    st.setCaret(2);
    st.moveRight(true);
    check(st.caret() == 2, "moveRight extend at end stays");
    check(!st.selection().hasSelection(), "no selection created");
  }

  std::printf("=== D68.2: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
