// D62.1 — DChartFile: serialize, verify JSON structure
#include "dc/export/DChartFile.hpp"

#include <cstdio>
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
  std::printf("=== D62.1 DChartFile Serialize Tests ===\n");

  // Test 1: Default DChartFile
  dc::DChartFile file;
  check(file.formatVersion == 1, "default formatVersion is 1");
  check(file.sceneJSON.empty(), "default sceneJSON is empty");
  check(file.annotationsJSON.empty(), "default annotationsJSON is empty");
  check(file.themeJSON.empty(), "default themeJSON is empty");
  check(file.metadata.empty(), "default metadata is empty");

  // Test 2: Populate and serialize
  file.sceneJSON = R"({"panes":[],"layers":[]})";
  file.annotationsJSON = R"([{"drawItemId":1,"role":"axis"}])";
  file.themeJSON = R"({"name":"dark"})";
  file.metadata = "created=2026-02-16";

  std::string json = dc::DChartFileIO::serialize(file);
  check(!json.empty(), "serialize produces non-empty string");

  // Test 3: JSON contains expected keys
  check(json.find("formatVersion") != std::string::npos, "JSON contains formatVersion");
  check(json.find("sceneJSON") != std::string::npos, "JSON contains sceneJSON");
  check(json.find("annotationsJSON") != std::string::npos, "JSON contains annotationsJSON");
  check(json.find("themeJSON") != std::string::npos, "JSON contains themeJSON");
  check(json.find("metadata") != std::string::npos, "JSON contains metadata");

  // Test 4: Deserialize
  dc::DChartFile loaded;
  bool ok = dc::DChartFileIO::deserialize(json, loaded);
  check(ok, "deserialize returns true");
  check(loaded.formatVersion == 1, "deserialized formatVersion is 1");
  check(loaded.sceneJSON == file.sceneJSON, "sceneJSON matches");
  check(loaded.annotationsJSON == file.annotationsJSON, "annotationsJSON matches");
  check(loaded.themeJSON == file.themeJSON, "themeJSON matches");
  check(loaded.metadata == file.metadata, "metadata matches");

  // Test 5: Invalid JSON returns false
  dc::DChartFile bad;
  ok = dc::DChartFileIO::deserialize("not json at all {{{", bad);
  check(!ok, "invalid JSON returns false");

  // Test 6: Empty JSON object is valid but has defaults
  dc::DChartFile empty;
  ok = dc::DChartFileIO::deserialize("{}", empty);
  check(ok, "empty object deserializes");
  check(empty.formatVersion == 1, "empty preserves default formatVersion");
  check(empty.sceneJSON.empty(), "empty preserves empty sceneJSON");

  // Test 7: Serialize with version 2
  dc::DChartFile v2;
  v2.formatVersion = 2;
  v2.sceneJSON = "{}";
  std::string v2json = dc::DChartFileIO::serialize(v2);
  dc::DChartFile v2loaded;
  dc::DChartFileIO::deserialize(v2json, v2loaded);
  check(v2loaded.formatVersion == 2, "formatVersion 2 round-trips");

  // Test 8: Embedded JSON strings are properly escaped
  dc::DChartFile withQuotes;
  withQuotes.sceneJSON = R"({"name":"test \"value\""})";
  std::string qjson = dc::DChartFileIO::serialize(withQuotes);
  dc::DChartFile qloaded;
  ok = dc::DChartFileIO::deserialize(qjson, qloaded);
  check(ok, "deserialize with escaped quotes succeeds");
  check(qloaded.sceneJSON == withQuotes.sceneJSON, "escaped quotes preserved");

  std::printf("=== D62.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
