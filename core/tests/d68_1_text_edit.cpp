// D68.1 -- TextEditState: insert, delete, caret movement

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
  std::printf("=== D68.1 TextEditState Tests ===\n");

  // Test 1: Initial state
  {
    dc::TextEditState st;
    check(st.empty(), "initially empty");
    check(st.length() == 0, "length is 0");
    check(st.caret() == 0, "caret at 0");
    check(st.text().empty(), "text is empty string");
    check(!st.isEditing(), "not editing initially");
  }

  // Test 2: setText and basic queries
  {
    dc::TextEditState st;
    st.setText("Hello");
    check(st.text() == "Hello", "setText stores text");
    check(st.length() == 5, "length is 5");
    check(!st.empty(), "not empty after setText");
  }

  // Test 3: insertChar
  {
    dc::TextEditState st;
    st.insertChar('A');
    st.insertChar('B');
    st.insertChar('C');
    check(st.text() == "ABC", "insertChar builds string");
    check(st.caret() == 3, "caret at end after inserts");
  }

  // Test 4: insertText
  {
    dc::TextEditState st;
    st.insertText("Hello");
    check(st.text() == "Hello", "insertText works");
    check(st.caret() == 5, "caret at end");

    st.setCaret(5);
    st.insertText(" World");
    check(st.text() == "Hello World", "insertText appends");
    check(st.caret() == 11, "caret at end after append");
  }

  // Test 5: insertText at middle
  {
    dc::TextEditState st;
    st.setText("HelloWorld");
    st.setCaret(5);
    st.insertText(" ");
    check(st.text() == "Hello World", "insertText at middle");
    check(st.caret() == 6, "caret after inserted text");
  }

  // Test 6: deleteBackward (backspace)
  {
    dc::TextEditState st;
    st.setText("ABC");
    st.setCaret(3);
    st.deleteBackward();
    check(st.text() == "AB", "deleteBackward removes last char");
    check(st.caret() == 2, "caret moves back");

    st.deleteBackward();
    check(st.text() == "A", "deleteBackward again");
    check(st.caret() == 1, "caret at 1");
  }

  // Test 7: deleteBackward at position 0 (no-op)
  {
    dc::TextEditState st;
    st.setText("X");
    st.setCaret(0);
    st.deleteBackward();
    check(st.text() == "X", "deleteBackward at 0 is no-op");
    check(st.caret() == 0, "caret stays at 0");
  }

  // Test 8: deleteForward (delete key)
  {
    dc::TextEditState st;
    st.setText("ABC");
    st.setCaret(0);
    st.deleteForward();
    check(st.text() == "BC", "deleteForward removes first char");
    check(st.caret() == 0, "caret stays at 0");
  }

  // Test 9: deleteForward at end (no-op)
  {
    dc::TextEditState st;
    st.setText("X");
    st.setCaret(1);
    st.deleteForward();
    check(st.text() == "X", "deleteForward at end is no-op");
    check(st.caret() == 1, "caret stays at end");
  }

  // Test 10: moveLeft / moveRight
  {
    dc::TextEditState st;
    st.setText("ABCDE");
    st.setCaret(3);

    st.moveLeft();
    check(st.caret() == 2, "moveLeft decrements caret");

    st.moveRight();
    check(st.caret() == 3, "moveRight increments caret");
  }

  // Test 11: moveLeft at 0 (no-op)
  {
    dc::TextEditState st;
    st.setText("AB");
    st.setCaret(0);
    st.moveLeft();
    check(st.caret() == 0, "moveLeft at 0 stays");
  }

  // Test 12: moveRight at end (no-op)
  {
    dc::TextEditState st;
    st.setText("AB");
    st.setCaret(2);
    st.moveRight();
    check(st.caret() == 2, "moveRight at end stays");
  }

  // Test 13: moveToStart / moveToEnd
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setCaret(5);

    st.moveToStart();
    check(st.caret() == 0, "moveToStart sets caret to 0");

    st.moveToEnd();
    check(st.caret() == 11, "moveToEnd sets caret to length");
  }

  // Test 14: beginEditing / endEditing
  {
    dc::TextEditState st;
    check(!st.isEditing(), "not editing initially");

    st.beginEditing();
    check(st.isEditing(), "editing after beginEditing");

    st.endEditing();
    check(!st.isEditing(), "not editing after endEditing");
  }

  // Test 15: setCaret clamps to valid range
  {
    dc::TextEditState st;
    st.setText("AB");
    st.setCaret(100);
    check(st.caret() == 2, "setCaret clamps to text length");

    st.setCaret(0);
    check(st.caret() == 0, "setCaret(0) works");
  }

  // Test 16: setText clamps existing caret
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setCaret(11);
    st.setText("Hi");
    check(st.caret() == 2, "setText clamps caret when text shrinks");
  }

  // Test 17: deleteBackward in middle of string
  {
    dc::TextEditState st;
    st.setText("ABCDE");
    st.setCaret(3); // caret after 'C'
    st.deleteBackward();
    check(st.text() == "ABDE", "deleteBackward in middle removes correct char");
    check(st.caret() == 2, "caret moves back in middle");
  }

  // Test 18: deleteForward in middle of string
  {
    dc::TextEditState st;
    st.setText("ABCDE");
    st.setCaret(2); // caret before 'C'
    st.deleteForward();
    check(st.text() == "ABDE", "deleteForward in middle removes correct char");
    check(st.caret() == 2, "caret stays in place after deleteForward");
  }

  // Test 19: insertChar with selection replaces it
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(5, 11); // select " World"
    st.insertChar('!');
    check(st.text() == "Hello!", "insertChar replaces selection");
    check(st.caret() == 6, "caret after inserted char");
  }

  // Test 20: insertText with selection replaces it
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(0, 5); // select "Hello"
    st.insertText("Goodbye");
    check(st.text() == "Goodbye World", "insertText replaces selection");
    check(st.caret() == 7, "caret after inserted text");
  }

  // Test 21: deleteBackward with selection deletes selection
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(5, 11);
    st.deleteBackward();
    check(st.text() == "Hello", "deleteBackward with selection deletes selected text");
    check(st.caret() == 5, "caret at start of former selection");
  }

  // Test 22: deleteForward with selection deletes selection
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(0, 6); // select "Hello "
    st.deleteForward();
    check(st.text() == "World", "deleteForward with selection deletes selected text");
    check(st.caret() == 0, "caret at start of former selection");
  }

  // Test 23: operations on empty text
  {
    dc::TextEditState st;
    st.deleteBackward();
    check(st.text().empty(), "deleteBackward on empty is no-op");
    st.deleteForward();
    check(st.text().empty(), "deleteForward on empty is no-op");
    st.moveLeft();
    check(st.caret() == 0, "moveLeft on empty stays at 0");
    st.moveRight();
    check(st.caret() == 0, "moveRight on empty stays at 0");
    st.moveToStart();
    check(st.caret() == 0, "moveToStart on empty stays at 0");
    st.moveToEnd();
    check(st.caret() == 0, "moveToEnd on empty stays at 0");
  }

  // Test 24: moveWordLeft
  {
    dc::TextEditState st;
    st.setText("one two three");
    st.setCaret(13); // end
    st.moveWordLeft();
    check(st.caret() == 8, "moveWordLeft from end to start of 'three'");
    st.moveWordLeft();
    check(st.caret() == 4, "moveWordLeft to start of 'two'");
    st.moveWordLeft();
    check(st.caret() == 0, "moveWordLeft to start of 'one'");
    st.moveWordLeft();
    check(st.caret() == 0, "moveWordLeft at 0 stays");
  }

  // Test 25: moveWordRight
  {
    dc::TextEditState st;
    st.setText("one two three");
    st.setCaret(0);
    st.moveWordRight();
    check(st.caret() == 4, "moveWordRight from 0 past 'one '");
    st.moveWordRight();
    check(st.caret() == 8, "moveWordRight past 'two '");
    st.moveWordRight();
    check(st.caret() == 13, "moveWordRight past 'three' to end");
    st.moveWordRight();
    check(st.caret() == 13, "moveWordRight at end stays");
  }

  // Test 26: moveWordLeft with punctuation
  {
    dc::TextEditState st;
    st.setText("hello-world");
    st.setCaret(11); // end
    st.moveWordLeft();
    check(st.caret() == 6, "moveWordLeft skips past '-' to start of 'world'");
    st.moveWordLeft();
    check(st.caret() == 0, "moveWordLeft to start of 'hello'");
  }

  // Test 27: moveLeft collapses selection to left edge
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(3, 8);
    st.moveLeft();
    check(st.caret() == 3, "moveLeft collapses selection to left edge");
    check(!st.selection().hasSelection(), "selection cleared");
  }

  // Test 28: moveRight collapses selection to right edge
  {
    dc::TextEditState st;
    st.setText("Hello World");
    st.setSelection(3, 8);
    st.moveRight();
    check(st.caret() == 8, "moveRight collapses selection to right edge");
    check(!st.selection().hasSelection(), "selection cleared");
  }

  std::printf("=== D68.1: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
