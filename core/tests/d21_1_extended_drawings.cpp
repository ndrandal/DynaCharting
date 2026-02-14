// D21.1-D21.4 — Extended Drawing Tools: VerticalLine, Rectangle, FibRetracement

#include "dc/drawing/DrawingStore.hpp"
#include "dc/drawing/DrawingInteraction.hpp"
#include "dc/recipe/DrawingRecipe.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

int main() {
  // ---- Test 1: addVerticalLine ----
  {
    dc::DrawingStore store;
    auto id = store.addVerticalLine(42.0);
    requireTrue(id == 1, "first ID is 1");
    requireTrue(store.count() == 1, "1 drawing");

    const auto* d = store.get(id);
    requireTrue(d != nullptr, "drawing exists");
    requireTrue(d->type == dc::DrawingType::VerticalLine, "is vertical line");
    requireTrue(d->x0 == 42.0, "x0 is 42");
    std::printf("  Test 1 (addVerticalLine): PASS\n");
  }

  // ---- Test 2: addRectangle ----
  {
    dc::DrawingStore store;
    auto id = store.addRectangle(10.0, 50.0, 30.0, 80.0);
    requireTrue(id == 1, "first ID is 1");
    requireTrue(store.count() == 1, "1 drawing");

    const auto* d = store.get(id);
    requireTrue(d != nullptr, "drawing exists");
    requireTrue(d->type == dc::DrawingType::Rectangle, "is rectangle");
    requireTrue(d->x0 == 10.0, "x0");
    requireTrue(d->y0 == 50.0, "y0");
    requireTrue(d->x1 == 30.0, "x1");
    requireTrue(d->y1 == 80.0, "y1");
    std::printf("  Test 2 (addRectangle): PASS\n");
  }

  // ---- Test 3: addFibRetracement ----
  {
    dc::DrawingStore store;
    auto id = store.addFibRetracement(5.0, 100.0, 25.0, 50.0);
    requireTrue(id == 1, "first ID is 1");
    requireTrue(store.count() == 1, "1 drawing");

    const auto* d = store.get(id);
    requireTrue(d != nullptr, "drawing exists");
    requireTrue(d->type == dc::DrawingType::FibRetracement, "is fib retracement");
    requireTrue(d->x0 == 5.0, "x0");
    requireTrue(d->y0 == 100.0, "y0 (high)");
    requireTrue(d->x1 == 25.0, "x1");
    requireTrue(d->y1 == 50.0, "y1 (low)");
    std::printf("  Test 3 (addFibRetracement): PASS\n");
  }

  // ---- Test 4: computeDrawings with VerticalLine — 1 segment ----
  {
    dc::DrawingRecipeConfig cfg;
    dc::DrawingRecipe recipe(900, cfg);

    dc::DrawingStore store;
    store.addVerticalLine(15.0);

    auto data = recipe.computeDrawings(store, 0.0, 100.0, 0.0, 200.0);
    requireTrue(data.segmentCount == 1, "1 segment");
    requireTrue(data.lineSegments.size() == 4, "4 floats");

    // Vertical line: (15, 0) -> (15, 200)
    requireTrue(data.lineSegments[0] == 15.0f, "vl x0");
    requireTrue(data.lineSegments[1] == 0.0f, "vl yMin");
    requireTrue(data.lineSegments[2] == 15.0f, "vl x1");
    requireTrue(data.lineSegments[3] == 200.0f, "vl yMax");
    std::printf("  Test 4 (computeDrawings VerticalLine): PASS\n");
  }

  // ---- Test 5: computeDrawings with Rectangle — 4 segments ----
  {
    dc::DrawingRecipeConfig cfg;
    dc::DrawingRecipe recipe(910, cfg);

    dc::DrawingStore store;
    store.addRectangle(10.0, 50.0, 30.0, 80.0);

    auto data = recipe.computeDrawings(store, 0.0, 100.0, 0.0, 200.0);
    requireTrue(data.segmentCount == 4, "4 segments (border)");
    requireTrue(data.lineSegments.size() == 16, "16 floats");

    // Top: (10,50) -> (30,50)
    requireTrue(data.lineSegments[0] == 10.0f, "top x0");
    requireTrue(data.lineSegments[1] == 50.0f, "top y0");
    requireTrue(data.lineSegments[2] == 30.0f, "top x1");
    requireTrue(data.lineSegments[3] == 50.0f, "top y1");

    // Bottom: (10,80) -> (30,80)
    requireTrue(data.lineSegments[4] == 10.0f, "bot x0");
    requireTrue(data.lineSegments[5] == 80.0f, "bot y0");
    requireTrue(data.lineSegments[6] == 30.0f, "bot x1");
    requireTrue(data.lineSegments[7] == 80.0f, "bot y1");

    // Left: (10,50) -> (10,80)
    requireTrue(data.lineSegments[8] == 10.0f, "left x0");
    requireTrue(data.lineSegments[9] == 50.0f, "left y0");
    requireTrue(data.lineSegments[10] == 10.0f, "left x1");
    requireTrue(data.lineSegments[11] == 80.0f, "left y1");

    // Right: (30,50) -> (30,80)
    requireTrue(data.lineSegments[12] == 30.0f, "right x0");
    requireTrue(data.lineSegments[13] == 50.0f, "right y0");
    requireTrue(data.lineSegments[14] == 30.0f, "right x1");
    requireTrue(data.lineSegments[15] == 80.0f, "right y1");

    std::printf("  Test 5 (computeDrawings Rectangle): PASS\n");
  }

  // ---- Test 6: computeDrawings with FibRetracement — 6 segments ----
  {
    dc::DrawingRecipeConfig cfg;
    dc::DrawingRecipe recipe(920, cfg);

    dc::DrawingStore store;
    // High at y=100, low at y=50; range = -50
    store.addFibRetracement(5.0, 100.0, 25.0, 50.0);

    auto data = recipe.computeDrawings(store, 0.0, 100.0, 0.0, 200.0);
    requireTrue(data.segmentCount == 6, "6 fib levels");
    requireTrue(data.lineSegments.size() == 24, "24 floats");

    // Fib levels: 0%, 23.6%, 38.2%, 50%, 61.8%, 100%
    // y = y0 + (y1 - y0) * level = 100 + (-50) * level
    double y0 = 100.0, y1 = 50.0;
    double yRange = y1 - y0; // -50
    double levels[] = {0.0, 0.236, 0.382, 0.5, 0.618, 1.0};

    for (int i = 0; i < 6; ++i) {
      float expectedY = static_cast<float>(y0 + yRange * levels[i]);
      requireTrue(data.lineSegments[i * 4 + 0] == 5.0f, "fib x0");
      requireTrue(std::fabs(data.lineSegments[i * 4 + 1] - expectedY) < 0.01f, "fib y");
      requireTrue(data.lineSegments[i * 4 + 2] == 25.0f, "fib x1");
      requireTrue(std::fabs(data.lineSegments[i * 4 + 3] - expectedY) < 0.01f, "fib y dup");
    }
    std::printf("  Test 6 (computeDrawings FibRetracement): PASS\n");
  }

  // ---- Test 7: DrawingInteraction — vertical line single-click ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    interaction.beginVerticalLine();
    requireTrue(interaction.isActive(), "active");
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingVerticalLine, "placing vline");

    auto id = interaction.onClick(42.0, 99.0, store);
    requireTrue(id != 0, "click creates vline");
    requireTrue(!interaction.isActive(), "idle after");

    const auto* d = store.get(id);
    requireTrue(d->type == dc::DrawingType::VerticalLine, "is vline");
    requireTrue(d->x0 == 42.0, "x0");
    std::printf("  Test 7 (interaction VerticalLine): PASS\n");
  }

  // ---- Test 8: DrawingInteraction — rectangle two-click ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    interaction.beginRectangle();
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingRectangleFirst, "placing rect first");

    auto id1 = interaction.onClick(10.0, 50.0, store);
    requireTrue(id1 == 0, "first click returns 0");
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingRectangleSecond, "placing rect second");

    auto id2 = interaction.onClick(30.0, 80.0, store);
    requireTrue(id2 != 0, "second click creates rectangle");
    requireTrue(!interaction.isActive(), "idle after");

    const auto* d = store.get(id2);
    requireTrue(d->type == dc::DrawingType::Rectangle, "is rectangle");
    requireTrue(d->x0 == 10.0, "x0");
    requireTrue(d->y0 == 50.0, "y0");
    requireTrue(d->x1 == 30.0, "x1");
    requireTrue(d->y1 == 80.0, "y1");
    std::printf("  Test 8 (interaction Rectangle): PASS\n");
  }

  // ---- Test 9: DrawingInteraction — fib retracement two-click ----
  {
    dc::DrawingInteraction interaction;
    dc::DrawingStore store;

    interaction.beginFibRetracement();
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingFibFirst, "placing fib first");

    auto id1 = interaction.onClick(5.0, 100.0, store);
    requireTrue(id1 == 0, "first click returns 0");
    requireTrue(interaction.mode() == dc::DrawingMode::PlacingFibSecond, "placing fib second");

    auto id2 = interaction.onClick(25.0, 50.0, store);
    requireTrue(id2 != 0, "second click creates fib");
    requireTrue(!interaction.isActive(), "idle after");

    const auto* d = store.get(id2);
    requireTrue(d->type == dc::DrawingType::FibRetracement, "is fib");
    requireTrue(d->x0 == 5.0, "x0");
    requireTrue(d->y0 == 100.0, "y0");
    requireTrue(d->x1 == 25.0, "x1");
    requireTrue(d->y1 == 50.0, "y1");
    std::printf("  Test 9 (interaction FibRetracement): PASS\n");
  }

  // ---- Test 10: Serialization round-trip with new types ----
  {
    dc::DrawingStore store;
    store.addVerticalLine(42.0);
    store.addRectangle(10.0, 50.0, 30.0, 80.0);
    store.addFibRetracement(5.0, 100.0, 25.0, 50.0);
    store.setColor(1, 0.0f, 1.0f, 0.0f, 1.0f);
    store.setLineWidth(2, 4.0f);

    std::string json = store.toJSON();

    dc::DrawingStore loaded;
    requireTrue(loaded.loadJSON(json), "loadJSON ok");
    requireTrue(loaded.count() == 3, "3 drawings loaded");

    // VerticalLine
    const auto* d1 = loaded.get(1);
    requireTrue(d1 != nullptr, "vline exists");
    requireTrue(d1->type == dc::DrawingType::VerticalLine, "vline type");
    requireTrue(d1->x0 == 42.0, "vline x0");
    requireTrue(d1->color[1] == 1.0f, "vline green");

    // Rectangle
    const auto* d2 = loaded.get(2);
    requireTrue(d2 != nullptr, "rect exists");
    requireTrue(d2->type == dc::DrawingType::Rectangle, "rect type");
    requireTrue(d2->x0 == 10.0, "rect x0");
    requireTrue(d2->y1 == 80.0, "rect y1");
    requireTrue(d2->lineWidth == 4.0f, "rect lineWidth");

    // FibRetracement
    const auto* d3 = loaded.get(3);
    requireTrue(d3 != nullptr, "fib exists");
    requireTrue(d3->type == dc::DrawingType::FibRetracement, "fib type");
    requireTrue(d3->x0 == 5.0, "fib x0");
    requireTrue(d3->y0 == 100.0, "fib y0");
    requireTrue(d3->y1 == 50.0, "fib y1");

    std::printf("  Test 10 (serialization round-trip): PASS\n");
  }

  std::printf("D21.1 extended_drawings: ALL PASS\n");
  return 0;
}
