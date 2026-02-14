// D22.1 — ChartState serialization: serialize / deserialize round-trip

#include "dc/session/ChartState.hpp"
#include "dc/drawing/DrawingStore.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) { std::fprintf(stderr, "ASSERT FAIL: %s\n", msg); std::exit(1); }
}

static bool approx(double a, double b, double eps = 1e-9) {
  return std::fabs(a - b) < eps;
}

int main() {
  // ---- Test 1: Serialize default ChartState produces valid JSON ----
  {
    dc::ChartState state;
    std::string json = dc::serializeChartState(state);

    requireTrue(!json.empty(), "serialized JSON not empty");
    requireTrue(json.find("\"version\"") != std::string::npos, "has version field");
    requireTrue(json.find("\"viewport\"") != std::string::npos, "has viewport field");
    requireTrue(json.find("\"theme\"") != std::string::npos, "has theme field");
    requireTrue(json.find("\"symbol\"") != std::string::npos, "has symbol field");
    requireTrue(json.find("\"timeframe\"") != std::string::npos, "has timeframe field");
    requireTrue(json.find("\"1.0\"") != std::string::npos, "version is 1.0");
    std::printf("  Test 1 (serialize default): PASS\n");
  }

  // ---- Test 2: Round-trip all fields ----
  {
    dc::ChartState original;
    original.version = "1.0";
    original.viewport.xMin = -50.5;
    original.viewport.xMax = 150.3;
    original.viewport.yMin = 10.0;
    original.viewport.yMax = 200.0;
    original.themeName = "Dark";
    original.symbol = "BTCUSD";
    original.timeframe = "1H";

    std::string json = dc::serializeChartState(original);

    dc::ChartState restored;
    bool ok = dc::deserializeChartState(json, restored);
    requireTrue(ok, "deserialize succeeds");
    requireTrue(restored.version == "1.0", "version matches");
    requireTrue(approx(restored.viewport.xMin, -50.5), "xMin matches");
    requireTrue(approx(restored.viewport.xMax, 150.3), "xMax matches");
    requireTrue(approx(restored.viewport.yMin, 10.0), "yMin matches");
    requireTrue(approx(restored.viewport.yMax, 200.0), "yMax matches");
    requireTrue(restored.themeName == "Dark", "theme matches");
    requireTrue(restored.symbol == "BTCUSD", "symbol matches");
    requireTrue(restored.timeframe == "1H", "timeframe matches");
    std::printf("  Test 2 (round-trip all fields): PASS\n");
  }

  // ---- Test 3: Viewport state preserved with precision ----
  {
    dc::ChartState state;
    state.viewport.xMin = 1234567.890123;
    state.viewport.xMax = 9876543.210987;
    state.viewport.yMin = -0.000001;
    state.viewport.yMax = 0.000001;

    std::string json = dc::serializeChartState(state);

    dc::ChartState restored;
    bool ok = dc::deserializeChartState(json, restored);
    requireTrue(ok, "deserialize succeeds");
    requireTrue(approx(restored.viewport.xMin, 1234567.890123, 1e-4),
                "xMin precision");
    requireTrue(approx(restored.viewport.xMax, 9876543.210987, 1e-4),
                "xMax precision");
    requireTrue(approx(restored.viewport.yMin, -0.000001, 1e-10),
                "yMin precision");
    requireTrue(approx(restored.viewport.yMax, 0.000001, 1e-10),
                "yMax precision");
    std::printf("  Test 3 (viewport precision): PASS\n");
  }

  // ---- Test 4: Drawings embedded and restored via DrawingStore ----
  {
    // Create drawings
    dc::DrawingStore store;
    store.addTrendline(10.0, 50.0, 20.0, 60.0);
    store.setColor(1, 1.0f, 0.0f, 0.0f, 1.0f);
    store.addHorizontalLevel(100.0);

    // Put into ChartState
    dc::ChartState state;
    state.drawingsJSON = store.toJSON();
    state.themeName = "Light";

    // Serialize entire chart state
    std::string json = dc::serializeChartState(state);

    // Verify drawings is embedded as an object (not an escaped string)
    requireTrue(json.find("\"drawings\":{") != std::string::npos ||
                json.find("\"drawings\": {") != std::string::npos,
                "drawings is embedded object");

    // Deserialize
    dc::ChartState restored;
    bool ok = dc::deserializeChartState(json, restored);
    requireTrue(ok, "deserialize succeeds");
    requireTrue(!restored.drawingsJSON.empty(), "drawingsJSON not empty");

    // Load into a new DrawingStore
    dc::DrawingStore restored_store;
    bool loadOk = restored_store.loadJSON(restored.drawingsJSON);
    requireTrue(loadOk, "loadJSON succeeds on restored drawings");
    requireTrue(restored_store.count() == 2, "2 drawings restored");

    const auto* d1 = restored_store.get(1);
    requireTrue(d1 != nullptr, "drawing id=1 exists");
    requireTrue(d1->type == dc::DrawingType::Trendline, "type is trendline");
    requireTrue(approx(d1->x0, 10.0), "x0 matches");
    requireTrue(approx(d1->y0, 50.0), "y0 matches");
    requireTrue(approx(d1->x1, 20.0), "x1 matches");
    requireTrue(approx(d1->y1, 60.0), "y1 matches");
    requireTrue(approx(d1->color[0], 1.0, 1e-5), "color[0] red");
    requireTrue(approx(d1->color[1], 0.0, 1e-5), "color[1] green");

    const auto* d2 = restored_store.get(2);
    requireTrue(d2 != nullptr, "drawing id=2 exists");
    requireTrue(d2->type == dc::DrawingType::HorizontalLevel, "type is h-level");
    requireTrue(approx(d2->y0, 100.0), "price matches");

    std::printf("  Test 4 (drawings embedded): PASS\n");
  }

  // ---- Test 5: Theme name preserved ----
  {
    dc::ChartState state;
    state.themeName = "CustomNeonTheme";
    std::string json = dc::serializeChartState(state);

    dc::ChartState restored;
    bool ok = dc::deserializeChartState(json, restored);
    requireTrue(ok, "deserialize succeeds");
    requireTrue(restored.themeName == "CustomNeonTheme", "theme name preserved");
    std::printf("  Test 5 (theme name): PASS\n");
  }

  // ---- Test 6: Deserialize returns false on invalid JSON ----
  {
    dc::ChartState out;
    requireTrue(!dc::deserializeChartState("", out), "empty string fails");
    requireTrue(!dc::deserializeChartState("{", out), "truncated JSON fails");
    requireTrue(!dc::deserializeChartState("null", out), "null fails");
    requireTrue(!dc::deserializeChartState("[]", out), "array at root fails");
    requireTrue(!dc::deserializeChartState("not json at all", out),
                "garbage fails");
    std::printf("  Test 6 (invalid JSON): PASS\n");
  }

  // ---- Test 7: Missing optional fields don't cause failure ----
  {
    // Minimal valid JSON — only version and viewport
    const char* minimal = R"({"version":"1.0","viewport":{"xMin":0,"xMax":1,"yMin":0,"yMax":1}})";

    dc::ChartState out;
    bool ok = dc::deserializeChartState(minimal, out);
    requireTrue(ok, "minimal JSON succeeds");
    requireTrue(out.version == "1.0", "version present");
    requireTrue(out.symbol.empty(), "symbol defaults to empty");
    requireTrue(out.timeframe.empty(), "timeframe defaults to empty");
    requireTrue(out.drawingsJSON.empty(), "drawingsJSON defaults to empty");
    requireTrue(out.themeName.empty(), "themeName defaults to empty");

    // Even an empty object should succeed
    dc::ChartState out2;
    bool ok2 = dc::deserializeChartState("{}", out2);
    requireTrue(ok2, "empty object succeeds");
    std::printf("  Test 7 (missing optional fields): PASS\n");
  }

  // ---- Test 8: Empty drawingsJSON produces no drawings field ----
  {
    dc::ChartState state;
    state.drawingsJSON = ""; // empty
    state.themeName = "Dark";
    std::string json = dc::serializeChartState(state);

    // Should not have a "drawings" key at all
    requireTrue(json.find("\"drawings\"") == std::string::npos,
                "no drawings key when drawingsJSON empty");

    // Round-trip still works
    dc::ChartState restored;
    bool ok = dc::deserializeChartState(json, restored);
    requireTrue(ok, "deserialize succeeds");
    requireTrue(restored.drawingsJSON.empty(), "drawingsJSON stays empty");
    std::printf("  Test 8 (empty drawingsJSON): PASS\n");
  }

  std::printf("D22.1 chart_state: ALL PASS\n");
  return 0;
}
