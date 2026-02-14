// D19.1 — CommandHistory: undo/redo stack

#include "dc/commands/CommandHistory.hpp"
#include "dc/drawing/DrawingStore.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: execute + undo + redo flow ----
  {
    dc::CommandHistory hist;
    int value = 0;

    hist.execute({"set to 10",
      [&]() { value = 10; },
      [&]() { value = 0; }
    });
    requireTrue(value == 10, "execute sets value to 10");
    requireTrue(hist.undoCount() == 1, "undo stack has 1");
    requireTrue(hist.redoCount() == 0, "redo stack empty");

    hist.undo();
    requireTrue(value == 0, "undo reverts value to 0");
    requireTrue(hist.undoCount() == 0, "undo stack empty after undo");
    requireTrue(hist.redoCount() == 1, "redo stack has 1");

    hist.redo();
    requireTrue(value == 10, "redo restores value to 10");
    requireTrue(hist.undoCount() == 1, "undo stack has 1 after redo");
    requireTrue(hist.redoCount() == 0, "redo stack empty after redo");
    std::printf("  Test 1 (execute + undo + redo): PASS\n");
  }

  // ---- Test 2: redo cleared on new execute ----
  {
    dc::CommandHistory hist;
    int value = 0;

    hist.execute({"set 1", [&]() { value = 1; }, [&]() { value = 0; }});
    hist.execute({"set 2", [&]() { value = 2; }, [&]() { value = 1; }});
    requireTrue(value == 2, "value is 2");
    requireTrue(hist.undoCount() == 2, "2 on undo stack");

    hist.undo();
    requireTrue(value == 1, "undo to 1");
    requireTrue(hist.redoCount() == 1, "1 on redo stack");

    // New execute should clear redo
    hist.execute({"set 5", [&]() { value = 5; }, [&]() { value = 1; }});
    requireTrue(value == 5, "value is 5 after new execute");
    requireTrue(hist.redoCount() == 0, "redo cleared after new execute");
    requireTrue(hist.undoCount() == 2, "undo stack has 2 (set 1 + set 5)");
    std::printf("  Test 2 (redo cleared on new execute): PASS\n");
  }

  // ---- Test 3: canUndo/canRedo state ----
  {
    dc::CommandHistory hist;
    requireTrue(!hist.canUndo(), "initially cannot undo");
    requireTrue(!hist.canRedo(), "initially cannot redo");
    requireTrue(hist.undoDescription().empty(), "no undo description initially");
    requireTrue(hist.redoDescription().empty(), "no redo description initially");

    // undo/redo on empty stacks return false
    requireTrue(!hist.undo(), "undo on empty returns false");
    requireTrue(!hist.redo(), "redo on empty returns false");

    int dummy = 0;
    hist.execute({"action A", [&]() { dummy = 1; }, [&]() { dummy = 0; }});
    requireTrue(hist.canUndo(), "can undo after execute");
    requireTrue(!hist.canRedo(), "cannot redo after execute");
    requireTrue(hist.undoDescription() == "action A", "undo description is action A");

    hist.undo();
    requireTrue(!hist.canUndo(), "cannot undo after undoing only action");
    requireTrue(hist.canRedo(), "can redo after undo");
    requireTrue(hist.redoDescription() == "action A", "redo description is action A");
    std::printf("  Test 3 (canUndo/canRedo state): PASS\n");
  }

  // ---- Test 4: undo/redo with DrawingStore ----
  {
    dc::DrawingStore store;
    dc::CommandHistory hist;

    std::uint32_t addedId = 0;
    hist.execute({"add trendline",
      [&]() { addedId = store.addTrendline(10.0, 50.0, 20.0, 60.0); },
      [&]() { store.remove(addedId); }
    });

    requireTrue(store.count() == 1, "1 drawing after execute");
    requireTrue(store.get(addedId) != nullptr, "drawing exists");
    requireTrue(store.get(addedId)->x0 == 10.0, "x0 correct");

    hist.undo();
    requireTrue(store.count() == 0, "0 drawings after undo");
    requireTrue(store.get(addedId) == nullptr, "drawing gone after undo");

    hist.redo();
    // Redo calls execute again, which calls addTrendline — gets a new ID
    requireTrue(store.count() == 1, "1 drawing after redo");
    // The addedId is updated by the lambda
    requireTrue(store.get(addedId) != nullptr, "drawing exists after redo");
    requireTrue(store.get(addedId)->type == dc::DrawingType::Trendline, "type preserved");
    std::printf("  Test 4 (undo/redo with DrawingStore): PASS\n");
  }

  // ---- Test 5: clear ----
  {
    dc::CommandHistory hist;
    int v = 0;
    hist.execute({"a", [&]() { v = 1; }, [&]() { v = 0; }});
    hist.execute({"b", [&]() { v = 2; }, [&]() { v = 1; }});
    hist.undo();
    requireTrue(hist.undoCount() == 1, "1 on undo before clear");
    requireTrue(hist.redoCount() == 1, "1 on redo before clear");

    hist.clear();
    requireTrue(hist.undoCount() == 0, "undo empty after clear");
    requireTrue(hist.redoCount() == 0, "redo empty after clear");
    requireTrue(!hist.canUndo(), "cannot undo after clear");
    requireTrue(!hist.canRedo(), "cannot redo after clear");
    std::printf("  Test 5 (clear): PASS\n");
  }

  std::printf("D19.1 undo_redo: ALL PASS\n");
  return 0;
}
