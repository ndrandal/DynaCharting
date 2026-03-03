// D26.1 — Index buffer commands (no GL)
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdlib>
#include <cstdio>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
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
  std::printf("=== D26.1 Index Buffer Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Setup: pane + layer + draw item + 2 buffers + geometry
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "createDrawItem");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})"), "createBuffer vertex");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":0})"), "createBuffer index");

  // Test 1: Default Geometry has indexBufferId=0, indexCount=0
  {
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
      "createGeometry default");
    const dc::Geometry* g = scene.getGeometry(100);
    check(g != nullptr, "geometry exists");
    check(g->indexBufferId == 0, "default indexBufferId is 0");
    check(g->indexCount == 0, "default indexCount is 0");
  }

  // Test 2: createGeometry with optional indexBufferId and indexCount
  {
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":10,"vertexCount":6,"format":"pos2_clip","indexBufferId":11,"indexCount":3})"),
      "createGeometry with index");
    const dc::Geometry* g = scene.getGeometry(101);
    check(g != nullptr, "indexed geometry exists");
    check(g->indexBufferId == 11, "indexBufferId set via createGeometry");
    check(g->indexCount == 3, "indexCount set via createGeometry");
  }

  // Test 3: createGeometry with invalid indexBufferId fails
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"createGeometry","id":102,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip","indexBufferId":999})");
    check(!r.ok, "invalid indexBufferId rejected");
    check(r.err.code == "MISSING_BUFFER", "error code is MISSING_BUFFER");
  }

  // Test 4: setGeometryIndexBuffer command
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexBuffer","geometryId":100,"indexBufferId":11})");
    requireOk(r, "setGeometryIndexBuffer");
    const dc::Geometry* g = scene.getGeometry(100);
    check(g->indexBufferId == 11, "indexBufferId updated via setGeometryIndexBuffer");
  }

  // Test 5: setGeometryIndexCount command
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexCount","geometryId":100,"indexCount":6})");
    requireOk(r, "setGeometryIndexCount");
    const dc::Geometry* g = scene.getGeometry(100);
    check(g->indexCount == 6, "indexCount updated via setGeometryIndexCount");
  }

  // Test 6: setGeometryIndexBuffer with 0 detaches
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexBuffer","geometryId":100,"indexBufferId":0})");
    requireOk(r, "setGeometryIndexBuffer detach");
    const dc::Geometry* g = scene.getGeometry(100);
    check(g->indexBufferId == 0, "indexBufferId=0 detaches index buffer");
  }

  // Test 7: setGeometryIndexBuffer with invalid buffer ID fails
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexBuffer","geometryId":100,"indexBufferId":999})");
    check(!r.ok, "invalid indexBufferId rejected on set");
    check(r.err.code == "MISSING_BUFFER", "error code is MISSING_BUFFER on set");
  }

  // Test 8: setGeometryIndexBuffer with invalid geometryId fails
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexBuffer","geometryId":999,"indexBufferId":11})");
    check(!r.ok, "invalid geometryId rejected");
    check(r.err.code == "NOT_FOUND", "error code is NOT_FOUND");
  }

  // Test 9: setGeometryIndexCount with invalid geometryId fails
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setGeometryIndexCount","geometryId":999,"indexCount":3})");
    check(!r.ok, "invalid geometryId rejected on indexCount");
    check(r.err.code == "NOT_FOUND", "error code is NOT_FOUND on indexCount");
  }

  std::printf("=== D26.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
