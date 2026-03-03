// D61.2 -- UndoGroup: compound undo/redo of multiple actions

#include "dc/commands/UndoGroup.hpp"

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
  std::printf("=== D61.2 UndoGroup Tests ===\n");

  // Test 1: Group of 2 actions (color + position) — execute, undo, redo
  {
    dc::CommandHistory hist;

    float r = 1.0f, g = 1.0f, b = 1.0f; // white
    float x = 0.0f, y = 0.0f;            // origin

    dc::UndoGroup group("move and recolor");

    group.addAction({"set color to blue",
      [&]() { r = 0.0f; g = 0.0f; b = 1.0f; },
      [&]() { r = 1.0f; g = 1.0f; b = 1.0f; }
    });

    group.addAction({"set position to (10,20)",
      [&]() { x = 10.0f; y = 20.0f; },
      [&]() { x = 0.0f; y = 0.0f; }
    });

    hist.execute(group.toAction());

    // Both actions executed
    check(r == 0.0f && g == 0.0f && b == 1.0f, "group execute: color is blue");
    check(x == 10.0f && y == 20.0f, "group execute: position is (10,20)");
    check(hist.undoCount() == 1, "group counts as 1 undo entry");
    check(hist.undoDescription() == "move and recolor", "group description");

    // Undo reverts both
    hist.undo();
    check(r == 1.0f && g == 1.0f && b == 1.0f, "group undo: color reverted to white");
    check(x == 0.0f && y == 0.0f, "group undo: position reverted to origin");
    check(hist.undoCount() == 0, "undo stack empty");
    check(hist.redoCount() == 1, "redo has 1");

    // Redo re-applies both
    hist.redo();
    check(r == 0.0f && g == 0.0f && b == 1.0f, "group redo: color is blue again");
    check(x == 10.0f && y == 20.0f, "group redo: position is (10,20) again");
  }

  // Test 2: Undo reverses in correct order
  {
    dc::CommandHistory hist;
    std::vector<int> log;

    dc::UndoGroup group("ordered undo");

    group.addAction({"step A",
      [&]() { log.push_back(1); },
      [&]() { log.push_back(-1); }
    });
    group.addAction({"step B",
      [&]() { log.push_back(2); },
      [&]() { log.push_back(-2); }
    });
    group.addAction({"step C",
      [&]() { log.push_back(3); },
      [&]() { log.push_back(-3); }
    });

    hist.execute(group.toAction());
    // Execute order: 1, 2, 3
    check(log.size() == 3, "3 execute entries");
    check(log[0] == 1 && log[1] == 2 && log[2] == 3, "execute order: A, B, C");

    hist.undo();
    // Undo order: -3, -2, -1 (reverse)
    check(log.size() == 6, "3 more undo entries");
    check(log[3] == -3 && log[4] == -2 && log[5] == -1, "undo order: C, B, A (reversed)");
  }

  // Test 3: Group interleaves with regular actions
  {
    dc::CommandHistory hist;
    int a = 0, b = 0, c = 0;

    // Regular action
    hist.execute({"set a",
      [&]() { a = 1; },
      [&]() { a = 0; }
    });

    // Grouped action
    dc::UndoGroup group("set b and c");
    group.addAction({"set b",
      [&]() { b = 2; },
      [&]() { b = 0; }
    });
    group.addAction({"set c",
      [&]() { c = 3; },
      [&]() { c = 0; }
    });
    hist.execute(group.toAction());

    check(a == 1 && b == 2 && c == 3, "all values set");
    check(hist.undoCount() == 2, "2 undo entries (1 regular + 1 group)");

    // Undo group
    hist.undo();
    check(a == 1, "a unchanged after group undo");
    check(b == 0 && c == 0, "b and c reverted by group undo");

    // Undo regular
    hist.undo();
    check(a == 0, "a reverted");

    // Redo both
    hist.redo();
    check(a == 1, "redo regular");
    hist.redo();
    check(b == 2 && c == 3, "redo group");
  }

  // Test 4: Empty group
  {
    dc::CommandHistory hist;
    dc::UndoGroup group("empty group");

    // toAction with no sub-actions — execute/undo are no-ops
    hist.execute(group.toAction());
    check(hist.undoCount() == 1, "empty group still pushed to undo stack");
    check(hist.undoDescription() == "empty group", "empty group description");

    hist.undo();
    check(hist.undoCount() == 0, "empty group undone without crash");
    hist.redo();
    check(hist.undoCount() == 1, "empty group redone without crash");
  }

  // Test 5: Large group (10 actions)
  {
    dc::CommandHistory hist;
    int values[10] = {};

    dc::UndoGroup group("batch update");
    for (int i = 0; i < 10; ++i) {
      int idx = i;
      group.addAction({"set " + std::to_string(i),
        [&values, idx]() { values[idx] = idx + 1; },
        [&values, idx]() { values[idx] = 0; }
      });
    }
    hist.execute(group.toAction());

    bool allSet = true;
    for (int i = 0; i < 10; ++i) {
      if (values[i] != i + 1) { allSet = false; break; }
    }
    check(allSet, "all 10 values set by group execute");

    hist.undo();
    bool allZero = true;
    for (int i = 0; i < 10; ++i) {
      if (values[i] != 0) { allZero = false; break; }
    }
    check(allZero, "all 10 values reverted by group undo");

    hist.redo();
    bool allSetAgain = true;
    for (int i = 0; i < 10; ++i) {
      if (values[i] != i + 1) { allSetAgain = false; break; }
    }
    check(allSetAgain, "all 10 values restored by group redo");
  }

  std::printf("=== D61.2: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
