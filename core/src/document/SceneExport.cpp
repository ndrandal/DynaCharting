// ENC-984: SceneExport — Scene -> SceneDocument extraction. See SceneExport.hpp
// for what this carries and what it deliberately does not.
//
// Every string spelling here is the one the INBOUND path already uses, so the
// output re-parses to the same values:
//   blendMode      <- SceneReconciler::blendModeToString
//   anchorPoint    <- the inverse of SceneReconciler::anchorStringToEnum /
//                     CommandProcessor's setDrawItemAnchor table
//   gradientType   <- the inverse of SceneReconciler::gradientStringToEnum
//   geometry format<- dc::toString(VertexFormat)
// If any of those tables gains a value, the matching case below must gain it
// too; dc_enc984_scene_export pins every one of them.
#include "dc/document/SceneExport.hpp"

#include <cstring>

namespace dc {

namespace {

const char* blendModeName(BlendMode bm) {
  switch (bm) {
    case BlendMode::Normal:   return "normal";
    case BlendMode::Additive: return "additive";
    case BlendMode::Multiply: return "multiply";
    case BlendMode::Screen:   return "screen";
  }
  return "normal";
}

// Inverse of SceneReconciler::anchorStringToEnum (and of CommandProcessor's
// setDrawItemAnchor table, which is the same 9 names in the same order).
const char* anchorName(std::uint8_t ap) {
  switch (ap) {
    case 0: return "topLeft";
    case 1: return "topCenter";
    case 2: return "topRight";
    case 3: return "middleLeft";
    case 4: return "center";
    case 5: return "middleRight";
    case 6: return "bottomLeft";
    case 7: return "bottomCenter";
    case 8: return "bottomRight";
  }
  return "topLeft";
}

// Inverse of SceneReconciler::gradientStringToEnum. 0 maps to the EMPTY string
// (not "none"): DocDrawItem::gradientType defaults to empty, so an item with no
// gradient compares equal to the default and compact serialization omits it.
const char* gradientName(std::uint8_t gt) {
  switch (gt) {
    case 1: return "linear";
    case 2: return "radial";
  }
  return "";
}

void copy4(float (&dst)[4], const float (&src)[4]) {
  std::memcpy(dst, src, sizeof(dst));
}

void copy2(float (&dst)[2], const float (&src)[2]) {
  std::memcpy(dst, src, sizeof(dst));
}

}  // namespace

void sceneToDocument(const Scene& scene, SceneDocument& out) {
  out = SceneDocument{};

  for (Id id : scene.bufferIds()) {
    const Buffer* b = scene.getBuffer(id);
    if (!b) continue;
    DocBuffer db;
    db.byteLength = b->byteLength;
    // db.data stays empty — the bytes are not in the Scene (SceneExport.hpp).
    out.buffers[id] = std::move(db);
  }

  for (Id id : scene.transformIds()) {
    const Transform* t = scene.getTransform(id);
    if (!t) continue;
    DocTransform dt;
    dt.tx = t->params.tx;
    dt.ty = t->params.ty;
    dt.sx = t->params.sx;
    dt.sy = t->params.sy;
    out.transforms[id] = dt;
  }

  for (Id id : scene.paneIds()) {
    const Pane* p = scene.getPane(id);
    if (!p) continue;
    DocPane dp;
    dp.name = p->name;
    dp.region.clipYMin = p->region.clipYMin;
    dp.region.clipYMax = p->region.clipYMax;
    dp.region.clipXMin = p->region.clipXMin;
    dp.region.clipXMax = p->region.clipXMax;
    copy4(dp.clearColor, p->clearColor);
    dp.hasClearColor = p->hasClearColor;
    out.panes[id] = std::move(dp);
  }

  for (Id id : scene.layerIds()) {
    const Layer* l = scene.getLayer(id);
    if (!l) continue;
    DocLayer dl;
    dl.paneId = l->paneId;
    dl.name = l->name;
    out.layers[id] = std::move(dl);
  }

  for (Id id : scene.geometryIds()) {
    const Geometry* g = scene.getGeometry(id);
    if (!g) continue;
    DocGeometry dg;
    dg.vertexBufferId = g->vertexBufferId;
    dg.format = toString(g->format);
    dg.vertexCount = g->vertexCount;
    dg.indexBufferId = g->indexBufferId;
    dg.indexCount = g->indexCount;
    out.geometries[id] = std::move(dg);
  }

  for (Id id : scene.drawItemIds()) {
    const DrawItem* d = scene.getDrawItem(id);
    if (!d) continue;
    DocDrawItem dd;
    dd.layerId = d->layerId;
    dd.name = d->name;
    dd.pipeline = d->pipeline;
    dd.geometryId = d->geometryId;
    dd.transformId = d->transformId;

    copy4(dd.color, d->color);
    copy4(dd.colorUp, d->colorUp);
    copy4(dd.colorDown, d->colorDown);

    dd.pointSize = d->pointSize;
    dd.lineWidth = d->lineWidth;
    dd.dashLength = d->dashLength;
    dd.gapLength = d->gapLength;
    dd.cornerRadius = d->cornerRadius;

    dd.blendMode = blendModeName(d->blendMode);
    dd.isClipSource = d->isClipSource;
    dd.useClipMask = d->useClipMask;

    dd.textureId = d->textureId;

    // An unanchored item keeps anchorPoint EMPTY — that is how both the parser
    // and the reconciler spell "no anchor" (hasAnchor has no document field).
    if (d->hasAnchor) {
      dd.anchorPoint = anchorName(d->anchorPoint);
      dd.anchorOffsetX = d->anchorOffsetX;
      dd.anchorOffsetY = d->anchorOffsetY;
    }

    dd.visible = d->visible;

    dd.gradientType = gradientName(d->gradientType);
    dd.gradientAngle = d->gradientAngle;
    copy4(dd.gradientColor0, d->gradientColor0);
    copy4(dd.gradientColor1, d->gradientColor1);
    copy2(dd.gradientCenter, d->gradientCenter);
    dd.gradientRadius = d->gradientRadius;

    out.drawItems[id] = std::move(dd);
  }
}

std::string serializeScene(const Scene& scene, bool compact) {
  SceneDocument doc;
  sceneToDocument(scene, doc);
  return serializeSceneDocument(doc, compact);
}

}  // namespace dc
