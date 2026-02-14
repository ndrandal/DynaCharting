// D23.1 — MeasureState + MeasureRecipe tests
// Tests: state lifecycle, cancel, current(), percentChange, build(), computeMeasure, distance.

#include "dc/measure/MeasureState.hpp"
#include "dc/recipe/MeasureRecipe.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireClose(double a, double b, double tol, const char* msg) {
  if (std::fabs(a - b) > tol) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: MeasureState begin/update/finish flow ---
  {
    dc::MeasureState ms;
    requireTrue(!ms.isActive(), "initially inactive");

    ms.begin(10.0, 100.0);
    requireTrue(ms.isActive(), "active after begin");

    ms.update(20.0, 150.0);
    requireTrue(ms.isActive(), "still active after update");

    dc::MeasureResult r = ms.finish(20.0, 150.0);
    requireTrue(r.valid, "result valid after finish");
    requireClose(r.x0, 10.0, 1e-9, "x0");
    requireClose(r.y0, 100.0, 1e-9, "y0");
    requireClose(r.x1, 20.0, 1e-9, "x1");
    requireClose(r.y1, 150.0, 1e-9, "y1");
    requireClose(r.dx, 10.0, 1e-9, "dx");
    requireClose(r.dy, 50.0, 1e-9, "dy");
    requireTrue(!ms.isActive(), "inactive after finish");

    std::printf("  Test 1 (begin/update/finish) PASS\n");
  }

  // --- Test 2: MeasureState cancel ---
  {
    dc::MeasureState ms;
    ms.begin(5.0, 50.0);
    ms.update(15.0, 75.0);
    requireTrue(ms.isActive(), "active before cancel");

    ms.cancel();
    requireTrue(!ms.isActive(), "inactive after cancel");

    dc::MeasureResult r = ms.current();
    requireTrue(!r.valid, "current() invalid after cancel");

    std::printf("  Test 2 (cancel) PASS\n");
  }

  // --- Test 3: MeasureState current() during active measurement ---
  {
    dc::MeasureState ms;

    // current() before begin
    dc::MeasureResult r = ms.current();
    requireTrue(!r.valid, "current() invalid when not active");

    ms.begin(1.0, 2.0);
    r = ms.current();
    requireTrue(!r.valid, "current() invalid before first update (no second point)");

    ms.update(3.0, 4.0);
    r = ms.current();
    requireTrue(r.valid, "current() valid after update");
    requireClose(r.x0, 1.0, 1e-9, "current x0");
    requireClose(r.y0, 2.0, 1e-9, "current y0");
    requireClose(r.x1, 3.0, 1e-9, "current x1");
    requireClose(r.y1, 4.0, 1e-9, "current y1");
    requireClose(r.dx, 2.0, 1e-9, "current dx");
    requireClose(r.dy, 2.0, 1e-9, "current dy");

    std::printf("  Test 3 (current() active) PASS\n");
  }

  // --- Test 4: percentChange calculation ---
  {
    dc::MeasureState ms;

    // Positive change: 100 -> 150 = +50%
    ms.begin(0.0, 100.0);
    dc::MeasureResult r = ms.finish(10.0, 150.0);
    requireClose(r.percentChange, 50.0, 1e-9, "positive percent change");

    // Negative change: 200 -> 150 = -25%
    ms.begin(0.0, 200.0);
    r = ms.finish(10.0, 150.0);
    requireClose(r.percentChange, -25.0, 1e-9, "negative percent change");

    // From zero: should be 0 (no division by zero)
    ms.begin(0.0, 0.0);
    r = ms.finish(10.0, 50.0);
    requireClose(r.percentChange, 0.0, 1e-9, "percent change from zero");

    std::printf("  Test 4 (percentChange) PASS\n");
  }

  // --- Test 5: MeasureRecipe build() creates correct scene objects ---
  {
    dc::MeasureRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    cfg.name = "measure";
    cfg.lineColor[0] = 0.8f; cfg.lineColor[1] = 0.8f;
    cfg.lineColor[2] = 0.8f; cfg.lineColor[3] = 0.8f;
    cfg.lineWidth = 1.0f;

    dc::MeasureRecipe recipe(500, cfg);

    auto result = recipe.build();

    // 5 create commands: createBuffer, createGeometry, createDrawItem, bindDrawItem, setDrawItemStyle
    requireTrue(result.createCommands.size() == 5, "5 create commands");

    // Verify createBuffer
    requireTrue(result.createCommands[0].find("createBuffer") != std::string::npos,
                "cmd[0] is createBuffer");
    requireTrue(result.createCommands[0].find("\"byteLength\":0") != std::string::npos,
                "createBuffer has byteLength:0");

    // Verify createGeometry with rect4 format
    requireTrue(result.createCommands[1].find("createGeometry") != std::string::npos,
                "cmd[1] is createGeometry");
    requireTrue(result.createCommands[1].find("rect4") != std::string::npos,
                "geometry format is rect4");

    // Verify bindDrawItem with lineAA@1
    requireTrue(result.createCommands[3].find("bindDrawItem") != std::string::npos,
                "cmd[3] is bindDrawItem");
    requireTrue(result.createCommands[3].find("lineAA@1") != std::string::npos,
                "pipeline is lineAA@1");

    // Verify setDrawItemStyle
    requireTrue(result.createCommands[4].find("setDrawItemStyle") != std::string::npos,
                "cmd[4] is setDrawItemStyle");
    requireTrue(result.createCommands[4].find("lineWidth") != std::string::npos,
                "setDrawItemStyle has lineWidth");

    // 3 dispose commands
    requireTrue(result.disposeCommands.size() == 3, "3 dispose commands");

    // Verify deterministic IDs
    requireTrue(recipe.bufferId() == 500, "bufferId = idBase + 0");
    requireTrue(recipe.geometryId() == 501, "geometryId = idBase + 1");
    requireTrue(recipe.drawItemId() == 502, "drawItemId = idBase + 2");

    std::printf("  Test 5 (build) PASS\n");
  }

  // --- Test 6: computeMeasure with valid result produces 3 segments ---
  {
    dc::MeasureRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    dc::MeasureRecipe recipe(600, cfg);

    dc::MeasureResult mr;
    mr.x0 = 10.0; mr.y0 = 100.0;
    mr.x1 = 20.0; mr.y1 = 150.0;
    mr.dx = 10.0; mr.dy = 50.0;
    mr.distance = std::sqrt(10.0 * 10.0 + 50.0 * 50.0);
    mr.percentChange = 50.0;
    mr.valid = true;

    auto data = recipe.computeMeasure(mr);
    requireTrue(data.segmentCount == 3, "3 segments");
    requireTrue(data.lineSegments.size() == 12, "12 floats (3 segments * 4)");

    // Segment 1: diagonal (10,100) -> (20,150)
    requireClose(data.lineSegments[0], 10.0f, 1e-5, "seg1 x0");
    requireClose(data.lineSegments[1], 100.0f, 1e-5, "seg1 y0");
    requireClose(data.lineSegments[2], 20.0f, 1e-5, "seg1 x1");
    requireClose(data.lineSegments[3], 150.0f, 1e-5, "seg1 y1");

    // Segment 2: horizontal (10,100) -> (20,100)
    requireClose(data.lineSegments[4], 10.0f, 1e-5, "seg2 x0");
    requireClose(data.lineSegments[5], 100.0f, 1e-5, "seg2 y0");
    requireClose(data.lineSegments[6], 20.0f, 1e-5, "seg2 x1");
    requireClose(data.lineSegments[7], 100.0f, 1e-5, "seg2 y1");

    // Segment 3: vertical (20,100) -> (20,150)
    requireClose(data.lineSegments[8], 20.0f, 1e-5, "seg3 x0");
    requireClose(data.lineSegments[9], 100.0f, 1e-5, "seg3 y0");
    requireClose(data.lineSegments[10], 20.0f, 1e-5, "seg3 x1");
    requireClose(data.lineSegments[11], 150.0f, 1e-5, "seg3 y1");

    std::printf("  Test 6 (computeMeasure valid) PASS\n");
  }

  // --- Test 7: computeMeasure with invalid result produces 0 segments ---
  {
    dc::MeasureRecipeConfig cfg;
    cfg.paneId = 1;
    cfg.layerId = 10;
    dc::MeasureRecipe recipe(700, cfg);

    dc::MeasureResult mr;  // default: valid = false
    auto data = recipe.computeMeasure(mr);
    requireTrue(data.segmentCount == 0, "0 segments for invalid measure");
    requireTrue(data.lineSegments.empty(), "no data for invalid measure");

    std::printf("  Test 7 (computeMeasure invalid) PASS\n");
  }

  // --- Test 8: Distance calculation (3,4) triangle = 5.0 ---
  {
    dc::MeasureState ms;
    ms.begin(0.0, 0.0);
    dc::MeasureResult r = ms.finish(3.0, 4.0);
    requireTrue(r.valid, "result valid");
    requireClose(r.distance, 5.0, 1e-9, "distance (0,0)->(3,4) = 5.0");
    requireClose(r.dx, 3.0, 1e-9, "dx = 3");
    requireClose(r.dy, 4.0, 1e-9, "dy = 4");

    std::printf("  Test 8 (distance 3-4-5) PASS\n");
  }

  std::printf("D23.1 measure: ALL PASS\n");
  return 0;
}
