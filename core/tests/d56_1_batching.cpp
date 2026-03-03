// D56.1 — BatchBuilder: verify grouping and ordering
#include "dc/gl/BatchBuilder.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

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

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
  }
}

int main() {
  std::printf("=== D56.1 BatchBuilder Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Create scene: 1 pane, 1 layer, 3 same-pipeline draw items
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P1"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // 3 buffers + geometries for 3 draw items
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf1");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "buf2");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":24})"), "buf3");

  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"), "geom2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":102,"vertexBufferId":12,"vertexCount":3,"format":"pos2_clip"})"), "geom3");

  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":2})"), "di1");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":100})"), "bind1");

  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":51,"layerId":2})"), "di2");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":51,"pipeline":"triSolid@1","geometryId":101})"), "bind2");

  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":52,"layerId":2})"), "di3");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":52,"pipeline":"triSolid@1","geometryId":102})"), "bind3");

  // Test 1: 3 same-pipeline items should produce 1 batch
  dc::BatchBuilder builder;
  dc::BatchedFrame frame = builder.build(scene);

  check(frame.panes.size() == 1, "1 pane in frame");
  check(frame.panes[0].paneId == 1, "paneId is 1");
  check(frame.panes[0].batches.size() == 1, "3 same-pipeline items -> 1 batch");
  check(frame.panes[0].batches[0].items.size() == 3, "batch has 3 items");
  check(frame.panes[0].batches[0].pipeline == "triSolid@1", "batch pipeline is triSolid@1");

  // Test 2: Different pipeline breaks batch
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":13,"byteLength":24})"), "buf4");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":103,"vertexBufferId":13,"vertexCount":2,"format":"pos2_clip"})"), "geom4");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":53,"layerId":2})"), "di4");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":53,"pipeline":"line2d@1","geometryId":103})"), "bind4");

  frame = builder.build(scene);
  check(frame.panes[0].batches.size() == 2, "different pipeline -> 2 batches");
  check(frame.panes[0].batches[0].pipeline == "triSolid@1", "first batch is triSolid@1");
  check(frame.panes[0].batches[1].pipeline == "line2d@1", "second batch is line2d@1");

  // Test 3: Different blend mode breaks batch
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":14,"byteLength":24})"), "buf5");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":104,"vertexBufferId":14,"vertexCount":2,"format":"pos2_clip"})"), "geom5");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":54,"layerId":2})"), "di5");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":54,"pipeline":"line2d@1","geometryId":104})"), "bind5");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemStyle","drawItemId":54,"blendMode":"additive"})"), "style5");

  frame = builder.build(scene);
  // triSolid@1 x3 | line2d@1 (normal) | line2d@1 (additive)
  check(frame.panes[0].batches.size() == 3, "different blend mode -> 3 batches");

  // Test 4: Invisible items are excluded
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemVisible","drawItemId":50,"visible":false})"), "hide1");

  frame = builder.build(scene);
  // Item 50 is hidden, so triSolid batch should have 2 items
  bool found = false;
  for (const auto& batch : frame.panes[0].batches) {
    if (batch.pipeline == "triSolid@1") {
      check(batch.items.size() == 2, "hidden item excluded from batch");
      found = true;
    }
  }
  check(found, "triSolid batch still exists");

  // Test 5: Multiple panes
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":5,"name":"P2"})"), "pane2");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":6,"paneId":5})"), "layer2");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":15,"byteLength":24})"), "buf6");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":105,"vertexBufferId":15,"vertexCount":3,"format":"pos2_clip"})"), "geom6");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":60,"layerId":6})"), "di6");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":60,"pipeline":"triSolid@1","geometryId":105})"), "bind6");

  frame = builder.build(scene);
  check(frame.panes.size() == 2, "2 panes in frame");

  // Test 6: Layer ordering preserved across panes
  bool pane1First = (frame.panes.size() >= 2 && frame.panes[0].paneId == 1);
  check(pane1First, "pane 1 comes before pane 5 (sorted)");

  // Test 7: Empty scene produces empty frame
  dc::Scene emptyScene;
  dc::BatchedFrame emptyFrame = builder.build(emptyScene);
  check(emptyFrame.panes.empty(), "empty scene -> empty frame");

  std::printf("=== D56.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
