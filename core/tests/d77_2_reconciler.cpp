// D77.2: SceneReconciler diff/reconcile tests
#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Types.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace dc;

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-5f; }

// Helper: build a minimal but complete document for one triangle
static SceneDocument makeTriangleDoc() {
  SceneDocument doc;
  doc.version = 1;
  doc.viewportWidth = 400;
  doc.viewportHeight = 300;

  doc.buffers[100] = {0};
  doc.transforms[50] = {0, 0, 1, 1};

  DocPane p;
  p.name = "Main";
  doc.panes[1] = p;

  doc.layers[10] = {1, "Data"};

  DocGeometry g;
  g.vertexBufferId = 100;
  g.format = "pos2_clip";
  g.vertexCount = 3;
  doc.geometries[101] = g;

  DocDrawItem di;
  di.layerId = 10;
  di.pipeline = "triSolid@1";
  di.geometryId = 101;
  di.transformId = 50;
  di.color[0] = 1; di.color[1] = 0; di.color[2] = 0; di.color[3] = 1;
  doc.drawItems[200] = di;

  return doc;
}

// T1: Cold start — reconcile against empty Scene → all resources created
static void testColdStart() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto result = reconciler.reconcile(doc, scene);

  assert(result.ok);
  assert(result.created > 0);
  assert(result.deleted == 0);

  // Verify scene has all resources
  assert(scene.hasBuffer(100));
  assert(scene.hasTransform(50));
  assert(scene.hasPane(1));
  assert(scene.hasLayer(10));
  assert(scene.hasGeometry(101));
  assert(scene.hasDrawItem(200));

  // Verify draw item bindings
  const DrawItem* di = scene.getDrawItem(200);
  assert(di);
  assert(di->pipeline == "triSolid@1");
  assert(di->geometryId == 101);
  assert(di->transformId == 50);
  assert(feq(di->color[0], 1.0f));
  assert(feq(di->color[1], 0.0f));

  std::printf("T1 coldStart: PASS\n");
}

// T2: No-op — reconcile same document twice → 0 updates, 0 creates, 0 deletes
static void testNoop() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();

  // First reconcile
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Second reconcile — no changes
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.created == 0);
  assert(r2.deleted == 0);
  // Style is not re-sent because color matches
  assert(r2.updated == 0);

  std::printf("T2 noop: PASS\n");
}

// T3: Update — change a DrawItem color → only update emitted
static void testUpdateColor() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Modify color
  doc.drawItems[200].color[0] = 0.0f;
  doc.drawItems[200].color[1] = 1.0f;
  doc.drawItems[200].color[2] = 0.0f;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.created == 0);
  assert(r2.deleted == 0);
  assert(r2.updated >= 1);

  // Verify color changed in scene
  const DrawItem* di = scene.getDrawItem(200);
  assert(di);
  assert(feq(di->color[0], 0.0f));
  assert(feq(di->color[1], 1.0f));
  assert(feq(di->color[2], 0.0f));

  std::printf("T3 updateColor: PASS\n");
}

// T4: Delete — remove a DrawItem from document → deleted from Scene
static void testDelete() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.hasDrawItem(200));

  // Remove drawItem from document
  doc.drawItems.erase(200);

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.deleted >= 1);
  assert(!scene.hasDrawItem(200));

  std::printf("T4 delete: PASS\n");
}

// T5: Add — add a new DrawItem → created in Scene
static void testAdd() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Add a second draw item
  DocDrawItem di2;
  di2.layerId = 10;
  di2.pipeline = "triSolid@1";
  di2.geometryId = 101;
  di2.color[0] = 0; di2.color[1] = 0; di2.color[2] = 1; di2.color[3] = 1;
  doc.drawItems[201] = di2;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.created >= 1);
  assert(scene.hasDrawItem(201));

  const DrawItem* newDi = scene.getDrawItem(201);
  assert(newDi);
  assert(newDi->pipeline == "triSolid@1");
  assert(feq(newDi->color[2], 1.0f));

  std::printf("T5 add: PASS\n");
}

// T6: Re-parent layer — change paneId → old deleted, new created
static void testReparentLayer() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  // Document with 2 panes
  SceneDocument doc = makeTriangleDoc();
  DocPane p2;
  p2.name = "Secondary";
  doc.panes[2] = p2;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getLayer(10)->paneId == 1);

  // Move layer 10 to pane 2
  doc.layers[10].paneId = 2;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  // Layer should now belong to pane 2
  assert(scene.hasLayer(10));
  assert(scene.getLayer(10)->paneId == 2);

  std::printf("T6 reparentLayer: PASS\n");
}

// T7: Full cycle — create → update → delete → verify final Scene state
static void testFullCycle() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  // Step 1: Create initial scene
  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.drawItemIds().size() == 1);

  // Step 2: Add second draw item + change color of first
  DocDrawItem di2;
  di2.layerId = 10;
  di2.pipeline = "triSolid@1";
  di2.geometryId = 101;
  di2.color[0] = 0; di2.color[1] = 0; di2.color[2] = 1; di2.color[3] = 1;
  doc.drawItems[201] = di2;
  doc.drawItems[200].color[0] = 0.5f;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(scene.drawItemIds().size() == 2);
  assert(feq(scene.getDrawItem(200)->color[0], 0.5f));

  // Step 3: Delete first draw item
  doc.drawItems.erase(200);

  auto r3 = reconciler.reconcile(doc, scene);
  assert(r3.ok);
  assert(scene.drawItemIds().size() == 1);
  assert(!scene.hasDrawItem(200));
  assert(scene.hasDrawItem(201));

  // Step 4: Delete everything
  doc.drawItems.clear();
  doc.geometries.clear();
  doc.layers.clear();
  doc.panes.clear();
  doc.transforms.clear();
  doc.buffers.clear();

  auto r4 = reconciler.reconcile(doc, scene);
  assert(r4.ok);
  assert(scene.drawItemIds().empty());
  assert(scene.layerIds().empty());
  assert(scene.paneIds().empty());
  assert(scene.geometryIds().empty());
  assert(scene.transformIds().empty());
  assert(scene.bufferIds().empty());

  std::printf("T7 fullCycle: PASS\n");
}

// T8: Update transform params
static void testUpdateTransform() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  const Transform* t = scene.getTransform(50);
  assert(t);
  assert(feq(t->params.sx, 1.0f));

  // Change transform
  doc.transforms[50].sx = 2.0f;
  doc.transforms[50].tx = 0.5f;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.updated >= 1);

  t = scene.getTransform(50);
  assert(feq(t->params.sx, 2.0f));
  assert(feq(t->params.tx, 0.5f));

  std::printf("T8 updateTransform: PASS\n");
}

// T9: Update pane region
static void testUpdatePaneRegion() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Change pane region
  doc.panes[1].region.clipYMin = 0.1f;
  doc.panes[1].region.clipYMax = 0.9f;

  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);

  const Pane* p = scene.getPane(1);
  assert(feq(p->region.clipYMin, 0.1f));
  assert(feq(p->region.clipYMax, 0.9f));

  std::printf("T9 updatePaneRegion: PASS\n");
}

// T10: Visibility toggle
static void testVisibility() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getDrawItem(200)->visible);

  // Hide
  doc.drawItems[200].visible = false;
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(!scene.getDrawItem(200)->visible);

  // Show again
  doc.drawItems[200].visible = true;
  auto r3 = reconciler.reconcile(doc, scene);
  assert(r3.ok);
  assert(scene.getDrawItem(200)->visible);

  std::printf("T10 visibility: PASS\n");
}

// T11: Gradient property update — set then change gradient
static void testGradientUpdate() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.drawItems[200].gradientType = "linear";
  doc.drawItems[200].gradientAngle = 1.0f;
  doc.drawItems[200].gradientColor0[0] = 1; doc.drawItems[200].gradientColor0[1] = 0;
  doc.drawItems[200].gradientColor0[2] = 0; doc.drawItems[200].gradientColor0[3] = 1;
  doc.drawItems[200].gradientColor1[0] = 0; doc.drawItems[200].gradientColor1[1] = 0;
  doc.drawItems[200].gradientColor1[2] = 1; doc.drawItems[200].gradientColor1[3] = 1;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  const DrawItem* di = scene.getDrawItem(200);
  assert(di->gradientType == 1); // linear

  // Change gradient type to radial
  doc.drawItems[200].gradientType = "radial";
  doc.drawItems[200].gradientRadius = 0.75f;
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  di = scene.getDrawItem(200);
  assert(di->gradientType == 2); // radial
  assert(feq(di->gradientRadius, 0.75f));

  // Remove gradient
  doc.drawItems[200].gradientType = "none";
  auto r3 = reconciler.reconcile(doc, scene);
  assert(r3.ok);
  di = scene.getDrawItem(200);
  assert(di->gradientType == 0); // none

  std::printf("T11 gradientUpdate: PASS\n");
}

// T12: Anchor property update — set then change anchor
static void testAnchorUpdate() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.drawItems[200].anchorPoint = "center";
  doc.drawItems[200].anchorOffsetX = 5.0f;
  doc.drawItems[200].anchorOffsetY = 10.0f;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  const DrawItem* di = scene.getDrawItem(200);
  assert(di->hasAnchor);
  assert(di->anchorPoint == 4); // center
  assert(feq(di->anchorOffsetX, 5.0f));
  assert(feq(di->anchorOffsetY, 10.0f));

  // Change anchor
  doc.drawItems[200].anchorPoint = "topLeft";
  doc.drawItems[200].anchorOffsetX = 0.0f;
  doc.drawItems[200].anchorOffsetY = 0.0f;
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  di = scene.getDrawItem(200);
  assert(di->hasAnchor);
  assert(di->anchorPoint == 0); // topLeft

  std::printf("T12 anchorUpdate: PASS\n");
}

// T13: Texture ID update
static void testTextureUpdate() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getDrawItem(200)->textureId == 0);

  // Set texture
  doc.drawItems[200].textureId = 42;
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(scene.getDrawItem(200)->textureId == 42);

  // Change texture
  doc.drawItems[200].textureId = 99;
  auto r3 = reconciler.reconcile(doc, scene);
  assert(r3.ok);
  assert(scene.getDrawItem(200)->textureId == 99);

  // Clear texture
  doc.drawItems[200].textureId = 0;
  auto r4 = reconciler.reconcile(doc, scene);
  assert(r4.ok);
  assert(scene.getDrawItem(200)->textureId == 0);

  std::printf("T13 textureUpdate: PASS\n");
}

// T14: BlendMode update
static void testBlendModeUpdate() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getDrawItem(200)->blendMode == BlendMode::Normal);

  // Change blend mode to additive
  doc.drawItems[200].blendMode = "additive";
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.updated >= 1);
  assert(scene.getDrawItem(200)->blendMode == BlendMode::Additive);

  // Change to screen
  doc.drawItems[200].blendMode = "screen";
  auto r3 = reconciler.reconcile(doc, scene);
  assert(r3.ok);
  assert(scene.getDrawItem(200)->blendMode == BlendMode::Screen);

  // Back to normal
  doc.drawItems[200].blendMode = "normal";
  auto r4 = reconciler.reconcile(doc, scene);
  assert(r4.ok);
  assert(scene.getDrawItem(200)->blendMode == BlendMode::Normal);

  std::printf("T14 blendModeUpdate: PASS\n");
}

// T15: Pane name change — triggers delete + recreate
static void testPaneNameChange() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getPane(1)->name == "Main");

  // Rename pane
  doc.panes[1].name = "Renamed";
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.deleted >= 1);
  assert(r2.created >= 1);
  assert(scene.hasPane(1));
  assert(scene.getPane(1)->name == "Renamed");

  std::printf("T15 paneNameChange: PASS\n");
}

// T16: Layer name change — triggers delete + recreate
static void testLayerNameChange() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  assert(scene.getLayer(10)->name == "Data");

  // Rename layer
  doc.layers[10].name = "NewLayerName";
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.deleted >= 1);
  assert(r2.created >= 1);
  assert(scene.hasLayer(10));
  assert(scene.getLayer(10)->name == "NewLayerName");

  std::printf("T16 layerNameChange: PASS\n");
}

// T17: Geometry format change — triggers delete + recreate
static void testGeometryFormatChange() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  const Geometry* g = scene.getGeometry(101);
  assert(g);
  assert(g->format == VertexFormat::Pos2_Clip);

  // Change format
  doc.geometries[101].format = "rect4";
  doc.geometries[101].vertexCount = 1; // rect4 needs different count
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.deleted >= 1);
  assert(r2.created >= 1);
  g = scene.getGeometry(101);
  assert(g);
  assert(g->format == VertexFormat::Rect4);

  std::printf("T17 geometryFormatChange: PASS\n");
}

// T18: Clear color removal
static void testClearColorRemoval() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.panes[1].hasClearColor = true;
  doc.panes[1].clearColor[0] = 0.1f;
  doc.panes[1].clearColor[1] = 0.2f;
  doc.panes[1].clearColor[2] = 0.3f;
  doc.panes[1].clearColor[3] = 1.0f;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);
  const Pane* p = scene.getPane(1);
  assert(p->hasClearColor);
  assert(feq(p->clearColor[0], 0.1f));

  // Remove clear color
  doc.panes[1].hasClearColor = false;
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  p = scene.getPane(1);
  assert(!p->hasClearColor);

  std::printf("T18 clearColorRemoval: PASS\n");
}

// T19: Viewport dimensions surfaced in ReconcileResult
static void testViewportInResult() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.viewportWidth = 1920;
  doc.viewportHeight = 1080;

  auto r = reconciler.reconcile(doc, scene);
  assert(r.ok);
  assert(r.viewportWidth == 1920);
  assert(r.viewportHeight == 1080);

  std::printf("T19 viewportInResult: PASS\n");
}

// T20: Gradient no-op — same gradient twice → 0 changes on second reconcile
static void testGradientNoop() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.drawItems[200].gradientType = "linear";
  doc.drawItems[200].gradientAngle = 0.5f;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Second reconcile with identical gradient — should be a no-op
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.created == 0);
  assert(r2.deleted == 0);
  assert(r2.updated == 0);

  std::printf("T20 gradientNoop: PASS\n");
}

// T21: Anchor no-op — same anchor twice → 0 changes on second reconcile
static void testAnchorNoop() {
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  SceneReconciler reconciler(cp);

  SceneDocument doc = makeTriangleDoc();
  doc.drawItems[200].anchorPoint = "bottomRight";
  doc.drawItems[200].anchorOffsetX = 3.0f;
  doc.drawItems[200].anchorOffsetY = 7.0f;

  auto r1 = reconciler.reconcile(doc, scene);
  assert(r1.ok);

  // Second reconcile with identical anchor — should be a no-op
  auto r2 = reconciler.reconcile(doc, scene);
  assert(r2.ok);
  assert(r2.created == 0);
  assert(r2.deleted == 0);
  assert(r2.updated == 0);

  std::printf("T21 anchorNoop: PASS\n");
}

int main() {
  testColdStart();
  testNoop();
  testUpdateColor();
  testDelete();
  testAdd();
  testReparentLayer();
  testFullCycle();
  testUpdateTransform();
  testUpdatePaneRegion();
  testVisibility();
  testGradientUpdate();
  testAnchorUpdate();
  testTextureUpdate();
  testBlendModeUpdate();
  testPaneNameChange();
  testLayerNameChange();
  testGeometryFormatChange();
  testClearColorRemoval();
  testViewportInResult();
  testGradientNoop();
  testAnchorNoop();

  std::printf("\nAll D77.2 tests passed.\n");
  return 0;
}
