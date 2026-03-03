// D45.1 — SceneSerializer: simple scene round-trip
// Create a scene with 1 pane, 1 layer, 1 drawItem, 1 buffer, 1 geometry, 1 transform.
// Serialize, deserialize into a new scene, verify all fields match.
#include "dc/session/SceneSerializer.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <cmath>
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

static bool feq(float a, float b, float tol = 1e-6f) {
  return std::fabs(a - b) < tol;
}

int main() {
  std::printf("=== D45.1 SceneSerializer Simple Round-Trip ===\n");

  // ---- Build source scene ----
  dc::Scene srcScene;
  dc::ResourceRegistry srcReg;
  dc::CommandProcessor srcCp(srcScene, srcReg);

  // Buffer (id=10)
  srcCp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})");
  // Transform (id=20)
  srcCp.applyJsonText(R"({"cmd":"createTransform","id":20})");
  srcCp.applyJsonText(R"({"cmd":"setTransform","id":20,"tx":0.5,"ty":-0.3,"sx":2.0,"sy":1.5})");
  // Pane (id=30)
  srcCp.applyJsonText(R"({"cmd":"createPane","id":30,"name":"MainPane"})");
  srcCp.applyJsonText(R"({"cmd":"setPaneRegion","id":30,"clipYMin":-0.8,"clipYMax":0.9,"clipXMin":-0.7,"clipXMax":0.6})");
  // Layer (id=40)
  srcCp.applyJsonText(R"({"cmd":"createLayer","id":40,"paneId":30,"name":"DataLayer"})");
  // Geometry (id=50)
  srcCp.applyJsonText(R"({"cmd":"createGeometry","id":50,"vertexBufferId":10,"format":"pos2_clip","vertexCount":3})");
  // DrawItem (id=60)
  srcCp.applyJsonText(R"({"cmd":"createDrawItem","id":60,"layerId":40,"name":"Triangle"})");
  srcCp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":60,"pipeline":"triSolid@1","geometryId":50})");
  srcCp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":60,"transformId":20})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":60,"r":0.2,"g":0.4,"b":0.6,"a":0.8})");

  // ---- Serialize ----
  std::string json = dc::serializeScene(srcScene);
  check(!json.empty(), "serialized JSON is non-empty");

  // ---- Deserialize into new scene ----
  dc::Scene dstScene;
  dc::ResourceRegistry dstReg;
  dc::CommandProcessor dstCp(dstScene, dstReg);

  bool ok = dc::deserializeScene(json, dstScene, dstCp);
  check(ok, "deserializeScene returns true");

  // ---- Verify buffer ----
  check(dstScene.hasBuffer(10), "buffer 10 exists");
  {
    const dc::Buffer* b = dstScene.getBuffer(10);
    check(b != nullptr, "buffer 10 is not null");
    check(b->id == 10, "buffer id matches");
  }

  // ---- Verify transform ----
  check(dstScene.hasTransform(20), "transform 20 exists");
  {
    const dc::Transform* t = dstScene.getTransform(20);
    check(t != nullptr, "transform 20 is not null");
    check(feq(t->params.tx, 0.5f), "transform tx matches");
    check(feq(t->params.ty, -0.3f), "transform ty matches");
    check(feq(t->params.sx, 2.0f), "transform sx matches");
    check(feq(t->params.sy, 1.5f), "transform sy matches");
  }

  // ---- Verify pane ----
  check(dstScene.hasPane(30), "pane 30 exists");
  {
    const dc::Pane* p = dstScene.getPane(30);
    check(p != nullptr, "pane 30 is not null");
    check(p->name == "MainPane", "pane name matches");
    check(feq(p->region.clipYMin, -0.8f), "pane clipYMin matches");
    check(feq(p->region.clipYMax, 0.9f), "pane clipYMax matches");
    check(feq(p->region.clipXMin, -0.7f), "pane clipXMin matches");
    check(feq(p->region.clipXMax, 0.6f), "pane clipXMax matches");
  }

  // ---- Verify layer ----
  check(dstScene.hasLayer(40), "layer 40 exists");
  {
    const dc::Layer* l = dstScene.getLayer(40);
    check(l != nullptr, "layer 40 is not null");
    check(l->paneId == 30, "layer paneId matches");
    check(l->name == "DataLayer", "layer name matches");
  }

  // ---- Verify geometry ----
  check(dstScene.hasGeometry(50), "geometry 50 exists");
  {
    const dc::Geometry* g = dstScene.getGeometry(50);
    check(g != nullptr, "geometry 50 is not null");
    check(g->vertexBufferId == 10, "geometry vertexBufferId matches");
    check(g->format == dc::VertexFormat::Pos2_Clip, "geometry format matches");
    check(g->vertexCount == 3, "geometry vertexCount matches");
  }

  // ---- Verify drawItem ----
  check(dstScene.hasDrawItem(60), "drawItem 60 exists");
  {
    const dc::DrawItem* di = dstScene.getDrawItem(60);
    check(di != nullptr, "drawItem 60 is not null");
    check(di->layerId == 40, "drawItem layerId matches");
    check(di->name == "Triangle", "drawItem name matches");
    check(di->pipeline == "triSolid@1", "drawItem pipeline matches");
    check(di->geometryId == 50, "drawItem geometryId matches");
    check(di->transformId == 20, "drawItem transformId matches");
    check(feq(di->color[0], 0.2f), "drawItem color[0] matches");
    check(feq(di->color[1], 0.4f), "drawItem color[1] matches");
    check(feq(di->color[2], 0.6f), "drawItem color[2] matches");
    check(feq(di->color[3], 0.8f), "drawItem color[3] matches");
    check(di->visible, "drawItem is visible");
  }

  // ---- Verify re-serialization produces same string ----
  std::string json2 = dc::serializeScene(dstScene);
  check(json == json2, "re-serialized JSON matches original");

  std::printf("=== D45.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
