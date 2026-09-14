// ENC-984: sceneToDocument — read a SceneDocument back out of a live Scene.
//
// The gap this closes: serializeSceneDocument() has been round-trip tested since
// D77, but only ever against a document that came from parseSceneDocument. A
// Scene built the way every real host builds one — CommandProcessor JSON
// commands — could not be turned into a document at all, so a live chart had no
// exportable structure. These tests drive the FULL loop:
//
//   commands -> Scene -> sceneToDocument -> serialize -> parse -> reconcile
//            -> a SECOND Scene -> sceneToDocument -> serialize
//
// and require the two JSON strings to be byte-identical. That is the property
// that makes the export worth anything: the document you save restores the scene
// you saved it from.
#include "dc/document/SceneExport.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc_check.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace dc;

namespace {

// Apply a list of JSON commands, asserting each is accepted. Returns false on
// the first rejection (with the failing command printed).
bool applyAll(CommandProcessor& cp, const std::vector<std::string>& cmds) {
  for (const auto& c : cmds) {
    CmdResult r = cp.applyJsonText(c);
    if (!r.ok) {
      std::printf("  command rejected: %s\n    -> %s: %s\n", c.c_str(),
                  r.err.code.c_str(), r.err.message.c_str());
      return false;
    }
  }
  return true;
}

// A scene that exercises every field sceneToDocument has to carry: two panes
// (one with a clear color and a non-default region), two layers, a transform,
// an index buffer, a non-default vertex format, and a draw item with a
// non-default value in each style family (colors, widths, blend, clip, texture,
// anchor, visibility, gradient).
std::vector<std::string> richScene() {
  return {
      R"({"cmd":"createPane","id":1,"name":"price"})",
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":0.1,"clipYMax":0.9,"clipXMin":-0.8,"clipXMax":0.8})",
      R"({"cmd":"setPaneClearColor","id":1,"r":0.05,"g":0.0625,"b":0.075,"a":1.0})",
      R"({"cmd":"createPane","id":2,"name":"volume"})",

      R"({"cmd":"createLayer","id":10,"paneId":1,"name":"candles"})",
      R"({"cmd":"createLayer","id":11,"paneId":2,"name":"bars"})",

      R"({"cmd":"createTransform","id":30})",
      R"({"cmd":"setTransform","id":30,"tx":-0.25,"ty":0.5,"sx":2.5,"sy":0.125})",

      R"({"cmd":"createBuffer","id":100,"byteLength":240})",
      R"({"cmd":"createBuffer","id":101,"byteLength":24})",

      R"({"cmd":"createGeometry","id":200,"vertexBufferId":100,"vertexCount":10,"format":"candle6","indexBufferId":101,"indexCount":6})",
      R"({"cmd":"createGeometry","id":201,"vertexBufferId":100,"vertexCount":4,"format":"rect4"})",

      R"({"cmd":"createDrawItem","id":300,"layerId":10,"name":"ohlc"})",
      R"({"cmd":"bindDrawItem","drawItemId":300,"pipeline":"instancedCandle@1","geometryId":200})",
      R"({"cmd":"attachTransform","drawItemId":300,"transformId":30})",
      // One setDrawItemStyle carries every style family the Scene has
      // (colors / sizes / dash / corner / blend / clip) — that is the real
      // command surface; there is no separate setDrawItemDash or
      // setDrawItemBlendMode verb.
      R"({"cmd":"setDrawItemStyle","drawItemId":300,)"
      R"("r":0.25,"g":0.5,"b":0.625,"a":0.75,)"
      R"("colorUpR":0.125,"colorUpG":0.875,"colorUpB":0.25,"colorUpA":1.0,)"
      R"("colorDownR":0.875,"colorDownG":0.125,"colorDownB":0.25,"colorDownA":1.0,)"
      R"("pointSize":7.5,"lineWidth":3.25,"dashLength":6.0,"gapLength":2.5,"cornerRadius":4.0,)"
      R"("blendMode":"additive"})",
      R"({"cmd":"setDrawItemTexture","drawItemId":300,"textureId":7})",
      R"({"cmd":"setDrawItemAnchor","drawItemId":300,"anchor":"bottomRight","offsetX":12,"offsetY":-8})",
      R"({"cmd":"setDrawItemGradient","drawItemId":300,"type":"radial","angle":1.25,)"
      R"("color0":{"r":1,"g":0,"b":0,"a":1},"color1":{"r":0,"g":0,"b":1,"a":1},)"
      R"("center":{"x":0.25,"y":0.75},"radius":0.875})",

      R"({"cmd":"createDrawItem","id":301,"layerId":11,"name":"vol"})",
      R"({"cmd":"bindDrawItem","drawItemId":301,"pipeline":"instancedRect@1","geometryId":201})",
      R"({"cmd":"setDrawItemVisible","drawItemId":301,"visible":false})",
      R"({"cmd":"setDrawItemStyle","drawItemId":301,"isClipSource":true,"useClipMask":true})",
  };
}

}  // namespace

// ---------------------------------------------------------------------------
// 1. Extraction sees the whole scene, with the right values in the right slots.
// ---------------------------------------------------------------------------
static bool test_extract_fields() {
  std::printf("[enc984] extract: every resource and style field reaches the document\n");
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  DC_CHECK(applyAll(cp, richScene()));

  SceneDocument doc;
  sceneToDocument(scene, doc);

  DC_CHECK(doc.panes.size() == 2);
  DC_CHECK(doc.layers.size() == 2);
  DC_CHECK(doc.buffers.size() == 2);
  DC_CHECK(doc.geometries.size() == 2);
  DC_CHECK(doc.drawItems.size() == 2);
  DC_CHECK(doc.transforms.size() == 1);

  // Pane: name, region, clear color.
  const DocPane& p1 = doc.panes.at(1);
  DC_CHECK(p1.name == "price");
  DC_CHECK(std::fabs(p1.region.clipYMin - 0.1f) < 1e-5f);
  DC_CHECK(std::fabs(p1.region.clipXMax - 0.8f) < 1e-5f);
  DC_CHECK(p1.hasClearColor);
  DC_CHECK(std::fabs(p1.clearColor[2] - 0.075f) < 1e-5f);
  DC_CHECK(!doc.panes.at(2).hasClearColor);

  // Layer -> pane parentage.
  DC_CHECK(doc.layers.at(10).paneId == 1);
  DC_CHECK(doc.layers.at(11).paneId == 2);
  DC_CHECK(doc.layers.at(10).name == "candles");

  // Buffer byteLength survives; bytes deliberately do NOT (they are not in the
  // Scene — hosts read them with their own buffer readback).
  DC_CHECK(doc.buffers.at(100).byteLength == 240);
  DC_CHECK(doc.buffers.at(100).data.empty());

  // Transform params.
  const DocTransform& t = doc.transforms.at(30);
  DC_CHECK(std::fabs(t.tx + 0.25f) < 1e-5f);
  DC_CHECK(std::fabs(t.sx - 2.5f) < 1e-5f);
  DC_CHECK(std::fabs(t.sy - 0.125f) < 1e-5f);

  // Geometry: the vertex FORMAT must come back as the same string the parser
  // accepts, not as a number.
  const DocGeometry& g = doc.geometries.at(200);
  DC_CHECK(g.format == "candle6");
  DC_CHECK(g.vertexBufferId == 100);
  DC_CHECK(g.vertexCount == 10);
  DC_CHECK(g.indexBufferId == 101);
  DC_CHECK(g.indexCount == 6);
  DC_CHECK(doc.geometries.at(201).format == "rect4");

  // DrawItem: one assertion per style family.
  const DocDrawItem& d = doc.drawItems.at(300);
  DC_CHECK(d.layerId == 10);
  DC_CHECK(d.name == "ohlc");
  DC_CHECK(d.pipeline == "instancedCandle@1");
  DC_CHECK(d.geometryId == 200);
  DC_CHECK(d.transformId == 30);
  DC_CHECK(std::fabs(d.color[1] - 0.5f) < 1e-5f);
  DC_CHECK(std::fabs(d.colorUp[1] - 0.875f) < 1e-5f);
  DC_CHECK(std::fabs(d.colorDown[0] - 0.875f) < 1e-5f);
  DC_CHECK(std::fabs(d.pointSize - 7.5f) < 1e-5f);
  DC_CHECK(std::fabs(d.lineWidth - 3.25f) < 1e-5f);
  DC_CHECK(std::fabs(d.dashLength - 6.0f) < 1e-5f);
  DC_CHECK(std::fabs(d.gapLength - 2.5f) < 1e-5f);
  DC_CHECK(std::fabs(d.cornerRadius - 4.0f) < 1e-5f);
  DC_CHECK(d.blendMode == "additive");       // enum -> the parser's spelling
  DC_CHECK(d.textureId == 7);
  DC_CHECK(d.anchorPoint == "bottomRight");  // enum -> the parser's spelling
  DC_CHECK(std::fabs(d.anchorOffsetX - 12.0f) < 1e-5f);
  DC_CHECK(std::fabs(d.anchorOffsetY + 8.0f) < 1e-5f);
  DC_CHECK(d.gradientType == "radial");      // enum -> the parser's spelling
  DC_CHECK(std::fabs(d.gradientAngle - 1.25f) < 1e-5f);
  DC_CHECK(std::fabs(d.gradientRadius - 0.875f) < 1e-5f);
  DC_CHECK(std::fabs(d.gradientCenter[1] - 0.75f) < 1e-5f);
  DC_CHECK(d.visible);

  const DocDrawItem& d2 = doc.drawItems.at(301);
  DC_CHECK(!d2.visible);
  DC_CHECK(d2.isClipSource);
  DC_CHECK(d2.useClipMask);
  // An UNanchored item must leave anchorPoint empty — that is how both the
  // parser and the reconciler spell "no anchor" (there is no hasAnchor field in
  // the document). Emitting "topLeft" here would silently anchor it on restore.
  DC_CHECK(d2.anchorPoint.empty());
  // Likewise no gradient -> empty, not "none" and not "linear".
  DC_CHECK(d2.gradientType.empty());

  return true;
}

// ---------------------------------------------------------------------------
// 2. The full loop: export a live scene, restore it into a FRESH scene through
//    the real parse+reconcile path, export that, and demand identical JSON.
// ---------------------------------------------------------------------------
static bool test_round_trip_through_reconciler(bool compact) {
  std::printf("[enc984] round trip (compact=%d): scene -> json -> scene -> json\n",
              compact ? 1 : 0);

  Scene a;
  ResourceRegistry regA;
  CommandProcessor cpA(a, regA);
  DC_CHECK(applyAll(cpA, richScene()));

  const std::string json1 = serializeSceneAsDocument(a, compact);
  DC_CHECK(!json1.empty());

  // Restore into a completely fresh scene the way a real host would.
  SceneDocument parsed;
  DC_CHECK(parseSceneDocument(json1, parsed));

  Scene b;
  ResourceRegistry regB;
  CommandProcessor cpB(b, regB);
  SceneReconciler rec(cpB);
  ReconcileResult rr = rec.reconcile(parsed, b);
  if (!rr.ok) {
    for (const auto& e : rr.errors) std::printf("  reconcile error: %s\n", e.c_str());
  }
  DC_CHECK(rr.ok);

  // Same resource population.
  DC_CHECK(b.paneIds().size() == a.paneIds().size());
  DC_CHECK(b.layerIds().size() == a.layerIds().size());
  DC_CHECK(b.drawItemIds().size() == a.drawItemIds().size());
  DC_CHECK(b.bufferIds().size() == a.bufferIds().size());
  DC_CHECK(b.geometryIds().size() == a.geometryIds().size());
  DC_CHECK(b.transformIds().size() == a.transformIds().size());

  // And — the bar for THIS scene — re-exporting the restored scene reproduces
  // the same document text.
  //
  // Read this for exactly what it is: byte equality over the states richScene()
  // builds. It is NOT a proof that the loop is lossless in general, and saying
  // so would be wrong in two directions at once: `test_known_lossy_round_trips`
  // below exhibits states where json1 != json2, and — worse — states where
  // COMPACT mode reports "identical" precisely because both sides dropped the
  // same field. An equality criterion cannot see a value that neither side
  // serialized.
  const std::string json2 = serializeSceneAsDocument(b, compact);
  if (json1 != json2) {
    std::printf("  json1: %s\n", json1.c_str());
    std::printf("  json2: %s\n", json2.c_str());
  }
  DC_CHECK(json1 == json2);

  return true;
}

// ---------------------------------------------------------------------------
// 3. An empty scene exports a valid, re-parseable document (the save-before-
//    anything-is-drawn case).
// ---------------------------------------------------------------------------
static bool test_empty_scene() {
  std::printf("[enc984] empty scene exports a valid document\n");
  Scene s;
  const std::string json = serializeSceneAsDocument(s, false);
  DC_CHECK(!json.empty());

  SceneDocument doc;
  DC_CHECK(parseSceneDocument(json, doc));
  DC_CHECK(doc.panes.empty());
  DC_CHECK(doc.drawItems.empty());
  DC_CHECK(doc.version == 1);
  return true;
}

// ---------------------------------------------------------------------------
// 4. The export tracks DELETIONS — a stale document would be worse than none.
// ---------------------------------------------------------------------------
static bool test_tracks_deletion() {
  std::printf("[enc984] export reflects deletions (cascade included)\n");
  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  DC_CHECK(applyAll(cp, richScene()));

  SceneDocument before;
  sceneToDocument(scene, before);
  DC_CHECK(before.drawItems.count(301) == 1);
  DC_CHECK(before.layers.count(11) == 1);

  // Deleting pane 2 cascades to layer 11 and draw item 301.
  CmdResult r = cp.applyJsonText(R"({"cmd":"delete","kind":"pane","id":2})");
  DC_CHECK(r.ok);

  SceneDocument after;
  sceneToDocument(scene, after);
  DC_CHECK(after.panes.count(2) == 0);
  DC_CHECK(after.layers.count(11) == 0);
  DC_CHECK(after.drawItems.count(301) == 0);
  // …and the untouched pane's subtree is still there.
  DC_CHECK(after.panes.count(1) == 1);
  DC_CHECK(after.drawItems.count(300) == 1);
  return true;
}

// ---------------------------------------------------------------------------
// 5. Every enum->string table, exhaustively. These are the mappings most likely
//    to rot: a new BlendMode / AnchorPoint / VertexFormat added to the Scene
//    silently falls through to the default arm of the switch in SceneExport.cpp
//    and exports as the WRONG value rather than failing. richScene() pins one
//    value of each; this pins all of them.
// ---------------------------------------------------------------------------
static bool test_all_enum_spellings() {
  std::printf("[enc984] every blendMode / anchorPoint / gradientType / vertex format\n");

  // All 9 vertex formats survive as the string the parser accepts.
  const char* kFormats[] = {"pos2_clip", "rect4", "candle6", "glyph8", "pos2_alpha",
                            "pos2_color4", "pos2_uv4", "rect4_color", "point4_color"};
  for (int i = 0; i < 9; i++) {
    Scene sc; ResourceRegistry rg; CommandProcessor cp(sc, rg);
    DC_CHECK(applyAll(cp, {
        R"({"cmd":"createBuffer","id":10,"byteLength":256})",
        std::string(R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":")") +
            kFormats[i] + R"("})"}));
    SceneDocument d; sceneToDocument(sc, d);
    if (d.geometries.at(100).format != kFormats[i]) {
      std::printf("  format %s exported as '%s'\n", kFormats[i], d.geometries.at(100).format.c_str());
    }
    DC_CHECK(d.geometries.at(100).format == kFormats[i]);
  }

  // All 4 blend modes.
  const char* kBlends[] = {"normal", "additive", "multiply", "screen"};
  // All 9 anchor points, in enum order.
  const char* kAnchors[] = {"topLeft", "topCenter", "topRight", "middleLeft", "center",
                            "middleRight", "bottomLeft", "bottomCenter", "bottomRight"};
  // Both real gradient types ("none" is the absence, covered in test 1).
  const char* kGradients[] = {"linear", "radial"};

  for (int i = 0; i < 4; i++) {
    Scene sc; ResourceRegistry rg; CommandProcessor cp(sc, rg);
    DC_CHECK(applyAll(cp, {
        R"({"cmd":"createPane","id":1})", R"({"cmd":"createLayer","id":2,"paneId":1})",
        R"({"cmd":"createDrawItem","id":3,"layerId":2})",
        std::string(R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":")") + kBlends[i] + R"("})"}));
    SceneDocument d; sceneToDocument(sc, d);
    DC_CHECK(d.drawItems.at(3).blendMode == kBlends[i]);
  }

  for (int i = 0; i < 9; i++) {
    Scene sc; ResourceRegistry rg; CommandProcessor cp(sc, rg);
    DC_CHECK(applyAll(cp, {
        R"({"cmd":"createPane","id":1})", R"({"cmd":"createLayer","id":2,"paneId":1})",
        R"({"cmd":"createDrawItem","id":3,"layerId":2})",
        std::string(R"({"cmd":"setDrawItemAnchor","drawItemId":3,"anchor":")") + kAnchors[i] + R"("})"}));
    SceneDocument d; sceneToDocument(sc, d);
    DC_CHECK(d.drawItems.at(3).anchorPoint == kAnchors[i]);
  }

  for (int i = 0; i < 2; i++) {
    Scene sc; ResourceRegistry rg; CommandProcessor cp(sc, rg);
    DC_CHECK(applyAll(cp, {
        R"({"cmd":"createPane","id":1})", R"({"cmd":"createLayer","id":2,"paneId":1})",
        R"({"cmd":"createDrawItem","id":3,"layerId":2})",
        std::string(R"({"cmd":"setDrawItemGradient","drawItemId":3,"type":")") + kGradients[i] + R"("})"}));
    SceneDocument d; sceneToDocument(sc, d);
    DC_CHECK(d.drawItems.at(3).gradientType == kGradients[i]);
  }
  return true;
}

// ---------------------------------------------------------------------------
// 6. The KNOWN-LOSSY round trips, pinned deliberately.
//
// These assert that the loop LOSES something. That reads backwards until you
// notice the alternative: leaving them untested means the loss is invisible,
// and the compact-mode cases are invisible even to the byte-equality test above
// because both sides drop the same field. Pinning them here makes the boundary
// explicit, documents it in the place someone will look, and turns any future
// FIX into a loud test failure rather than a silent behaviour change.
//
// In all three cases sceneToDocument itself is FAITHFUL — it exports the value
// the Scene holds. The loss is downstream, in SceneReconciler (cases A and B)
// or in the SceneDocument schema (case C). See LIMITATIONS.md DC-L10.
// ---------------------------------------------------------------------------
static bool test_known_lossy_round_trips() {
  std::printf("[enc984] known-lossy cases are pinned, not papered over\n");

  // --- A. Gradient params behind a cleared gradientType. -------------------
  // setDrawItemGradient accepts type:"none" and still writes the params below
  // it (CommandProcessor.cpp), so the Scene legitimately holds gradientType=0
  // with a non-default angle/center/radius. The export carries them; the
  // reconciler only emits setDrawItemGradient when the type is non-empty, so
  // they do not come back.
  {
    Scene a; ResourceRegistry ra; CommandProcessor ca(a, ra);
    DC_CHECK(applyAll(ca, {
        R"({"cmd":"createPane","id":1})", R"({"cmd":"createLayer","id":2,"paneId":1})",
        R"({"cmd":"createDrawItem","id":3,"layerId":2})",
        R"({"cmd":"setDrawItemGradient","drawItemId":3,"type":"linear","angle":1.25,"center":{"x":0.25,"y":0.75},"radius":0.875})",
        R"({"cmd":"setDrawItemGradient","drawItemId":3,"type":"none"})"}));

    SceneDocument d; sceneToDocument(a, d);
    // The EXPORT is faithful: the stale params are in the document.
    DC_CHECK(d.drawItems.at(3).gradientType.empty());
    DC_CHECK(std::fabs(d.drawItems.at(3).gradientAngle - 1.25f) < 1e-5f);
    DC_CHECK(std::fabs(d.drawItems.at(3).gradientRadius - 0.875f) < 1e-5f);

    // The RESTORE is not. Non-compact, so the fields are on the wire both ways.
    const std::string json1 = serializeSceneAsDocument(a, false);
    SceneDocument parsed; DC_CHECK(parseSceneDocument(json1, parsed));
    Scene b; ResourceRegistry rb; CommandProcessor cb(b, rb);
    SceneReconciler rec(cb);
    DC_CHECK(rec.reconcile(parsed, b).ok);
    const DrawItem* di = b.getDrawItem(3);
    DC_CHECK(di != nullptr);
    DC_CHECK(std::fabs(di->gradientAngle - 0.0f) < 1e-5f);      // 1.25 was lost
    DC_CHECK(std::fabs(di->gradientRadius - 0.5f) < 1e-5f);     // 0.875 was lost
    DC_CHECK(serializeSceneAsDocument(b, false) != json1);      // and it SHOWS, non-compact

    // …but in COMPACT mode the same loop reports "identical", because both
    // sides omit gradient params when the type is empty. This is the case the
    // round-trip test structurally cannot detect.
    const std::string c1 = serializeSceneAsDocument(a, true);
    SceneDocument pc; DC_CHECK(parseSceneDocument(c1, pc));
    Scene cScene; ResourceRegistry rc; CommandProcessor cc(cScene, rc);
    SceneReconciler rec2(cc);
    DC_CHECK(rec2.reconcile(pc, cScene).ok);
    DC_CHECK(serializeSceneAsDocument(cScene, true) == c1);     // "identical" — and lossy
  }

  // --- B. Clear color behind hasClearColor=false. --------------------------
  // setPaneClearColor enabled:false clears only the FLAG, leaving the colour in
  // the Scene. The export carries it (non-compact writes clearColor either way);
  // SceneReconciler::reconcilePanes only emits setPaneClearColor when the doc
  // says hasClearColor, so the colour resets to the default on restore.
  {
    Scene a; ResourceRegistry ra; CommandProcessor ca(a, ra);
    DC_CHECK(applyAll(ca, {
        R"({"cmd":"createPane","id":1})",
        R"({"cmd":"setPaneClearColor","id":1,"r":0.5,"g":0.25,"b":0.125,"a":1})",
        R"({"cmd":"setPaneClearColor","id":1,"enabled":false})"}));

    SceneDocument d; sceneToDocument(a, d);
    DC_CHECK(!d.panes.at(1).hasClearColor);
    DC_CHECK(std::fabs(d.panes.at(1).clearColor[0] - 0.5f) < 1e-5f);   // export faithful

    const std::string json1 = serializeSceneAsDocument(a, false);
    SceneDocument parsed; DC_CHECK(parseSceneDocument(json1, parsed));
    Scene b; ResourceRegistry rb; CommandProcessor cb(b, rb);
    SceneReconciler rec(cb);
    DC_CHECK(rec.reconcile(parsed, b).ok);
    const Pane* p = b.getPane(1);
    DC_CHECK(p != nullptr);
    DC_CHECK(std::fabs(p->clearColor[0] - 0.0f) < 1e-5f);              // 0.5 was lost
  }

  // --- C. Geometry bounds have no SceneDocument field at all. --------------
  // boundsMin/boundsMax/boundsValid are live Scene state (DawnSceneRenderer
  // frustum-culls on them), set by the real setGeometryBounds command. The
  // SceneDocument schema has no slot for them, so they cannot be exported —
  // note the D45 dc::serializeScene DOES carry them, so relative to that older
  // serializer this dialect is less faithful here.
  {
    Scene a; ResourceRegistry ra; CommandProcessor ca(a, ra);
    DC_CHECK(applyAll(ca, {
        R"({"cmd":"createBuffer","id":10,"byteLength":24})",
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3})",
        R"({"cmd":"setGeometryBounds","geometryId":100,"minX":-1,"minY":-2,"maxX":3,"maxY":4})"}));
    const Geometry* g = a.getGeometry(100);
    DC_CHECK(g != nullptr && g->boundsValid);
    DC_CHECK(std::fabs(g->boundsMin[0] + 1.0f) < 1e-5f);

    // Nothing in the exported document mentions bounds, in either mode.
    const std::string json = serializeSceneAsDocument(a, false);
    DC_CHECK(json.find("bounds") == std::string::npos);
    DC_CHECK(serializeSceneAsDocument(a, true).find("bounds") == std::string::npos);
  }

  return true;
}

int main() {
  int failures = 0;
  if (!test_extract_fields()) failures++;
  if (!test_round_trip_through_reconciler(false)) failures++;
  if (!test_round_trip_through_reconciler(true)) failures++;
  if (!test_empty_scene()) failures++;
  if (!test_tracks_deletion()) failures++;
  if (!test_all_enum_spellings()) failures++;
  if (!test_known_lossy_round_trips()) failures++;

  if (failures) {
    std::printf("ENC-984 scene export: %d test(s) FAILED\n", failures);
    return 1;
  }
  std::printf("ENC-984 scene export: all tests passed\n");
  return 0;
}
