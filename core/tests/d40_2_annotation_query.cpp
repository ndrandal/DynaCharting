// D40.2 — Annotation commands: command-driven set/remove, JSON round-trip
#include "dc/metadata/AnnotationStore.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"

#include <cstdio>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
  }
}

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
  std::printf("=== D40.2 Annotation Command Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  dc::AnnotationStore store;
  cp.setAnnotationStore(&store);

  // Setup: create a draw item
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");

  // Test 1: setAnnotation command
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setAnnotation","drawItemId":3,"role":"series","label":"Price","value":"line"})");
    requireOk(r, "setAnnotation");
    check(r.ok, "setAnnotation succeeds");

    const dc::Annotation* ann = store.get(3);
    check(ann != nullptr, "annotation stored");
    check(ann != nullptr && ann->role == "series", "role set via command");
    check(ann != nullptr && ann->label == "Price", "label set via command");
    check(ann != nullptr && ann->value == "line", "value set via command");
  }

  // Test 2: Overwrite via command
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setAnnotation","drawItemId":3,"role":"indicator","label":"RSI","value":"14"})");
    requireOk(r, "setAnnotation overwrite");
    const dc::Annotation* ann = store.get(3);
    check(ann != nullptr && ann->role == "indicator", "role overwritten");
    check(ann != nullptr && ann->label == "RSI", "label overwritten");
  }

  // Test 3: removeAnnotation command
  {
    auto r = cp.applyJsonText(R"({"cmd":"removeAnnotation","drawItemId":3})");
    requireOk(r, "removeAnnotation");
    check(store.get(3) == nullptr, "annotation removed");
  }

  // Test 4: removeAnnotation on non-existent is OK
  {
    auto r = cp.applyJsonText(R"({"cmd":"removeAnnotation","drawItemId":999})");
    check(r.ok, "removeAnnotation non-existent is OK");
  }

  // Test 5: setAnnotation without store attached fails gracefully
  {
    dc::Scene scene2;
    dc::ResourceRegistry reg2;
    dc::CommandProcessor cp2(scene2, reg2);
    // No annotation store set
    auto r = cp2.applyJsonText(
      R"({"cmd":"setAnnotation","drawItemId":1,"role":"test","label":"L","value":"V"})");
    check(!r.ok, "setAnnotation without store fails");
  }

  // Test 6: JSON round-trip with commands
  {
    cp.applyJsonText(R"({"cmd":"setAnnotation","drawItemId":3,"role":"axis","label":"X","value":"time"})");
    cp.applyJsonText(R"({"cmd":"setAnnotation","drawItemId":4,"role":"axis","label":"Y","value":"price"})");

    std::string json = store.toJSON();
    dc::AnnotationStore store2;
    store2.loadJSON(json);
    check(store2.count() == 2, "JSON round-trip preserves count");
    const dc::Annotation* a3 = store2.get(3);
    check(a3 != nullptr && a3->label == "X", "JSON round-trip preserves data");
  }

  std::printf("=== D40.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
