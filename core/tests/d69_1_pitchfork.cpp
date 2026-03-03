// D69.1 — ExtDrawingInteraction: pitchfork, ray, arrow, cancel

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
  std::printf("=== D69.1 ExtDrawingInteraction: Pitchfork/Ray/Arrow ===\n");

  // ---- Test 1: Pitchfork — 3 clicks to complete ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Pitchfork);
    check(ei.isActive(), "pitchfork: active after begin");
    check(ei.activeType() == dc::ExtDrawingType::Pitchfork, "pitchfork: correct type");

    auto r1 = ei.onClick(10.0, 20.0);
    check(r1 == 0, "pitchfork: click 1 returns 0");
    check(ei.currentPointCount() == 1, "pitchfork: 1 point after click 1");

    auto r2 = ei.onClick(30.0, 40.0);
    check(r2 == 0, "pitchfork: click 2 returns 0");
    check(ei.currentPointCount() == 2, "pitchfork: 2 points after click 2");

    auto r3 = ei.onClick(50.0, 60.0);
    check(r3 > 0, "pitchfork: click 3 completes drawing");
    check(!ei.isActive(), "pitchfork: not active after completion");

    const auto* d = ei.get(r3);
    check(d != nullptr, "pitchfork: drawing exists");
    check(d->type == dc::ExtDrawingType::Pitchfork, "pitchfork: type is Pitchfork");
    check(d->pointCount() == 3, "pitchfork: 3 points stored");
    check(d->pointsX[0] == 10.0, "pitchfork: x0");
    check(d->pointsY[0] == 20.0, "pitchfork: y0");
    check(d->pointsX[1] == 30.0, "pitchfork: x1");
    check(d->pointsY[1] == 40.0, "pitchfork: y1");
    check(d->pointsX[2] == 50.0, "pitchfork: x2");
    check(d->pointsY[2] == 60.0, "pitchfork: y2");
  }

  // ---- Test 2: Cancel mid-placement ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Pitchfork);
    ei.onClick(1.0, 2.0);
    ei.onClick(3.0, 4.0);
    check(ei.isActive(), "cancel: still active after 2 clicks");
    check(ei.currentPointCount() == 2, "cancel: 2 temp points");

    ei.cancel();
    check(!ei.isActive(), "cancel: not active after cancel");
    check(ei.drawings().empty(), "cancel: no drawings created");
  }

  // ---- Test 3: Ray — 2 clicks ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Ray);
    check(ei.isActive(), "ray: active");
    check(dc::requiredClicks(dc::ExtDrawingType::Ray) == 2, "ray: requires 2 clicks");

    auto r1 = ei.onClick(0.0, 0.0);
    check(r1 == 0, "ray: click 1 returns 0");

    auto r2 = ei.onClick(100.0, 50.0);
    check(r2 > 0, "ray: click 2 completes");
    check(!ei.isActive(), "ray: not active after completion");

    const auto* d = ei.get(r2);
    check(d != nullptr, "ray: drawing exists");
    check(d->type == dc::ExtDrawingType::Ray, "ray: type is Ray");
    check(d->pointCount() == 2, "ray: 2 points");
    check(d->pointsX[0] == 0.0, "ray: x0");
    check(d->pointsX[1] == 100.0, "ray: x1");
  }

  // ---- Test 4: Arrow — 2 clicks ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Arrow);
    check(dc::requiredClicks(dc::ExtDrawingType::Arrow) == 2, "arrow: requires 2 clicks");

    auto r1 = ei.onClick(5.0, 10.0);
    check(r1 == 0, "arrow: click 1 returns 0");

    auto r2 = ei.onClick(25.0, 30.0);
    check(r2 > 0, "arrow: click 2 completes");

    const auto* d = ei.get(r2);
    check(d != nullptr, "arrow: drawing exists");
    check(d->type == dc::ExtDrawingType::Arrow, "arrow: type is Arrow");
    check(d->pointCount() == 2, "arrow: 2 points");
    check(d->pointsX[0] == 5.0, "arrow: x0");
    check(d->pointsY[0] == 10.0, "arrow: y0");
    check(d->pointsX[1] == 25.0, "arrow: x1");
    check(d->pointsY[1] == 30.0, "arrow: y1");
  }

  // ---- Test 5: ExtendedLine — 2 clicks ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::ExtendedLine);
    check(dc::requiredClicks(dc::ExtDrawingType::ExtendedLine) == 2,
          "extline: requires 2 clicks");

    ei.onClick(10.0, 20.0);
    auto id = ei.onClick(30.0, 40.0);
    check(id > 0, "extline: 2 clicks completes");

    const auto* d = ei.get(id);
    check(d->type == dc::ExtDrawingType::ExtendedLine, "extline: correct type");
    check(d->pointCount() == 2, "extline: 2 points");
  }

  // ---- Test 6: AnchoredNote — 1 click ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::AnchoredNote);
    check(dc::requiredClicks(dc::ExtDrawingType::AnchoredNote) == 1,
          "note: requires 1 click");

    auto id = ei.onClick(42.0, 99.0);
    check(id > 0, "note: 1 click completes");
    check(!ei.isActive(), "note: not active after completion");

    const auto* d = ei.get(id);
    check(d != nullptr, "note: drawing exists");
    check(d->type == dc::ExtDrawingType::AnchoredNote, "note: correct type");
    check(d->pointCount() == 1, "note: 1 point");
    check(d->pointsX[0] == 42.0, "note: x0");
    check(d->pointsY[0] == 99.0, "note: y0");
  }

  // ---- Test 7: FibExtension — 3 clicks ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::FibExtension);
    check(dc::requiredClicks(dc::ExtDrawingType::FibExtension) == 3,
          "fibext: requires 3 clicks");

    ei.onClick(10.0, 100.0);
    ei.onClick(20.0, 50.0);
    auto id = ei.onClick(30.0, 75.0);
    check(id > 0, "fibext: 3 clicks completes");

    const auto* d = ei.get(id);
    check(d->pointCount() == 3, "fibext: 3 points");
    check(d->type == dc::ExtDrawingType::FibExtension, "fibext: correct type");
  }

  // ---- Test 8: ParallelChannel — 3 clicks ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::ParallelChannel);
    check(dc::requiredClicks(dc::ExtDrawingType::ParallelChannel) == 3,
          "channel: requires 3 clicks");

    ei.onClick(0.0, 0.0);
    ei.onClick(100.0, 0.0);
    auto id = ei.onClick(50.0, 20.0);
    check(id > 0, "channel: 3 clicks completes");

    const auto* d = ei.get(id);
    check(d->pointCount() == 3, "channel: 3 points");
  }

  // ---- Test 9: Preview coordinates ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Pitchfork);
    ei.updatePreview(77.5, 88.5);
    check(ei.previewX() == 77.5, "preview: x");
    check(ei.previewY() == 88.5, "preview: y");
  }

  // ---- Test 10: onClick while not active returns 0 ----
  {
    dc::ExtDrawingInteraction ei;
    auto r = ei.onClick(1.0, 2.0);
    check(r == 0, "inactive: onClick returns 0");
    check(!ei.isActive(), "inactive: not active");
  }

  // ---- Test 11: Multiple drawings get sequential IDs ----
  {
    dc::ExtDrawingInteraction ei;
    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(0.0, 0.0);
    auto id1 = ei.onClick(1.0, 1.0);

    ei.begin(dc::ExtDrawingType::Arrow);
    ei.onClick(2.0, 2.0);
    auto id2 = ei.onClick(3.0, 3.0);

    check(id1 > 0, "seq: first drawing has valid id");
    check(id2 == id1 + 1, "seq: second drawing id increments");
    check(ei.drawings().size() == 2, "seq: 2 drawings total");
  }

  std::printf("=== D69.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
