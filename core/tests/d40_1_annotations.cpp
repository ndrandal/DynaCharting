// D40.1 — AnnotationStore: set/get/remove, findByRole, overwrite, clear
#include "dc/metadata/AnnotationStore.hpp"

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
  std::printf("=== D40.1 AnnotationStore Tests ===\n");

  dc::AnnotationStore store;

  // Test 1: Initially empty
  check(store.count() == 0, "initially empty");
  check(store.get(1) == nullptr, "get non-existent returns nullptr");

  // Test 2: Set and get
  store.set(1, "axis", "Price Axis", "USD");
  check(store.count() == 1, "count is 1 after set");
  const dc::Annotation* ann = store.get(1);
  check(ann != nullptr, "get returns annotation");
  check(ann->drawItemId == 1, "drawItemId matches");
  check(ann->role == "axis", "role matches");
  check(ann->label == "Price Axis", "label matches");
  check(ann->value == "USD", "value matches");

  // Test 3: Set multiple
  store.set(2, "series", "Open", "line");
  store.set(3, "series", "Close", "line");
  store.set(4, "indicator", "RSI", "oscillator");
  check(store.count() == 4, "count is 4");

  // Test 4: findByRole
  auto series = store.findByRole("series");
  check(series.size() == 2, "findByRole('series') returns 2");

  auto indicators = store.findByRole("indicator");
  check(indicators.size() == 1, "findByRole('indicator') returns 1");

  auto missing = store.findByRole("nonexistent");
  check(missing.empty(), "findByRole('nonexistent') returns empty");

  // Test 5: Overwrite
  store.set(1, "axis", "Volume Axis", "units");
  check(store.count() == 4, "count unchanged after overwrite");
  ann = store.get(1);
  check(ann->label == "Volume Axis", "label overwritten");
  check(ann->value == "units", "value overwritten");

  // Test 6: Remove
  store.remove(2);
  check(store.count() == 3, "count is 3 after remove");
  check(store.get(2) == nullptr, "removed annotation is gone");

  // Test 7: Remove non-existent is safe
  store.remove(999);
  check(store.count() == 3, "remove non-existent doesn't crash");

  // Test 8: All
  auto allAnns = store.all();
  check(allAnns.size() == 3, "all() returns 3");

  // Test 9: Clear
  store.clear();
  check(store.count() == 0, "clear empties store");
  check(store.get(1) == nullptr, "get after clear returns nullptr");

  // Test 10: JSON round-trip
  store.set(10, "data", "Candle", "OHLC");
  store.set(20, "legend", "Legend", "interactive");
  std::string json = store.toJSON();
  check(!json.empty(), "toJSON produces non-empty string");

  dc::AnnotationStore store2;
  store2.loadJSON(json);
  check(store2.count() == 2, "loadJSON restores 2 annotations");
  const dc::Annotation* ann10 = store2.get(10);
  check(ann10 != nullptr && ann10->role == "data", "loadJSON restores role");
  check(ann10 != nullptr && ann10->label == "Candle", "loadJSON restores label");

  std::printf("=== D40.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
