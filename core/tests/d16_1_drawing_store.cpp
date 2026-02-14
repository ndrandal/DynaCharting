// D16.1-D16.3 — DrawingStore: trendlines + horizontal levels

#include "dc/drawing/DrawingStore.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: Add trendline ----
  {
    dc::DrawingStore store;
    auto id = store.addTrendline(10.0, 50.0, 20.0, 60.0);
    requireTrue(id == 1, "first ID is 1");
    requireTrue(store.count() == 1, "1 drawing");

    const auto* d = store.get(id);
    requireTrue(d != nullptr, "drawing exists");
    requireTrue(d->type == dc::DrawingType::Trendline, "is trendline");
    requireTrue(d->x0 == 10.0, "x0");
    requireTrue(d->y0 == 50.0, "y0");
    requireTrue(d->x1 == 20.0, "x1");
    requireTrue(d->y1 == 60.0, "y1");
    std::printf("  Test 1 (trendline): PASS\n");
  }

  // ---- Test 2: Add horizontal level ----
  {
    dc::DrawingStore store;
    auto id = store.addHorizontalLevel(100.0);
    requireTrue(id == 1, "ID is 1");

    const auto* d = store.get(id);
    requireTrue(d->type == dc::DrawingType::HorizontalLevel, "is h-level");
    requireTrue(d->y0 == 100.0, "price");
    std::printf("  Test 2 (horizontal level): PASS\n");
  }

  // ---- Test 3: Multiple + remove ----
  {
    dc::DrawingStore store;
    auto id1 = store.addTrendline(0, 0, 10, 10);
    auto id2 = store.addHorizontalLevel(50.0);
    auto id3 = store.addTrendline(5, 5, 15, 15);
    requireTrue(store.count() == 3, "3 drawings");

    store.remove(id2);
    requireTrue(store.count() == 2, "2 after remove");
    requireTrue(store.get(id2) == nullptr, "removed drawing gone");
    requireTrue(store.get(id1) != nullptr, "id1 still there");
    requireTrue(store.get(id3) != nullptr, "id3 still there");
    std::printf("  Test 3 (remove): PASS\n");
  }

  // ---- Test 4: Set color + line width ----
  {
    dc::DrawingStore store;
    auto id = store.addTrendline(0, 0, 1, 1);
    store.setColor(id, 1.0f, 0.0f, 0.0f, 1.0f);
    store.setLineWidth(id, 3.0f);

    const auto* d = store.get(id);
    requireTrue(d->color[0] == 1.0f, "red");
    requireTrue(d->lineWidth == 3.0f, "line width");
    std::printf("  Test 4 (style): PASS\n");
  }

  // ---- Test 5: Clear ----
  {
    dc::DrawingStore store;
    store.addTrendline(0, 0, 1, 1);
    store.addHorizontalLevel(50);
    store.clear();
    requireTrue(store.count() == 0, "cleared");
    std::printf("  Test 5 (clear): PASS\n");
  }

  std::printf("D16.1-D16.3 drawing_store: ALL PASS\n");
  return 0;
}
