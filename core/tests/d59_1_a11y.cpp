// D59.1 — AccessibilityBridge: build accessibility tree from scene + annotations
#include "dc/metadata/AccessibilityBridge.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/metadata/AnnotationStore.hpp"

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
  std::printf("=== D59.1 AccessibilityBridge Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::AnnotationStore annotations;

  // Build scene: 1 pane with 2 annotated drawItems and 1 unannotated.
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di1");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "di2");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di3");

  // Add geometry with bounds to drawItems for bounding box computation.
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"), "bind1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryBounds","geometryId":100,"minX":-0.5,"minY":-0.5,"maxX":0.5,"maxY":0.5})"), "bounds1");

  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "buf2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"triSolid@1","geometryId":101})"), "bind2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryBounds","geometryId":101,"minX":0.0,"minY":0.0,"maxX":1.0,"maxY":1.0})"), "bounds2");

  // Annotate drawItems 3 and 4 (not 5).
  annotations.set(3, "series", "Price Line", "OHLC");
  annotations.set(4, "indicator", "RSI", "14-period");

  // Test 1: Build tree with default config (excludeUnannotated).
  dc::AccessibilityBridge bridge;
  dc::AccessibilityConfig config;
  config.viewW = 800;
  config.viewH = 600;
  config.includeUnannotated = false;

  auto tree = bridge.buildTree(scene, annotations, config);
  check(tree.size() == 1, "tree has 1 root (1 pane)");

  if (!tree.empty()) {
    check(tree[0].role == "group", "root role is group");
    check(tree[0].name == "Price", "root name is pane name");
    check(tree[0].id == 1, "root id is pane id");

    // Test 2: Children should only be annotated drawItems (3 and 4, not 5).
    check(tree[0].children.size() == 2, "pane has 2 annotated children");

    if (tree[0].children.size() >= 2) {
      // Children should be sorted by ID.
      check(tree[0].children[0].id == 3, "first child id is 3");
      check(tree[0].children[0].role == "series", "first child role is series");
      check(tree[0].children[0].name == "Price Line", "first child name is Price Line");
      check(tree[0].children[0].value == "OHLC", "first child value is OHLC");

      check(tree[0].children[1].id == 4, "second child id is 4");
      check(tree[0].children[1].role == "indicator", "second child role is indicator");
      check(tree[0].children[1].name == "RSI", "second child name is RSI");
      check(tree[0].children[1].value == "14-period", "second child value is 14-period");
    }
  }

  // Test 3: Bounding box is computed from geometry bounds.
  if (!tree.empty() && tree[0].children.size() >= 1) {
    // Geometry 100 bounds: clipX [-0.5, 0.5], clipY [-0.5, 0.5]
    // In pixel coords (800x600):
    //   x = (-0.5 + 1) * 0.5 * 800 = 200
    //   y = (1 - 0.5) * 0.5 * 600 = 150
    //   w = (0.5 - (-0.5)) * 0.5 * 800 = 400
    //   h = (0.5 - (-0.5)) * 0.5 * 600 = 300
    const auto& bb = tree[0].children[0].boundingBox;
    check(bb[0] > 195 && bb[0] < 205, "child 0 bb.x ~ 200");
    check(bb[1] > 145 && bb[1] < 155, "child 0 bb.y ~ 150");
    check(bb[2] > 395 && bb[2] < 405, "child 0 bb.w ~ 400");
    check(bb[3] > 295 && bb[3] < 305, "child 0 bb.h ~ 300");
  }

  // Test 4: Include unannotated drawItems.
  {
    dc::AccessibilityConfig cfg2;
    cfg2.viewW = 800;
    cfg2.viewH = 600;
    cfg2.includeUnannotated = true;
    cfg2.defaultRole = "presentation";

    auto tree2 = bridge.buildTree(scene, annotations, cfg2);
    check(tree2.size() == 1, "tree2 has 1 root");
    if (!tree2.empty()) {
      check(tree2[0].children.size() == 3, "include unannotated: 3 children");

      // Unannotated item should have default role.
      bool foundPresentation = false;
      for (const auto& child : tree2[0].children) {
        if (child.id == 5 && child.role == "presentation") foundPresentation = true;
      }
      check(foundPresentation, "unannotated child has defaultRole");
    }
  }

  // Test 5: Empty scene.
  {
    dc::Scene emptyScene;
    dc::AnnotationStore emptyAnns;
    auto tree3 = bridge.buildTree(emptyScene, emptyAnns);
    check(tree3.empty(), "empty scene produces empty tree");
  }

  // Test 6: Pane with no annotated items.
  {
    dc::AnnotationStore noAnns;
    auto tree4 = bridge.buildTree(scene, noAnns);
    check(tree4.size() == 1, "tree4 has root for pane");
    if (!tree4.empty()) {
      check(tree4[0].children.empty(), "no annotated children = empty children");
    }
  }

  // Test 7: Multiple panes.
  {
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":50,"name":"Volume"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":51,"paneId":50})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":52,"layerId":51})"), "di_vol");
    annotations.set(52, "series", "Volume Bars", "bar");

    auto tree5 = bridge.buildTree(scene, annotations);
    check(tree5.size() == 2, "two panes = two roots");

    // Find the Volume pane root.
    bool foundVolume = false;
    for (const auto& root : tree5) {
      if (root.name == "Volume") {
        foundVolume = true;
        check(root.children.size() == 1, "Volume pane has 1 child");
        if (!root.children.empty()) {
          check(root.children[0].name == "Volume Bars", "volume child name");
        }
      }
    }
    check(foundVolume, "Volume pane root found");
  }

  // Test 8: Pane bounding box is computed from region.
  if (!tree.empty()) {
    // Default pane region is clipX [-1,1], clipY [-1,1]
    // Pixel coords (800x600): x=0, y=0, w=800, h=600
    const auto& pbb = tree[0].boundingBox;
    check(pbb[0] < 1, "pane bb.x ~ 0");
    check(pbb[1] < 1, "pane bb.y ~ 0");
    check(pbb[2] > 795, "pane bb.w ~ 800");
    check(pbb[3] > 595, "pane bb.h ~ 600");
  }

  std::printf("=== D59.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
