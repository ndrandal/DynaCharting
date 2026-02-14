// D19.2 — DrawingStore serialization: toJSON / loadJSON

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
  // ---- Test 1: toJSON produces valid JSON with correct structure ----
  {
    dc::DrawingStore store;
    store.addTrendline(10.0, 50.0, 20.0, 60.0);
    store.setColor(1, 1.0f, 0.0f, 0.0f, 1.0f);
    store.addHorizontalLevel(100.0);

    std::string json = store.toJSON();

    // Basic structural checks
    requireTrue(json.find("\"drawings\"") != std::string::npos, "has drawings key");
    requireTrue(json.find("\"id\"") != std::string::npos, "has id field");
    requireTrue(json.find("\"type\"") != std::string::npos, "has type field");
    requireTrue(json.find("\"x0\"") != std::string::npos, "has x0 field");
    requireTrue(json.find("\"y0\"") != std::string::npos, "has y0 field");
    requireTrue(json.find("\"color\"") != std::string::npos, "has color field");
    requireTrue(json.find("\"lineWidth\"") != std::string::npos, "has lineWidth field");
    std::printf("  Test 1 (toJSON structure): PASS\n");
  }

  // ---- Test 2: loadJSON restores drawings correctly ----
  {
    const char* json = R"({"drawings":[
      {"id":5,"type":1,"x0":1.5,"y0":2.5,"x1":3.5,"y1":4.5,
       "color":[0.5,0.6,0.7,0.8],"lineWidth":3.0},
      {"id":10,"type":2,"x0":0,"y0":99.9,"x1":0,"y1":0,
       "color":[1,1,1,1],"lineWidth":1.5}
    ]})";

    dc::DrawingStore store;
    bool ok = store.loadJSON(json);
    requireTrue(ok, "loadJSON succeeds");
    requireTrue(store.count() == 2, "2 drawings loaded");

    const auto* d1 = store.get(5);
    requireTrue(d1 != nullptr, "drawing id=5 exists");
    requireTrue(d1->type == dc::DrawingType::Trendline, "type is trendline");
    requireTrue(approx(d1->x0, 1.5), "x0");
    requireTrue(approx(d1->y0, 2.5), "y0");
    requireTrue(approx(d1->x1, 3.5), "x1");
    requireTrue(approx(d1->y1, 4.5), "y1");
    requireTrue(approx(d1->color[0], 0.5, 1e-6), "color[0]");
    requireTrue(approx(d1->color[1], 0.6, 1e-6), "color[1]");
    requireTrue(approx(d1->color[2], 0.7, 1e-6), "color[2]");
    requireTrue(approx(d1->color[3], 0.8, 1e-6), "color[3]");
    requireTrue(approx(d1->lineWidth, 3.0, 1e-6), "lineWidth");

    const auto* d2 = store.get(10);
    requireTrue(d2 != nullptr, "drawing id=10 exists");
    requireTrue(d2->type == dc::DrawingType::HorizontalLevel, "type is h-level");
    requireTrue(approx(d2->y0, 99.9), "price");

    // nextId_ should be max(id)+1 = 11
    auto newId = store.addTrendline(0, 0, 0, 0);
    requireTrue(newId == 11, "nextId is max+1 (11)");
    std::printf("  Test 2 (loadJSON restores): PASS\n");
  }

  // ---- Test 3: round-trip (toJSON -> loadJSON -> verify equality) ----
  {
    dc::DrawingStore original;
    original.addTrendline(1.1, 2.2, 3.3, 4.4);
    original.setColor(1, 0.1f, 0.2f, 0.3f, 0.9f);
    original.setLineWidth(1, 5.0f);
    original.addHorizontalLevel(77.7);
    original.addTrendline(100.0, 200.0, 300.0, 400.0);

    std::string json = original.toJSON();

    dc::DrawingStore restored;
    bool ok = restored.loadJSON(json);
    requireTrue(ok, "round-trip loadJSON succeeds");
    requireTrue(restored.count() == original.count(), "same count");

    const auto& orig = original.drawings();
    const auto& rest = restored.drawings();
    for (std::size_t i = 0; i < orig.size(); ++i) {
      requireTrue(orig[i].id == rest[i].id, "id match");
      requireTrue(orig[i].type == rest[i].type, "type match");
      requireTrue(approx(orig[i].x0, rest[i].x0), "x0 match");
      requireTrue(approx(orig[i].y0, rest[i].y0), "y0 match");
      requireTrue(approx(orig[i].x1, rest[i].x1), "x1 match");
      requireTrue(approx(orig[i].y1, rest[i].y1), "y1 match");
      for (int c = 0; c < 4; ++c) {
        requireTrue(approx(orig[i].color[c], rest[i].color[c], 1e-5),
                    "color match");
      }
      requireTrue(approx(orig[i].lineWidth, rest[i].lineWidth, 1e-5),
                  "lineWidth match");
    }
    std::printf("  Test 3 (round-trip): PASS\n");
  }

  // ---- Test 4: loadJSON returns false on invalid JSON ----
  {
    dc::DrawingStore store;
    store.addTrendline(1, 2, 3, 4); // pre-existing data

    requireTrue(!store.loadJSON(""), "empty string fails");
    requireTrue(!store.loadJSON("{"), "truncated JSON fails");
    requireTrue(!store.loadJSON("null"), "null fails");
    requireTrue(!store.loadJSON("[]"), "array at root fails");
    requireTrue(!store.loadJSON("{\"drawings\":\"bad\"}"), "drawings not array fails");
    requireTrue(!store.loadJSON("{\"other\":[]}"), "missing drawings key fails");

    // On failure, existing data should remain unchanged
    requireTrue(store.count() == 1, "data unchanged on parse failure");
    std::printf("  Test 4 (invalid JSON): PASS\n");
  }

  // ---- Test 5: empty store serialization ----
  {
    dc::DrawingStore store;
    std::string json = store.toJSON();
    requireTrue(json.find("\"drawings\":[]") != std::string::npos ||
                json.find("\"drawings\": []") != std::string::npos,
                "empty array in JSON");

    dc::DrawingStore loaded;
    bool ok = loaded.loadJSON(json);
    requireTrue(ok, "loadJSON of empty succeeds");
    requireTrue(loaded.count() == 0, "0 drawings after load");

    // nextId should be 1 (max(0) + 1 = 1, but no drawings so stays at default)
    auto id = loaded.addTrendline(0, 0, 0, 0);
    requireTrue(id == 1, "nextId starts at 1 for empty load");
    std::printf("  Test 5 (empty serialization): PASS\n");
  }

  std::printf("D19.2 drawing_serial: ALL PASS\n");
  return 0;
}
