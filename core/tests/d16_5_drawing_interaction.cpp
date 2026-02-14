// D16.5 — DrawingInteraction: state machine for creating drawings

#include "dc/drawing/DrawingInteraction.hpp"
#include "dc/drawing/DrawingStore.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: Trendline two-click flow ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    requireTrue(!interaction.isActive(), "starts idle");

    interaction.beginTrendline();
    requireTrue(interaction.isActive(), "active after begin");
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingTrendlineFirst, "placing first");

    auto id1 = interaction.onClick(10.0, 50.0, store);
    requireTrue(id1 == 0, "first click returns 0");
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingTrendlineSecond, "placing second");

    auto id2 = interaction.onClick(20.0, 60.0, store);
    requireTrue(id2 != 0, "second click returns drawing ID");
    requireTrue(!interaction.isActive(), "back to idle");

    requireTrue(store.count() == 1, "1 drawing created");
    const auto* d = store.get(id2);
    requireTrue(d->type == dc::DrawingType::Trendline, "is trendline");
    requireTrue(d->x0 == 10.0, "x0");
    requireTrue(d->y1 == 60.0, "y1");

    std::printf("  Test 1 (trendline flow): PASS\n");
  }

  // ---- Test 2: Horizontal level single-click ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    interaction.beginHorizontalLevel();
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingHorizontalLevel, "placing h-level");

    auto id = interaction.onClick(15.0, 100.0, store);
    requireTrue(id != 0, "click creates h-level");
    requireTrue(!interaction.isActive(), "idle after");

    const auto* d = store.get(id);
    requireTrue(d->type == dc::DrawingType::HorizontalLevel, "is h-level");
    requireTrue(d->y0 == 100.0, "price");

    std::printf("  Test 2 (h-level flow): PASS\n");
  }

  // ---- Test 3: Cancel ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    interaction.beginTrendline();
    interaction.onClick(10.0, 50.0, store);
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingTrendlineSecond, "placing second");

    interaction.cancel();
    requireTrue(!interaction.isActive(), "cancelled");
    requireTrue(store.count() == 0, "no drawing created");

    std::printf("  Test 3 (cancel): PASS\n");
  }

  // ---- Test 4: Click while idle does nothing ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    auto id = interaction.onClick(10.0, 50.0, store);
    requireTrue(id == 0, "idle click returns 0");
    requireTrue(store.count() == 0, "no drawing");

    std::printf("  Test 4 (idle click): PASS\n");
  }

  std::printf("D16.5 drawing_interaction: ALL PASS\n");
  return 0;
}
