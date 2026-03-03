// D62.2 — DChartFile: save + load + compare round-trip via filesystem
#include "dc/export/DChartFile.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

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
  std::printf("=== D62.2 DChartFile Round-Trip Tests ===\n");

  // Use a temp file path
  std::string tmpPath = "/tmp/dc_test_d62_roundtrip.dchart";

  // Test 1: Build a DChartFile
  dc::DChartFile original;
  original.formatVersion = 1;
  original.sceneJSON = R"({"buffers":[{"id":10,"byteLength":48}],"transforms":[],"panes":[{"id":1,"name":"Main"}],"layers":[{"id":2,"paneId":1}],"geometries":[],"drawItems":[]})";
  original.annotationsJSON = R"([{"drawItemId":100,"role":"series","label":"Open","value":"line"},{"drawItemId":200,"role":"indicator","label":"RSI","value":"70.5"}])";
  original.themeJSON = R"({"name":"dark","backgroundColor":[0.1,0.1,0.12,1.0]})";
  original.metadata = "symbol=AAPL;timeframe=1D;created=2026-02-16T12:00:00Z";

  // Test 2: Save to file
  bool saveOk = dc::DChartFileIO::save(tmpPath, original);
  check(saveOk, "save returns true");

  // Test 3: Load from file
  dc::DChartFile loaded;
  bool loadOk = dc::DChartFileIO::load(tmpPath, loaded);
  check(loadOk, "load returns true");

  // Test 4: Compare all fields
  check(loaded.formatVersion == original.formatVersion, "formatVersion matches");
  check(loaded.sceneJSON == original.sceneJSON, "sceneJSON matches");
  check(loaded.annotationsJSON == original.annotationsJSON, "annotationsJSON matches");
  check(loaded.themeJSON == original.themeJSON, "themeJSON matches");
  check(loaded.metadata == original.metadata, "metadata matches");

  // Test 5: Re-serialize should produce same JSON
  std::string json1 = dc::DChartFileIO::serialize(original);
  std::string json2 = dc::DChartFileIO::serialize(loaded);
  check(json1 == json2, "re-serialized JSON matches");

  // Test 6: Load from non-existent file fails
  dc::DChartFile missing;
  bool loadMissing = dc::DChartFileIO::load("/tmp/dc_test_DOES_NOT_EXIST.dchart", missing);
  check(!loadMissing, "load non-existent file returns false");

  // Test 7: Save to invalid path fails
  bool saveBad = dc::DChartFileIO::save("/nonexistent/directory/file.dchart", original);
  check(!saveBad, "save to invalid path returns false");

  // Test 8: Double round-trip
  std::string tmpPath2 = "/tmp/dc_test_d62_roundtrip2.dchart";
  dc::DChartFileIO::save(tmpPath2, loaded);
  dc::DChartFile loaded2;
  dc::DChartFileIO::load(tmpPath2, loaded2);
  check(loaded2.sceneJSON == original.sceneJSON, "double round-trip sceneJSON matches");
  check(loaded2.annotationsJSON == original.annotationsJSON, "double round-trip annotationsJSON matches");
  check(loaded2.themeJSON == original.themeJSON, "double round-trip themeJSON matches");
  check(loaded2.metadata == original.metadata, "double round-trip metadata matches");

  // Test 9: Large content round-trip
  dc::DChartFile large;
  large.sceneJSON = std::string(10000, 'x');
  large.metadata = std::string(5000, 'y');
  std::string tmpPath3 = "/tmp/dc_test_d62_roundtrip_large.dchart";
  dc::DChartFileIO::save(tmpPath3, large);
  dc::DChartFile largeLoaded;
  dc::DChartFileIO::load(tmpPath3, largeLoaded);
  check(largeLoaded.sceneJSON.size() == 10000, "large sceneJSON preserved");
  check(largeLoaded.metadata.size() == 5000, "large metadata preserved");

  // Cleanup temp files
  std::remove(tmpPath.c_str());
  std::remove(tmpPath2.c_str());
  std::remove(tmpPath3.c_str());

  std::printf("=== D62.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
