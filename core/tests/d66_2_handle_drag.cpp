// D66.2 -- HandleSet drag workflow: beginDrag, updateDrag, endDrag, getModifiedCoords
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

static bool near(double a, double b, double tol = 1e-6) {
  return std::fabs(a - b) < tol;
}

int main() {
  std::printf("=== D66.2 HandleSet Drag Tests ===\n");

  // Test 1: Trendline endpoint drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 100.0, 100.0);

    // Find start handle (pointIndex 0)
    std::uint32_t startId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) startId = h.id;
    }
    check(startId != 0, "trendline: found start handle");

    hs.beginDrag(startId);
    const dc::Handle* h = hs.getHandle(startId);
    check(h && h->dragging, "beginDrag: handle is dragging");

    std::uint32_t modId = hs.updateDrag(startId, 10.0, 20.0);
    check(modId == 1, "updateDrag: returns correct drawingId");

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0), "drag start: x0 updated to 10");
    check(near(coords.y0, 20.0), "drag start: y0 updated to 20");
    check(near(coords.x1, 100.0), "drag start: x1 unchanged");
    check(near(coords.y1, 100.0), "drag start: y1 unchanged");

    // Verify midpoint updated
    for (auto& hh : hs.handles()) {
      if (hh.drawingId == 1 && hh.pointIndex == 2) {
        check(near(hh.x, 55.0), "drag start: midpoint x = (10+100)/2");
        check(near(hh.y, 60.0), "drag start: midpoint y = (20+100)/2");
      }
    }

    hs.endDrag(startId);
    h = hs.getHandle(startId);
    check(h && !h->dragging, "endDrag: handle no longer dragging");
  }

  // Test 2: Trendline midpoint drag (moves entire drawing)
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 100.0, 100.0);
    // Midpoint at (50, 50)

    std::uint32_t midId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 2) midId = h.id;
    }
    check(midId != 0, "trendline: found midpoint handle");

    hs.beginDrag(midId);
    // Move midpoint from (50,50) to (60,70) -> delta = (10, 20)
    hs.updateDrag(midId, 60.0, 70.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0), "midpoint drag: x0 shifted by +10");
    check(near(coords.y0, 20.0), "midpoint drag: y0 shifted by +20");
    check(near(coords.x1, 110.0), "midpoint drag: x1 shifted by +10");
    check(near(coords.y1, 120.0), "midpoint drag: y1 shifted by +20");

    // Verify all handle positions shifted
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) {
        check(near(h.x, 10.0) && near(h.y, 20.0), "midpoint drag: start handle shifted");
      }
      if (h.drawingId == 1 && h.pointIndex == 1) {
        check(near(h.x, 110.0) && near(h.y, 120.0), "midpoint drag: end handle shifted");
      }
    }
    hs.endDrag(midId);
  }

  // Test 3: Trendline end point drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Trendline*/1, 10.0, 20.0, 50.0, 80.0);

    std::uint32_t endId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 1) endId = h.id;
    }

    hs.beginDrag(endId);
    hs.updateDrag(endId, 90.0, 90.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0) && near(coords.y0, 20.0),
          "end drag: start unchanged");
    check(near(coords.x1, 90.0) && near(coords.y1, 90.0),
          "end drag: end moved to (90,90)");
    hs.endDrag(endId);
  }

  // Test 4: HorizontalLevel drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*HorizontalLevel*/2, 0.0, 100.0, 0.0, 0.0);

    std::uint32_t hId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) hId = h.id;
    }

    hs.beginDrag(hId);
    hs.updateDrag(hId, 50.0, 150.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.y0, 150.0), "horiz level: y moved to 150");
    hs.endDrag(hId);
  }

  // Test 5: VerticalLine drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*VerticalLine*/3, 50.0, 0.0, 0.0, 0.0);

    std::uint32_t hId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) hId = h.id;
    }

    hs.beginDrag(hId);
    hs.updateDrag(hId, 75.0, 0.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 75.0), "vert line: x moved to 75");
    hs.endDrag(hId);
  }

  // Test 6: Rectangle corner drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Rectangle*/4, 10.0, 20.0, 110.0, 120.0);

    // Drag bottom-right corner (pointIndex 2) from (110,120) to (200,200)
    std::uint32_t cornerId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 2) cornerId = h.id;
    }

    hs.beginDrag(cornerId);
    hs.updateDrag(cornerId, 200.0, 200.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0), "rect corner drag: x0 unchanged");
    check(near(coords.y0, 20.0), "rect corner drag: y0 unchanged");
    check(near(coords.x1, 200.0), "rect corner drag: x1 = 200");
    check(near(coords.y1, 200.0), "rect corner drag: y1 = 200");

    // Verify center handle updated
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 8) {
        check(near(h.x, 105.0) && near(h.y, 110.0),
              "rect corner drag: center updated to (105,110)");
      }
    }
    hs.endDrag(cornerId);
  }

  // Test 7: Rectangle center drag (move entire)
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Rectangle*/4, 0.0, 0.0, 100.0, 100.0);
    // Center at (50, 50)

    std::uint32_t centerId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 8) centerId = h.id;
    }

    hs.beginDrag(centerId);
    // Move center from (50,50) to (60,70) -> delta (10,20)
    hs.updateDrag(centerId, 60.0, 70.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0), "rect center drag: x0 += 10");
    check(near(coords.y0, 20.0), "rect center drag: y0 += 20");
    check(near(coords.x1, 110.0), "rect center drag: x1 += 10");
    check(near(coords.y1, 120.0), "rect center drag: y1 += 20");
    hs.endDrag(centerId);
  }

  // Test 8: Rectangle edge midpoint drag (constrained axis)
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Rectangle*/4, 0.0, 0.0, 100.0, 100.0);

    // Top edge midpoint (pointIndex 4) at (50, 0): constrain to Y only
    std::uint32_t topEdgeId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 4) topEdgeId = h.id;
    }

    hs.beginDrag(topEdgeId);
    // Move to (60, -20): only y0 should change
    hs.updateDrag(topEdgeId, 60.0, -20.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 0.0), "rect top edge: x0 unchanged");
    check(near(coords.y0, -20.0), "rect top edge: y0 = -20");
    check(near(coords.x1, 100.0), "rect top edge: x1 unchanged");
    check(near(coords.y1, 100.0), "rect top edge: y1 unchanged");
    hs.endDrag(topEdgeId);
  }

  // Test 9: Rectangle right edge midpoint drag (constrained X axis)
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Rectangle*/4, 0.0, 0.0, 100.0, 100.0);

    // Right edge midpoint (pointIndex 5) at (100, 50): constrain to X only
    std::uint32_t rightEdgeId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 5) rightEdgeId = h.id;
    }

    hs.beginDrag(rightEdgeId);
    hs.updateDrag(rightEdgeId, 150.0, 60.0);

    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x1, 150.0), "rect right edge: x1 = 150");
    check(near(coords.y0, 0.0), "rect right edge: y0 unchanged");
    check(near(coords.y1, 100.0), "rect right edge: y1 unchanged");
    hs.endDrag(rightEdgeId);
  }

  // Test 10: FibRetracement drag
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*FibRetracement*/5, 0.0, 100.0, 200.0, 300.0);

    std::uint32_t startId = 0, endId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 0) startId = h.id;
      if (h.drawingId == 1 && h.pointIndex == 1) endId = h.id;
    }

    hs.beginDrag(startId);
    hs.updateDrag(startId, 10.0, 110.0);
    auto coords = hs.getModifiedCoords(1);
    check(near(coords.x0, 10.0) && near(coords.y0, 110.0),
          "fib: start moved");
    check(near(coords.x1, 200.0) && near(coords.y1, 300.0),
          "fib: end unchanged");
    hs.endDrag(startId);

    hs.beginDrag(endId);
    hs.updateDrag(endId, 250.0, 350.0);
    coords = hs.getModifiedCoords(1);
    check(near(coords.x1, 250.0) && near(coords.y1, 350.0),
          "fib: end moved");
    hs.endDrag(endId);
  }

  // Test 11: updateDrag on invalid handle returns 0
  {
    dc::HandleSet hs;
    std::uint32_t result = hs.updateDrag(999, 0.0, 0.0);
    check(result == 0, "updateDrag invalid handle: returns 0");
  }

  // Test 12: getModifiedCoords on unknown drawingId returns zeros
  {
    dc::HandleSet hs;
    auto coords = hs.getModifiedCoords(999);
    check(coords.x0 == 0 && coords.y0 == 0 && coords.x1 == 0 && coords.y1 == 0,
          "getModifiedCoords unknown: returns zeros");
  }

  // Test 13: Sequential drags accumulate
  {
    dc::HandleSet hs;
    hs.createForDrawing(1, /*Trendline*/1, 0.0, 0.0, 100.0, 100.0);

    // Find midpoint
    std::uint32_t midId = 0;
    for (auto& h : hs.handles()) {
      if (h.drawingId == 1 && h.pointIndex == 2) midId = h.id;
    }

    // First drag: move midpoint from (50,50) to (60,60) -> delta (10,10)
    hs.beginDrag(midId);
    hs.updateDrag(midId, 60.0, 60.0);
    hs.endDrag(midId);

    // Second drag: midpoint now at (60,60), move to (80,80) -> delta (20,20)
    hs.beginDrag(midId);
    hs.updateDrag(midId, 80.0, 80.0);
    hs.endDrag(midId);

    auto coords = hs.getModifiedCoords(1);
    // Original: (0,0)-(100,100), total shift = (30,30)
    check(near(coords.x0, 30.0), "sequential drags: x0 accumulated");
    check(near(coords.y0, 30.0), "sequential drags: y0 accumulated");
    check(near(coords.x1, 130.0), "sequential drags: x1 accumulated");
    check(near(coords.y1, 130.0), "sequential drags: y1 accumulated");
  }

  std::printf("=== D66.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
