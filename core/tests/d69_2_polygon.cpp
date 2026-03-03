// D69.2 — ExtDrawingInteraction: polygon, polyline, remove, clear

#include "dc/drawing/ExtendedDrawings.hpp"

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
  std::printf("=== D69.2 ExtDrawingInteraction: Polygon/Polyline ===\n");

  // ---- Test 1: Polygon — multiple clicks + double-click to finalize ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polygon);
    check(ei.isActive(), "polygon: active");
    check(dc::requiredClicks(dc::ExtDrawingType::Polygon) == 0,
          "polygon: variable clicks (0)");

    auto r1 = ei.onClick(0.0, 0.0);
    check(r1 == 0, "polygon: click 1 returns 0");
    auto r2 = ei.onClick(100.0, 0.0);
    check(r2 == 0, "polygon: click 2 returns 0");
    auto r3 = ei.onClick(100.0, 100.0);
    check(r3 == 0, "polygon: click 3 returns 0");
    check(ei.currentPointCount() == 3, "polygon: 3 temp points");

    // Double-click adds point 4 and finalizes
    auto id = ei.onDoubleClick(0.0, 100.0);
    check(id > 0, "polygon: double-click finalizes");
    check(!ei.isActive(), "polygon: not active after finalize");

    const auto* d = ei.get(id);
    check(d != nullptr, "polygon: drawing exists");
    check(d->type == dc::ExtDrawingType::Polygon, "polygon: correct type");
    check(d->pointCount() == 4, "polygon: 4 points (3 clicks + 1 double-click)");
    check(d->pointsX[0] == 0.0, "polygon: x0");
    check(d->pointsY[0] == 0.0, "polygon: y0");
    check(d->pointsX[1] == 100.0, "polygon: x1");
    check(d->pointsY[1] == 0.0, "polygon: y1");
    check(d->pointsX[2] == 100.0, "polygon: x2");
    check(d->pointsY[2] == 100.0, "polygon: y2");
    check(d->pointsX[3] == 0.0, "polygon: x3");
    check(d->pointsY[3] == 100.0, "polygon: y3");
  }

  // ---- Test 2: Polyline — multiple clicks + double-click ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polyline);
    check(dc::requiredClicks(dc::ExtDrawingType::Polyline) == 0,
          "polyline: variable clicks (0)");

    ei.onClick(10.0, 10.0);
    ei.onClick(20.0, 30.0);
    ei.onClick(30.0, 10.0);
    ei.onClick(40.0, 30.0);
    ei.onClick(50.0, 10.0);
    check(ei.currentPointCount() == 5, "polyline: 5 temp points");

    auto id = ei.onDoubleClick(60.0, 30.0);
    check(id > 0, "polyline: double-click finalizes");

    const auto* d = ei.get(id);
    check(d != nullptr, "polyline: drawing exists");
    check(d->type == dc::ExtDrawingType::Polyline, "polyline: correct type");
    check(d->pointCount() == 6, "polyline: 6 points (5 clicks + 1 double-click)");
  }

  // ---- Test 3: Polygon with minimum points (1 click + double-click = 2 points) ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polygon);
    ei.onClick(0.0, 0.0);
    auto id = ei.onDoubleClick(100.0, 100.0);
    check(id > 0, "polygon-min: 2 points is enough");

    const auto* d = ei.get(id);
    check(d->pointCount() == 2, "polygon-min: 2 points stored");
  }

  // ---- Test 4: Double-click with no prior clicks — only 1 point, not enough ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polygon);
    auto id = ei.onDoubleClick(50.0, 50.0);
    check(id == 0, "polygon-1pt: double-click with 0 prior clicks (1 total) returns 0");
    check(ei.isActive(), "polygon-1pt: still active (need more points)");
  }

  // ---- Test 5: Double-click on fixed-point type does nothing ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(0.0, 0.0);
    auto id = ei.onDoubleClick(10.0, 10.0);
    check(id == 0, "arrow-dblclick: double-click on fixed-type returns 0");
    check(ei.isActive(), "arrow-dblclick: still active");
    // Can still finish normally with onClick
    auto id2 = ei.onClick(10.0, 10.0);
    check(id2 > 0, "arrow-dblclick: onClick still works to complete");
  }

  // ---- Test 6: remove() ----
  {
    dc::ExtDrawingInteraction ei;

    // Create 3 drawings
    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(0.0, 0.0);
    auto id1 = ei.onClick(1.0, 1.0);

    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(2.0, 2.0);
    auto id2 = ei.onClick(3.0, 3.0);

    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(4.0, 4.0);
    auto id3 = ei.onClick(5.0, 5.0);

    check(ei.drawings().size() == 3, "remove: 3 drawings");

    ei.remove(id2);
    check(ei.drawings().size() == 2, "remove: 2 drawings after remove");
    check(ei.get(id2) == nullptr, "remove: removed drawing not found");
    check(ei.get(id1) != nullptr, "remove: other drawing 1 still exists");
    check(ei.get(id3) != nullptr, "remove: other drawing 3 still exists");
  }

  // ---- Test 7: clear() ----
  {
    dc::ExtDrawingInteraction ei;

    ei.begin(dc::ExtDrawingType::Ray);
    ei.onClick(0.0, 0.0);
    ei.onClick(1.0, 1.0);

    ei.begin(dc::ExtDrawingType::Ray);
    ei.onClick(2.0, 2.0);
    ei.onClick(3.0, 3.0);

    check(ei.drawings().size() == 2, "clear: 2 drawings before clear");

    ei.clear();
    check(ei.drawings().empty(), "clear: 0 drawings after clear");
  }

  // ---- Test 8: Default color and lineWidth ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::AnchoredNote);
    auto id = ei.onClick(5.0, 5.0);
    const auto* d = ei.get(id);
    check(d != nullptr, "defaults: drawing exists");
    check(d->color[0] == 1.0f && d->color[1] == 1.0f &&
          d->color[2] == 0.0f && d->color[3] == 1.0f,
          "defaults: color is yellow (1,1,0,1)");
    check(d->lineWidth == 2.0f, "defaults: lineWidth is 2.0");
    check(d->text.empty(), "defaults: text is empty");
  }

  // ---- Test 9: Cancel polygon mid-placement ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polygon);
    ei.onClick(0.0, 0.0);
    ei.onClick(10.0, 0.0);
    ei.onClick(10.0, 10.0);
    check(ei.currentPointCount() == 3, "cancel-poly: 3 points accumulated");

    ei.cancel();
    check(!ei.isActive(), "cancel-poly: not active");
    check(ei.drawings().empty(), "cancel-poly: no drawings created");
  }

  // ---- Test 10: requiredClicks for all types ----
  {
    check(dc::requiredClicks(dc::ExtDrawingType::AnchoredNote) == 1, "clicks: AnchoredNote=1");
    check(dc::requiredClicks(dc::ExtDrawingType::Ray) == 2, "clicks: Ray=2");
    check(dc::requiredClicks(dc::ExtDrawingType::ExtendedLine) == 2, "clicks: ExtendedLine=2");
    check(dc::requiredClicks(dc::ExtDrawingType::Arrow) == 2, "clicks: Arrow=2");
    check(dc::requiredClicks(dc::ExtDrawingType::Pitchfork) == 3, "clicks: Pitchfork=3");
    check(dc::requiredClicks(dc::ExtDrawingType::ParallelChannel) == 3, "clicks: ParallelChannel=3");
    check(dc::requiredClicks(dc::ExtDrawingType::FibExtension) == 3, "clicks: FibExtension=3");
    check(dc::requiredClicks(dc::ExtDrawingType::Polygon) == 0, "clicks: Polygon=0");
    check(dc::requiredClicks(dc::ExtDrawingType::Polyline) == 0, "clicks: Polyline=0");
  }

  // ---- Test 11: Large polygon (many points) ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Polygon);
    for (int i = 0; i < 20; ++i) {
      auto r = ei.onClick(static_cast<double>(i), static_cast<double>(i * 2));
      check(r == 0, "large-poly: each click returns 0");
    }
    check(ei.currentPointCount() == 20, "large-poly: 20 temp points");

    auto id = ei.onDoubleClick(99.0, 99.0);
    check(id > 0, "large-poly: double-click finalizes");

    const auto* d = ei.get(id);
    check(d->pointCount() == 21, "large-poly: 21 points (20 + 1)");
    check(d->pointsX[0] == 0.0, "large-poly: first x");
    check(d->pointsX[20] == 99.0, "large-poly: last x");
  }

  std::printf("=== D69.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
