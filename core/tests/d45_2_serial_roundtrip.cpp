// D45.2 — SceneSerializer: complex scene with ALL field types
// Blend modes, clipping, anchor, texture, visibility, multiple panes/layers/drawItems.
// Also verifies JSON determinism (serialize twice -> same string).
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
  std::printf("=== D45.2 SceneSerializer Complex Round-Trip ===\n");

  // ---- Build complex source scene ----
  dc::Scene srcScene;
  dc::ResourceRegistry srcReg;
  dc::CommandProcessor srcCp(srcScene, srcReg);

  // Buffers
  srcCp.applyJsonText(R"({"cmd":"createBuffer","id":1,"byteLength":0})");
  srcCp.applyJsonText(R"({"cmd":"createBuffer","id":2,"byteLength":0})");
  srcCp.applyJsonText(R"({"cmd":"createBuffer","id":3,"byteLength":0})"); // index buffer

  // Transforms
  srcCp.applyJsonText(R"({"cmd":"createTransform","id":100})");
  srcCp.applyJsonText(R"({"cmd":"setTransform","id":100,"tx":1.0,"ty":2.0,"sx":3.0,"sy":4.0})");
  srcCp.applyJsonText(R"({"cmd":"createTransform","id":101})");
  // leave 101 at identity

  // Panes
  srcCp.applyJsonText(R"({"cmd":"createPane","id":10,"name":"TopPane"})");
  srcCp.applyJsonText(R"({"cmd":"setPaneRegion","id":10,"clipYMin":0.0,"clipYMax":1.0,"clipXMin":-1.0,"clipXMax":1.0})");
  srcCp.applyJsonText(R"({"cmd":"setPaneClearColor","id":10,"r":0.1,"g":0.2,"b":0.3,"a":1.0})");

  srcCp.applyJsonText(R"({"cmd":"createPane","id":11,"name":"BottomPane"})");
  srcCp.applyJsonText(R"({"cmd":"setPaneRegion","id":11,"clipYMin":-1.0,"clipYMax":0.0,"clipXMin":-1.0,"clipXMax":1.0})");

  // Layers
  srcCp.applyJsonText(R"({"cmd":"createLayer","id":20,"paneId":10,"name":"Layer0"})");
  srcCp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":10,"name":"Layer1"})");
  srcCp.applyJsonText(R"({"cmd":"createLayer","id":22,"paneId":11,"name":"BottomLayer"})");

  // Geometries
  srcCp.applyJsonText(R"({"cmd":"createGeometry","id":50,"vertexBufferId":1,"format":"pos2_clip","vertexCount":6})");
  srcCp.applyJsonText(R"({"cmd":"setGeometryBounds","geometryId":50,"minX":-1.0,"minY":-2.0,"maxX":3.0,"maxY":4.0})");

  srcCp.applyJsonText(R"({"cmd":"createGeometry","id":51,"vertexBufferId":2,"format":"rect4","vertexCount":4})");

  // Geometry with index buffer (D26)
  srcCp.applyJsonText(R"({"cmd":"createGeometry","id":52,"vertexBufferId":1,"format":"pos2_clip","vertexCount":3,"indexBufferId":3,"indexCount":6})");

  // DrawItem 1: normal blend, with transform, custom colors
  srcCp.applyJsonText(R"({"cmd":"createDrawItem","id":200,"layerId":20,"name":"FilledTriangles"})");
  srcCp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":200,"pipeline":"triSolid@1","geometryId":50})");
  srcCp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":200,"transformId":100})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":200,"r":0.9,"g":0.1,"b":0.2,"a":1.0,"pointSize":8.0,"lineWidth":2.5,"dashLength":5.0,"gapLength":3.0,"cornerRadius":2.0,"blendMode":"normal","isClipSource":false,"useClipMask":false})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":200,"colorUpR":0.0,"colorUpG":1.0,"colorUpB":0.0,"colorUpA":1.0,"colorDownR":1.0,"colorDownG":0.0,"colorDownB":0.0,"colorDownA":1.0})");

  // DrawItem 2: additive blend, clip source, hidden
  srcCp.applyJsonText(R"({"cmd":"createDrawItem","id":201,"layerId":20,"name":"ClipMask"})");
  srcCp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":201,"pipeline":"triSolid@1","geometryId":50})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":201,"blendMode":"additive","isClipSource":true})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":201,"visible":false})");

  // DrawItem 3: multiply blend, uses clip mask, with texture
  srcCp.applyJsonText(R"({"cmd":"createDrawItem","id":202,"layerId":21,"name":"TexturedRect"})");
  srcCp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":202,"pipeline":"instancedRect@1","geometryId":51})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":202,"blendMode":"multiply","useClipMask":true})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemTexture","drawItemId":202,"textureId":42})");

  // DrawItem 4: screen blend, with anchor
  srcCp.applyJsonText(R"({"cmd":"createDrawItem","id":203,"layerId":22,"name":"AnchoredItem"})");
  srcCp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":203,"pipeline":"instancedRect@1","geometryId":51})");
  srcCp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":203,"transformId":101})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemStyle","drawItemId":203,"blendMode":"screen"})");
  srcCp.applyJsonText(R"({"cmd":"setDrawItemAnchor","drawItemId":203,"anchor":"bottomRight","offsetX":10.5,"offsetY":-5.25})");

  // ---- Verify JSON determinism: serialize twice -> same string ----
  std::string json1 = dc::serializeScene(srcScene);
  std::string json2 = dc::serializeScene(srcScene);
  check(json1 == json2, "JSON determinism: serialize twice produces same string");
  check(!json1.empty(), "serialized JSON is non-empty");

  // ---- Deserialize into new scene ----
  dc::Scene dstScene;
  dc::ResourceRegistry dstReg;
  dc::CommandProcessor dstCp(dstScene, dstReg);

  bool ok = dc::deserializeScene(json1, dstScene, dstCp);
  check(ok, "deserializeScene returns true");

  // ---- Verify buffers ----
  check(dstScene.hasBuffer(1), "buffer 1 exists");
  check(dstScene.hasBuffer(2), "buffer 2 exists");
  check(dstScene.hasBuffer(3), "buffer 3 (index) exists");

  // ---- Verify transforms ----
  check(dstScene.hasTransform(100), "transform 100 exists");
  {
    const dc::Transform* t = dstScene.getTransform(100);
    check(t != nullptr, "transform 100 not null");
    check(feq(t->params.tx, 1.0f), "tx=1");
    check(feq(t->params.ty, 2.0f), "ty=2");
    check(feq(t->params.sx, 3.0f), "sx=3");
    check(feq(t->params.sy, 4.0f), "sy=4");
  }
  check(dstScene.hasTransform(101), "transform 101 (identity) exists");
  {
    const dc::Transform* t = dstScene.getTransform(101);
    check(feq(t->params.tx, 0.0f), "identity tx=0");
    check(feq(t->params.sy, 1.0f), "identity sy=1");
  }

  // ---- Verify panes ----
  check(dstScene.hasPane(10), "pane 10 exists");
  {
    const dc::Pane* p = dstScene.getPane(10);
    check(p->name == "TopPane", "pane 10 name");
    check(feq(p->region.clipYMin, 0.0f), "pane 10 clipYMin");
    check(feq(p->region.clipYMax, 1.0f), "pane 10 clipYMax");
    check(p->hasClearColor, "pane 10 hasClearColor");
    check(feq(p->clearColor[0], 0.1f), "pane 10 clearColor[0]");
    check(feq(p->clearColor[1], 0.2f), "pane 10 clearColor[1]");
    check(feq(p->clearColor[2], 0.3f), "pane 10 clearColor[2]");
  }
  check(dstScene.hasPane(11), "pane 11 exists");
  {
    const dc::Pane* p = dstScene.getPane(11);
    check(p->name == "BottomPane", "pane 11 name");
    check(!p->hasClearColor, "pane 11 no clear color");
  }

  // ---- Verify layers ----
  check(dstScene.hasLayer(20), "layer 20 exists");
  check(dstScene.hasLayer(21), "layer 21 exists");
  check(dstScene.hasLayer(22), "layer 22 exists");
  {
    const dc::Layer* l = dstScene.getLayer(22);
    check(l->paneId == 11, "layer 22 paneId=11");
    check(l->name == "BottomLayer", "layer 22 name");
  }

  // ---- Verify geometries ----
  check(dstScene.hasGeometry(50), "geometry 50 exists");
  {
    const dc::Geometry* g = dstScene.getGeometry(50);
    check(g->vertexBufferId == 1, "geom 50 vertexBufferId=1");
    check(g->format == dc::VertexFormat::Pos2_Clip, "geom 50 format=pos2_clip");
    check(g->vertexCount == 6, "geom 50 vertexCount=6");
    check(g->boundsValid, "geom 50 boundsValid");
    check(feq(g->boundsMin[0], -1.0f), "geom 50 boundsMin[0]");
    check(feq(g->boundsMin[1], -2.0f), "geom 50 boundsMin[1]");
    check(feq(g->boundsMax[0], 3.0f), "geom 50 boundsMax[0]");
    check(feq(g->boundsMax[1], 4.0f), "geom 50 boundsMax[1]");
  }
  check(dstScene.hasGeometry(51), "geometry 51 exists");
  {
    const dc::Geometry* g = dstScene.getGeometry(51);
    check(g->format == dc::VertexFormat::Rect4, "geom 51 format=rect4");
  }
  check(dstScene.hasGeometry(52), "geometry 52 exists");
  {
    const dc::Geometry* g = dstScene.getGeometry(52);
    check(g->indexBufferId == 3, "geom 52 indexBufferId=3");
    check(g->indexCount == 6, "geom 52 indexCount=6");
  }

  // ---- Verify drawItem 200: normal blend, colors, style ----
  check(dstScene.hasDrawItem(200), "drawItem 200 exists");
  {
    const dc::DrawItem* di = dstScene.getDrawItem(200);
    check(di->name == "FilledTriangles", "di 200 name");
    check(di->pipeline == "triSolid@1", "di 200 pipeline");
    check(di->geometryId == 50, "di 200 geometryId");
    check(di->transformId == 100, "di 200 transformId");
    check(feq(di->color[0], 0.9f), "di 200 color[0]");
    check(feq(di->color[1], 0.1f), "di 200 color[1]");
    check(feq(di->color[2], 0.2f), "di 200 color[2]");
    check(feq(di->colorUp[0], 0.0f), "di 200 colorUp[0]");
    check(feq(di->colorUp[1], 1.0f), "di 200 colorUp[1]");
    check(feq(di->colorDown[0], 1.0f), "di 200 colorDown[0]");
    check(feq(di->colorDown[1], 0.0f), "di 200 colorDown[1]");
    check(feq(di->pointSize, 8.0f), "di 200 pointSize=8");
    check(feq(di->lineWidth, 2.5f), "di 200 lineWidth=2.5");
    check(feq(di->dashLength, 5.0f), "di 200 dashLength=5");
    check(feq(di->gapLength, 3.0f), "di 200 gapLength=3");
    check(feq(di->cornerRadius, 2.0f), "di 200 cornerRadius=2");
    check(di->blendMode == dc::BlendMode::Normal, "di 200 blendMode=normal");
    check(!di->isClipSource, "di 200 not clip source");
    check(!di->useClipMask, "di 200 not clip mask");
    check(di->visible, "di 200 visible");
  }

  // ---- Verify drawItem 201: additive, clip source, hidden ----
  check(dstScene.hasDrawItem(201), "drawItem 201 exists");
  {
    const dc::DrawItem* di = dstScene.getDrawItem(201);
    check(di->name == "ClipMask", "di 201 name");
    check(di->blendMode == dc::BlendMode::Additive, "di 201 blendMode=additive");
    check(di->isClipSource, "di 201 isClipSource=true");
    check(!di->visible, "di 201 visible=false");
  }

  // ---- Verify drawItem 202: multiply, clip mask user, texture ----
  check(dstScene.hasDrawItem(202), "drawItem 202 exists");
  {
    const dc::DrawItem* di = dstScene.getDrawItem(202);
    check(di->name == "TexturedRect", "di 202 name");
    check(di->blendMode == dc::BlendMode::Multiply, "di 202 blendMode=multiply");
    check(di->useClipMask, "di 202 useClipMask=true");
    check(di->textureId == 42, "di 202 textureId=42");
  }

  // ---- Verify drawItem 203: screen blend, anchor ----
  check(dstScene.hasDrawItem(203), "drawItem 203 exists");
  {
    const dc::DrawItem* di = dstScene.getDrawItem(203);
    check(di->name == "AnchoredItem", "di 203 name");
    check(di->blendMode == dc::BlendMode::Screen, "di 203 blendMode=screen");
    check(di->transformId == 101, "di 203 transformId=101");
    check(di->hasAnchor, "di 203 hasAnchor=true");
    check(di->anchorPoint == 8, "di 203 anchorPoint=8 (bottomRight)");
    check(feq(di->anchorOffsetX, 10.5f), "di 203 anchorOffsetX=10.5");
    check(feq(di->anchorOffsetY, -5.25f), "di 203 anchorOffsetY=-5.25");
  }

  // ---- Verify full round-trip: serialize destination scene ----
  std::string json3 = dc::serializeScene(dstScene);
  check(json1 == json3, "full round-trip: src serialization == dst serialization");

  // ---- Verify parse error returns false ----
  dc::Scene errScene;
  dc::ResourceRegistry errReg;
  dc::CommandProcessor errCp(errScene, errReg);
  bool errResult = dc::deserializeScene("{{INVALID JSON", errScene, errCp);
  check(!errResult, "deserializeScene returns false on parse error");

  std::printf("=== D45.2 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
