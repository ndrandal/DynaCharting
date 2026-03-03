// D61.1 -- CommandHistory + UndoableAction: color change undo/redo

#include "dc/commands/CommandHistory.hpp"

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
  std::printf("=== D61.1 Undo Command Tests ===\n");

  // Test 1: Execute changes color, undo reverts, redo re-applies
  {
    dc::CommandHistory hist;
    float color[4] = {1.0f, 1.0f, 1.0f, 1.0f}; // white

    float oldR = color[0], oldG = color[1], oldB = color[2], oldA = color[3];

    hist.execute({"set color to red",
      [&]() { color[0] = 1.0f; color[1] = 0.0f; color[2] = 0.0f; color[3] = 1.0f; },
      [&, oldR, oldG, oldB, oldA]() { color[0] = oldR; color[1] = oldG; color[2] = oldB; color[3] = oldA; }
    });

    check(color[0] == 1.0f && color[1] == 0.0f && color[2] == 0.0f,
          "execute sets color to red");
    check(hist.undoCount() == 1, "undo stack has 1");
    check(hist.redoCount() == 0, "redo stack empty");

    hist.undo();
    check(color[0] == 1.0f && color[1] == 1.0f && color[2] == 1.0f,
          "undo reverts color to white");
    check(hist.undoCount() == 0, "undo stack empty after undo");
    check(hist.redoCount() == 1, "redo stack has 1");

    hist.redo();
    check(color[0] == 1.0f && color[1] == 0.0f && color[2] == 0.0f,
          "redo re-applies red color");
    check(hist.undoCount() == 1, "undo stack has 1 after redo");
    check(hist.redoCount() == 0, "redo stack empty after redo");
  }

  // Test 2: Multiple color changes with undo chain
  {
    dc::CommandHistory hist;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    // Set to red
    hist.execute({"set red",
      [&]() { r = 1.0f; g = 0.0f; b = 0.0f; },
      [&]() { r = 0.0f; g = 0.0f; b = 0.0f; }
    });
    check(r == 1.0f, "first: red");

    // Set to green
    hist.execute({"set green",
      [&]() { r = 0.0f; g = 1.0f; b = 0.0f; },
      [&]() { r = 1.0f; g = 0.0f; b = 0.0f; }
    });
    check(g == 1.0f, "second: green");

    // Set to blue
    hist.execute({"set blue",
      [&]() { r = 0.0f; g = 0.0f; b = 1.0f; },
      [&]() { r = 0.0f; g = 1.0f; b = 0.0f; }
    });
    check(b == 1.0f, "third: blue");
    check(hist.undoCount() == 3, "3 on undo stack");

    // Undo back to green
    hist.undo();
    check(r == 0.0f && g == 1.0f && b == 0.0f, "undo 1: back to green");

    // Undo back to red
    hist.undo();
    check(r == 1.0f && g == 0.0f && b == 0.0f, "undo 2: back to red");

    // Redo to green
    hist.redo();
    check(r == 0.0f && g == 1.0f && b == 0.0f, "redo 1: forward to green");

    // New action clears redo
    hist.execute({"set yellow",
      [&]() { r = 1.0f; g = 1.0f; b = 0.0f; },
      [&]() { r = 0.0f; g = 1.0f; b = 0.0f; }
    });
    check(r == 1.0f && g == 1.0f && b == 0.0f, "new action: yellow");
    check(hist.redoCount() == 0, "redo cleared by new action");
    check(hist.undoCount() == 3, "undo stack: red + green + yellow");
  }

  // Test 3: Description tracking
  {
    dc::CommandHistory hist;
    float v = 0;

    hist.execute({"change opacity",
      [&]() { v = 0.5f; },
      [&]() { v = 0.0f; }
    });
    check(hist.undoDescription() == "change opacity", "undo description matches");

    hist.undo();
    check(hist.redoDescription() == "change opacity", "redo description matches");
  }

  std::printf("=== D61.1: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
