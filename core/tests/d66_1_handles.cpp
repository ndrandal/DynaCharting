// D66.1 -- HandleSet: create handles per drawing type, hit test, remove
#include "dc/interaction/HandleSet.hpp"

#include <cstdio>
#include <cmath>

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
  std::printf("=== D66.1 HandleSet Tests ===\n");

  dc::HandleSet hs;

  // Test 1: Trendline gets 3 handles (start, end, midpoint)
  {
    hs.createForDrawing(/*drawingId=*/10, /*type=Trendline*/1,
                        0.0, 0.0, 100.0, 200.0);
    int count = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 10) ++count;
    }
    check(count == 3, "trendline: 3 handles");

    // Verify endpoint positions
    bool foundStart = false, foundEnd = false, foundMid = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId != 10) continue;
      if (h.pointIndex == 0) {
        foundStart = (h.x == 0.0 && h.y == 0.0);
      } else if (h.pointIndex == 1) {
        foundEnd = (h.x == 100.0 && h.y == 200.0);
      } else if (h.pointIndex == 2) {
        foundMid = (h.x == 50.0 && h.y == 100.0);
      }
    }
    check(foundStart, "trendline: start handle at (0,0)");
    check(foundEnd, "trendline: end handle at (100,200)");
    check(foundMid, "trendline: midpoint handle at (50,100)");
  }

  // Test 2: Rectangle gets 9 handles
  {
    hs.createForDrawing(/*drawingId=*/20, /*type=Rectangle*/4,
                        10.0, 20.0, 110.0, 120.0);
    int count = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 20) ++count;
    }
    check(count == 9, "rectangle: 9 handles");

    // Verify center handle
    bool foundCenter = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 20 && h.pointIndex == 8) {
        foundCenter = (h.x == 60.0 && h.y == 70.0);
      }
    }
    check(foundCenter, "rectangle: center at (60,70)");

    // Verify corner 0 (top-left)
    bool foundTL = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 20 && h.pointIndex == 0) {
        foundTL = (h.x == 10.0 && h.y == 20.0);
      }
    }
    check(foundTL, "rectangle: top-left corner at (10,20)");

    // Verify corner 2 (bottom-right)
    bool foundBR = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 20 && h.pointIndex == 2) {
        foundBR = (h.x == 110.0 && h.y == 120.0);
      }
    }
    check(foundBR, "rectangle: bottom-right corner at (110,120)");
  }

  // Test 3: HorizontalLevel gets 1 handle
  {
    hs.createForDrawing(/*drawingId=*/30, /*type=HorizontalLevel*/2,
                        50.0, 150.0, 0.0, 0.0);
    int count = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 30) ++count;
    }
    check(count == 1, "horizontal level: 1 handle");

    bool foundPrice = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 30 && h.pointIndex == 0) {
        foundPrice = (h.y == 150.0);
      }
    }
    check(foundPrice, "horizontal level: handle at price 150");
  }

  // Test 4: VerticalLine gets 1 handle
  {
    hs.createForDrawing(/*drawingId=*/40, /*type=VerticalLine*/3,
                        75.0, 0.0, 0.0, 0.0);
    int count = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 40) ++count;
    }
    check(count == 1, "vertical line: 1 handle");
  }

  // Test 5: FibRetracement gets 2 handles
  {
    hs.createForDrawing(/*drawingId=*/50, /*type=FibRetracement*/5,
                        0.0, 100.0, 200.0, 300.0);
    int count = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 50) ++count;
    }
    check(count == 2, "fib retracement: 2 handles");
  }

  // Test 6: Hit test finds nearest handle within radius
  {
    dc::HandleSet hs2;
    hs2.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 100.0, 100.0);
    // Handles at (0,0), (100,100), (50,50)
    // pixelPerDataX = pixelPerDataY = 1.0 for simplicity

    // Hit near start (0,0) within 8px radius
    std::uint32_t hitId = hs2.hitTest(3.0, 4.0, 1.0, 1.0);
    // distance = sqrt(9+16) = 5.0, within 8.0
    check(hitId != 0, "hitTest: found handle near (3,4)");

    const dc::Handle* hitHandle = hs2.getHandle(hitId);
    check(hitHandle != nullptr && hitHandle->pointIndex == 0,
          "hitTest: nearest is start handle");

    // Hit far away from all handles
    hitId = hs2.hitTest(500.0, 500.0, 1.0, 1.0);
    check(hitId == 0, "hitTest: no handle at (500,500)");

    // Hit near midpoint (50,50)
    hitId = hs2.hitTest(52.0, 52.0, 1.0, 1.0);
    hitHandle = hs2.getHandle(hitId);
    check(hitHandle != nullptr && hitHandle->pointIndex == 2,
          "hitTest: nearest is midpoint handle");
  }

  // Test 7: Hit test with different pixel scale
  {
    dc::HandleSet hs3;
    hs3.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 10.0, 10.0);
    // Handle at (0,0), radius 8px.
    // If pixelPerDataX = 100 (each data unit = 100px),
    // then 0.05 data units = 5px, which is within 8px radius.
    std::uint32_t hitId = hs3.hitTest(0.05, 0.0, 100.0, 100.0);
    check(hitId != 0, "hitTest scaled: 0.05 data units = 5px within 8px");

    // 0.1 data units = 10px, outside 8px radius
    hitId = hs3.hitTest(0.1, 0.0, 100.0, 100.0);
    check(hitId == 0, "hitTest scaled: 0.1 data units = 10px outside 8px");
  }

  // Test 8: removeForDrawing clears handles
  {
    std::size_t before = hs.handles().size();
    hs.removeForDrawing(10); // remove trendline handles
    std::size_t after = hs.handles().size();
    check(after == before - 3, "removeForDrawing: removed 3 trendline handles");

    // Verify no handles for drawing 10 remain
    bool anyLeft = false;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 10) anyLeft = true;
    }
    check(!anyLeft, "removeForDrawing: no handles left for drawing 10");
  }

  // Test 9: clear removes everything
  {
    hs.clear();
    check(hs.handles().empty(), "clear: all handles removed");
  }

  // Test 10: getHandle returns nullptr for invalid id
  {
    check(hs.getHandle(999) == nullptr, "getHandle: null for invalid id");
  }

  // Test 11: createForDrawing replaces existing handles
  {
    dc::HandleSet hs4;
    hs4.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 10.0, 10.0);
    check(hs4.handles().size() == 3, "initial: 3 handles");
    // Re-create with new coords
    hs4.createForDrawing(1, /*Trendline*/1, 5.0, 5.0, 15.0, 15.0);
    check(hs4.handles().size() == 3, "re-create: still 3 handles");
    bool startMoved = false;
    for (auto& h : hs4.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) {
        startMoved = (h.x == 5.0 && h.y == 5.0);
      }
    }
    check(startMoved, "re-create: start handle at new position");
  }

  std::printf("=== D66.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
