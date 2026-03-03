// D77: SceneReconciler — diffs a SceneDocument against the current Scene,
// emits JSON commands via CommandProcessor to bring the Scene into the desired state.
#include "dc/document/SceneReconciler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace dc {

SceneReconciler::SceneReconciler(CommandProcessor& cp) : cp_(cp) {}

bool SceneReconciler::emit(const std::string& json, ReconcileResult& result) {
  auto r = cp_.applyJsonText(json);
  if (!r.ok) {
    result.ok = false;
    result.errors.push_back(r.err.message + " [" + r.err.code + "] " + json);
    return false;
  }
  return true;
}

std::string SceneReconciler::fmtFloat(float v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
  return buf;
}

std::string SceneReconciler::escapeJson(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 4);
  for (char c : s) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:   out += c;
    }
  }
  return out;
}

bool SceneReconciler::floatEq(float a, float b) {
  return std::fabs(a - b) < 1e-6f;
}

bool SceneReconciler::color4Eq(const float a[4], const float b[4]) {
  return floatEq(a[0], b[0]) && floatEq(a[1], b[1]) &&
         floatEq(a[2], b[2]) && floatEq(a[3], b[3]);
}

bool SceneReconciler::color2Eq(const float a[2], const float b[2]) {
  return floatEq(a[0], b[0]) && floatEq(a[1], b[1]);
}

std::uint8_t SceneReconciler::anchorStringToEnum(const std::string& s) {
  if (s == "topLeft")      return 0;
  if (s == "topCenter")    return 1;
  if (s == "topRight")     return 2;
  if (s == "middleLeft")   return 3;
  if (s == "center")       return 4;
  if (s == "middleRight")  return 5;
  if (s == "bottomLeft")   return 6;
  if (s == "bottomCenter") return 7;
  if (s == "bottomRight")  return 8;
  return 0;
}

std::uint8_t SceneReconciler::gradientStringToEnum(const std::string& s) {
  if (s == "linear")  return 1;
  if (s == "radial")  return 2;
  return 0; // "none" or empty
}

std::string SceneReconciler::blendModeToString(BlendMode bm) {
  switch (bm) {
    case BlendMode::Normal:   return "normal";
    case BlendMode::Additive: return "additive";
    case BlendMode::Multiply: return "multiply";
    case BlendMode::Screen:   return "screen";
    default:                  return "normal";
  }
}

// --- Main reconcile ---

ReconcileResult SceneReconciler::reconcile(const SceneDocument& newDoc, const Scene& currentScene) {
  ReconcileResult result;

  // Surface viewport dimensions from the document
  result.viewportWidth = newDoc.viewportWidth;
  result.viewportHeight = newDoc.viewportHeight;

  // Begin frame for atomicity
  if (!emit(R"({"cmd":"beginFrame"})", result)) return result;

  // Deletions in reverse dependency order
  deleteRemovedDrawItems(newDoc, currentScene, result);
  deleteRemovedGeometries(newDoc, currentScene, result);
  deleteRemovedLayers(newDoc, currentScene, result);
  deleteRemovedPanes(newDoc, currentScene, result);
  deleteRemovedTransforms(newDoc, currentScene, result);
  deleteRemovedBuffers(newDoc, currentScene, result);

  // Creations & updates in dependency order
  reconcileBuffers(newDoc, currentScene, result);
  reconcileTransforms(newDoc, currentScene, result);
  reconcilePanes(newDoc, currentScene, result);
  reconcileLayers(newDoc, currentScene, result);
  reconcileGeometries(newDoc, currentScene, result);
  reconcileDrawItems(newDoc, currentScene, result);

  // Commit frame
  if (result.ok) {
    emit(R"({"cmd":"commitFrame"})", result);
  } else {
    // Still commit (will fail with FRAME_REJECTED, that's expected)
    cp_.applyJsonText(R"({"cmd":"commitFrame"})");
  }

  return result;
}

// --- Buffers ---

void SceneReconciler::reconcileBuffers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, db] : newDoc.buffers) {
    if (scene.hasBuffer(id)) {
      // Buffer exists — no updatable fields beyond byteLength (which is metadata)
      continue;
    }
    // Create
    std::string cmd = "{\"cmd\":\"createBuffer\",\"id\":" + std::to_string(id) +
                      ",\"byteLength\":" + std::to_string(db.byteLength) + "}";
    if (emit(cmd, r)) r.created++;
  }
}

void SceneReconciler::deleteRemovedBuffers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.bufferIds()) {
    if (newDoc.buffers.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

// --- Transforms ---

void SceneReconciler::reconcileTransforms(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, dt] : newDoc.transforms) {
    if (scene.hasTransform(id)) {
      // Update if changed
      const Transform* cur = scene.getTransform(id);
      if (!cur) continue;
      if (!floatEq(cur->params.tx, dt.tx) || !floatEq(cur->params.ty, dt.ty) ||
          !floatEq(cur->params.sx, dt.sx) || !floatEq(cur->params.sy, dt.sy)) {
        std::string cmd = "{\"cmd\":\"setTransform\",\"id\":" + std::to_string(id) +
                          ",\"tx\":" + fmtFloat(dt.tx) +
                          ",\"ty\":" + fmtFloat(dt.ty) +
                          ",\"sx\":" + fmtFloat(dt.sx) +
                          ",\"sy\":" + fmtFloat(dt.sy) + "}";
        if (emit(cmd, r)) r.updated++;
      }
    } else {
      // Create + set
      std::string cmd = "{\"cmd\":\"createTransform\",\"id\":" + std::to_string(id) + "}";
      if (!emit(cmd, r)) continue;
      cmd = "{\"cmd\":\"setTransform\",\"id\":" + std::to_string(id) +
            ",\"tx\":" + fmtFloat(dt.tx) +
            ",\"ty\":" + fmtFloat(dt.ty) +
            ",\"sx\":" + fmtFloat(dt.sx) +
            ",\"sy\":" + fmtFloat(dt.sy) + "}";
      emit(cmd, r);
      r.created++;
    }
  }
}

void SceneReconciler::deleteRemovedTransforms(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.transformIds()) {
    if (newDoc.transforms.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

// --- Panes ---

void SceneReconciler::reconcilePanes(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, dp] : newDoc.panes) {
    if (scene.hasPane(id)) {
      const Pane* cur = scene.getPane(id);
      if (!cur) continue;

      // Name change: no rename command exists, so delete + recreate
      if (cur->name != dp.name) {
        std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
        if (!emit(cmd, r)) continue;
        r.deleted++;
        // Fall through to create below
      } else {
        // Update region if changed
        if (!floatEq(cur->region.clipYMin, dp.region.clipYMin) ||
            !floatEq(cur->region.clipYMax, dp.region.clipYMax) ||
            !floatEq(cur->region.clipXMin, dp.region.clipXMin) ||
            !floatEq(cur->region.clipXMax, dp.region.clipXMax)) {
          std::string cmd = "{\"cmd\":\"setPaneRegion\",\"id\":" + std::to_string(id) +
                            ",\"clipYMin\":" + fmtFloat(dp.region.clipYMin) +
                            ",\"clipYMax\":" + fmtFloat(dp.region.clipYMax) +
                            ",\"clipXMin\":" + fmtFloat(dp.region.clipXMin) +
                            ",\"clipXMax\":" + fmtFloat(dp.region.clipXMax) + "}";
          if (emit(cmd, r)) r.updated++;
        }

        // Clear color: set, update, or remove
        if (dp.hasClearColor) {
          if (!cur->hasClearColor || !color4Eq(cur->clearColor, dp.clearColor)) {
            std::string cmd = "{\"cmd\":\"setPaneClearColor\",\"id\":" + std::to_string(id) +
                              ",\"r\":" + fmtFloat(dp.clearColor[0]) +
                              ",\"g\":" + fmtFloat(dp.clearColor[1]) +
                              ",\"b\":" + fmtFloat(dp.clearColor[2]) +
                              ",\"a\":" + fmtFloat(dp.clearColor[3]) + "}";
            emit(cmd, r);
          }
        } else if (cur->hasClearColor) {
          // Clear color was removed
          std::string cmd = "{\"cmd\":\"setPaneClearColor\",\"id\":" + std::to_string(id) +
                            ",\"enabled\":false}";
          emit(cmd, r);
        }
        continue;
      }
    }

    // Create pane (new or after name-change delete)
    std::string cmd = "{\"cmd\":\"createPane\",\"id\":" + std::to_string(id) +
                      ",\"name\":\"" + escapeJson(dp.name) + "\"}";
    if (!emit(cmd, r)) continue;
    r.created++;

    // Set region
    cmd = "{\"cmd\":\"setPaneRegion\",\"id\":" + std::to_string(id) +
          ",\"clipYMin\":" + fmtFloat(dp.region.clipYMin) +
          ",\"clipYMax\":" + fmtFloat(dp.region.clipYMax) +
          ",\"clipXMin\":" + fmtFloat(dp.region.clipXMin) +
          ",\"clipXMax\":" + fmtFloat(dp.region.clipXMax) + "}";
    emit(cmd, r);

    // Set clear color
    if (dp.hasClearColor) {
      cmd = "{\"cmd\":\"setPaneClearColor\",\"id\":" + std::to_string(id) +
            ",\"r\":" + fmtFloat(dp.clearColor[0]) +
            ",\"g\":" + fmtFloat(dp.clearColor[1]) +
            ",\"b\":" + fmtFloat(dp.clearColor[2]) +
            ",\"a\":" + fmtFloat(dp.clearColor[3]) + "}";
      emit(cmd, r);
    }
  }
}

void SceneReconciler::deleteRemovedPanes(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.paneIds()) {
    if (newDoc.panes.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

// --- Layers ---

void SceneReconciler::reconcileLayers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, dl] : newDoc.layers) {
    if (scene.hasLayer(id)) {
      const Layer* cur = scene.getLayer(id);
      if (!cur) continue;
      // Re-parent or name change: delete + recreate (no rename/reparent commands exist)
      if (cur->paneId != dl.paneId || cur->name != dl.name) {
        std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
        if (!emit(cmd, r)) continue;
        r.deleted++;
        // Fall through to create below
      } else {
        continue; // no changes
      }
    }
    // Create
    std::string cmd = "{\"cmd\":\"createLayer\",\"id\":" + std::to_string(id) +
                      ",\"paneId\":" + std::to_string(dl.paneId) +
                      ",\"name\":\"" + escapeJson(dl.name) + "\"}";
    if (emit(cmd, r)) r.created++;
  }
}

void SceneReconciler::deleteRemovedLayers(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.layerIds()) {
    if (newDoc.layers.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

// --- Geometries ---

void SceneReconciler::reconcileGeometries(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, dg] : newDoc.geometries) {
    if (scene.hasGeometry(id)) {
      const Geometry* cur = scene.getGeometry(id);
      if (!cur) continue;

      // Format change: format is immutable, must delete + recreate
      if (toString(cur->format) != dg.format) {
        std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
        if (!emit(cmd, r)) continue;
        r.deleted++;
        // Fall through to create below
      } else {
        // Update vertex count if changed
        if (cur->vertexCount != dg.vertexCount) {
          std::string cmd = "{\"cmd\":\"setGeometryVertexCount\",\"geometryId\":" + std::to_string(id) +
                            ",\"vertexCount\":" + std::to_string(dg.vertexCount) + "}";
          if (emit(cmd, r)) r.updated++;
        }

        // If vertexBufferId changed, update via setGeometryBuffer
        if (cur->vertexBufferId != dg.vertexBufferId) {
          std::string cmd = "{\"cmd\":\"setGeometryBuffer\",\"geometryId\":" + std::to_string(id) +
                            ",\"vertexBufferId\":" + std::to_string(dg.vertexBufferId) + "}";
          emit(cmd, r);
        }
        continue;
      }
    }

    // Create (new or after format-change delete)
    std::string cmd = "{\"cmd\":\"createGeometry\",\"id\":" + std::to_string(id) +
                      ",\"vertexBufferId\":" + std::to_string(dg.vertexBufferId) +
                      ",\"format\":\"" + dg.format + "\"" +
                      ",\"vertexCount\":" + std::to_string(dg.vertexCount);
    if (dg.indexBufferId != 0) {
      cmd += ",\"indexBufferId\":" + std::to_string(dg.indexBufferId);
    }
    if (dg.indexCount != 0) {
      cmd += ",\"indexCount\":" + std::to_string(dg.indexCount);
    }
    cmd += "}";
    if (emit(cmd, r)) r.created++;
  }
}

void SceneReconciler::deleteRemovedGeometries(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.geometryIds()) {
    if (newDoc.geometries.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

// --- DrawItems ---

void SceneReconciler::reconcileDrawItems(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (const auto& [id, dd] : newDoc.drawItems) {
    if (scene.hasDrawItem(id)) {
      const DrawItem* cur = scene.getDrawItem(id);
      if (!cur) continue;

      // Re-parent: if layerId changed, delete + recreate
      if (cur->layerId != dd.layerId) {
        std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
        if (!emit(cmd, r)) continue;
        r.deleted++;
        // Fall through to create
      } else {
        // Update existing drawItem

        // Pipeline/geometry change
        if (cur->pipeline != dd.pipeline || cur->geometryId != dd.geometryId) {
          if (!dd.pipeline.empty() && dd.geometryId != 0) {
            std::string cmd = "{\"cmd\":\"bindDrawItem\",\"drawItemId\":" + std::to_string(id) +
                              ",\"pipeline\":\"" + dd.pipeline + "\"" +
                              ",\"geometryId\":" + std::to_string(dd.geometryId) + "}";
            emit(cmd, r);
          }
        }

        // Transform change
        if (cur->transformId != dd.transformId) {
          std::string cmd = "{\"cmd\":\"attachTransform\",\"drawItemId\":" + std::to_string(id) +
                            ",\"transformId\":" + std::to_string(dd.transformId) + "}";
          emit(cmd, r);
        }

        // Color/style changes — use setDrawItemStyle for bulk update
        bool styleChanged =
            !color4Eq(cur->color, dd.color) ||
            !color4Eq(cur->colorUp, dd.colorUp) ||
            !color4Eq(cur->colorDown, dd.colorDown) ||
            !floatEq(cur->pointSize, dd.pointSize) ||
            !floatEq(cur->lineWidth, dd.lineWidth) ||
            !floatEq(cur->dashLength, dd.dashLength) ||
            !floatEq(cur->gapLength, dd.gapLength) ||
            !floatEq(cur->cornerRadius, dd.cornerRadius) ||
            blendModeToString(cur->blendMode) != dd.blendMode ||
            cur->isClipSource != dd.isClipSource ||
            cur->useClipMask != dd.useClipMask;

        if (styleChanged) {
          std::string cmd = "{\"cmd\":\"setDrawItemStyle\",\"drawItemId\":" + std::to_string(id) +
                            ",\"r\":" + fmtFloat(dd.color[0]) +
                            ",\"g\":" + fmtFloat(dd.color[1]) +
                            ",\"b\":" + fmtFloat(dd.color[2]) +
                            ",\"a\":" + fmtFloat(dd.color[3]) +
                            ",\"colorUpR\":" + fmtFloat(dd.colorUp[0]) +
                            ",\"colorUpG\":" + fmtFloat(dd.colorUp[1]) +
                            ",\"colorUpB\":" + fmtFloat(dd.colorUp[2]) +
                            ",\"colorUpA\":" + fmtFloat(dd.colorUp[3]) +
                            ",\"colorDownR\":" + fmtFloat(dd.colorDown[0]) +
                            ",\"colorDownG\":" + fmtFloat(dd.colorDown[1]) +
                            ",\"colorDownB\":" + fmtFloat(dd.colorDown[2]) +
                            ",\"colorDownA\":" + fmtFloat(dd.colorDown[3]) +
                            ",\"pointSize\":" + fmtFloat(dd.pointSize) +
                            ",\"lineWidth\":" + fmtFloat(dd.lineWidth) +
                            ",\"dashLength\":" + fmtFloat(dd.dashLength) +
                            ",\"gapLength\":" + fmtFloat(dd.gapLength) +
                            ",\"cornerRadius\":" + fmtFloat(dd.cornerRadius) +
                            ",\"blendMode\":\"" + dd.blendMode + "\"" +
                            ",\"isClipSource\":" + (dd.isClipSource ? "true" : "false") +
                            ",\"useClipMask\":" + (dd.useClipMask ? "true" : "false") + "}";
          if (emit(cmd, r)) r.updated++;
        }

        // Visibility
        if (cur->visible != dd.visible) {
          std::string cmd = "{\"cmd\":\"setDrawItemVisible\",\"drawItemId\":" + std::to_string(id) +
                            ",\"visible\":" + (dd.visible ? "true" : "false") + "}";
          emit(cmd, r);
        }

        // Texture
        if (cur->textureId != dd.textureId) {
          std::string cmd = "{\"cmd\":\"setDrawItemTexture\",\"drawItemId\":" + std::to_string(id) +
                            ",\"textureId\":" + std::to_string(dd.textureId) + "}";
          emit(cmd, r);
        }

        // Anchor — diff against current scene state
        {
          bool docHasAnchor = !dd.anchorPoint.empty();
          bool anchorChanged = false;

          if (docHasAnchor && cur->hasAnchor) {
            // Both have anchor — compare fields
            anchorChanged = anchorStringToEnum(dd.anchorPoint) != cur->anchorPoint ||
                            !floatEq(dd.anchorOffsetX, cur->anchorOffsetX) ||
                            !floatEq(dd.anchorOffsetY, cur->anchorOffsetY);
          } else if (docHasAnchor != cur->hasAnchor) {
            anchorChanged = true;
          }

          if (anchorChanged && docHasAnchor) {
            std::string cmd = "{\"cmd\":\"setDrawItemAnchor\",\"drawItemId\":" + std::to_string(id) +
                              ",\"anchor\":\"" + dd.anchorPoint + "\"" +
                              ",\"offsetX\":" + fmtFloat(dd.anchorOffsetX) +
                              ",\"offsetY\":" + fmtFloat(dd.anchorOffsetY) + "}";
            emit(cmd, r);
          }
          // Note: if anchor was removed (docHasAnchor == false, cur->hasAnchor == true),
          // there is no "removeAnchor" command — would need delete+recreate to clear it.
        }

        // Gradient — diff against current scene state
        {
          std::uint8_t docGradType = gradientStringToEnum(dd.gradientType);
          bool gradientChanged = false;

          if (docGradType != cur->gradientType) {
            gradientChanged = true;
          } else if (docGradType != 0) {
            // Same type, compare parameters
            gradientChanged = !floatEq(dd.gradientAngle, cur->gradientAngle) ||
                              !color4Eq(dd.gradientColor0, cur->gradientColor0) ||
                              !color4Eq(dd.gradientColor1, cur->gradientColor1) ||
                              !color2Eq(dd.gradientCenter, cur->gradientCenter) ||
                              !floatEq(dd.gradientRadius, cur->gradientRadius);
          }

          if (gradientChanged) {
            std::string cmd = "{\"cmd\":\"setDrawItemGradient\",\"drawItemId\":" + std::to_string(id) +
                              ",\"type\":\"" + dd.gradientType + "\"" +
                              ",\"angle\":" + fmtFloat(dd.gradientAngle) +
                              ",\"color0\":{\"r\":" + fmtFloat(dd.gradientColor0[0]) +
                              ",\"g\":" + fmtFloat(dd.gradientColor0[1]) +
                              ",\"b\":" + fmtFloat(dd.gradientColor0[2]) +
                              ",\"a\":" + fmtFloat(dd.gradientColor0[3]) + "}" +
                              ",\"color1\":{\"r\":" + fmtFloat(dd.gradientColor1[0]) +
                              ",\"g\":" + fmtFloat(dd.gradientColor1[1]) +
                              ",\"b\":" + fmtFloat(dd.gradientColor1[2]) +
                              ",\"a\":" + fmtFloat(dd.gradientColor1[3]) + "}" +
                              ",\"center\":{\"x\":" + fmtFloat(dd.gradientCenter[0]) +
                              ",\"y\":" + fmtFloat(dd.gradientCenter[1]) + "}" +
                              ",\"radius\":" + fmtFloat(dd.gradientRadius) + "}";
            emit(cmd, r);
          }
        }

        continue; // done updating
      }
    }

    // Create new drawItem (or re-create after re-parent delete)
    std::string cmd = "{\"cmd\":\"createDrawItem\",\"id\":" + std::to_string(id) +
                      ",\"layerId\":" + std::to_string(dd.layerId) +
                      ",\"name\":\"" + escapeJson(dd.name) + "\"}";
    if (!emit(cmd, r)) continue;
    r.created++;

    // Bind pipeline + geometry
    if (!dd.pipeline.empty() && dd.geometryId != 0) {
      cmd = "{\"cmd\":\"bindDrawItem\",\"drawItemId\":" + std::to_string(id) +
            ",\"pipeline\":\"" + dd.pipeline + "\"" +
            ",\"geometryId\":" + std::to_string(dd.geometryId) + "}";
      emit(cmd, r);
    }

    // Attach transform
    if (dd.transformId != 0) {
      cmd = "{\"cmd\":\"attachTransform\",\"drawItemId\":" + std::to_string(id) +
            ",\"transformId\":" + std::to_string(dd.transformId) + "}";
      emit(cmd, r);
    }

    // Set style
    cmd = "{\"cmd\":\"setDrawItemStyle\",\"drawItemId\":" + std::to_string(id) +
          ",\"r\":" + fmtFloat(dd.color[0]) +
          ",\"g\":" + fmtFloat(dd.color[1]) +
          ",\"b\":" + fmtFloat(dd.color[2]) +
          ",\"a\":" + fmtFloat(dd.color[3]) +
          ",\"colorUpR\":" + fmtFloat(dd.colorUp[0]) +
          ",\"colorUpG\":" + fmtFloat(dd.colorUp[1]) +
          ",\"colorUpB\":" + fmtFloat(dd.colorUp[2]) +
          ",\"colorUpA\":" + fmtFloat(dd.colorUp[3]) +
          ",\"colorDownR\":" + fmtFloat(dd.colorDown[0]) +
          ",\"colorDownG\":" + fmtFloat(dd.colorDown[1]) +
          ",\"colorDownB\":" + fmtFloat(dd.colorDown[2]) +
          ",\"colorDownA\":" + fmtFloat(dd.colorDown[3]) +
          ",\"pointSize\":" + fmtFloat(dd.pointSize) +
          ",\"lineWidth\":" + fmtFloat(dd.lineWidth) +
          ",\"dashLength\":" + fmtFloat(dd.dashLength) +
          ",\"gapLength\":" + fmtFloat(dd.gapLength) +
          ",\"cornerRadius\":" + fmtFloat(dd.cornerRadius) +
          ",\"blendMode\":\"" + dd.blendMode + "\"" +
          ",\"isClipSource\":" + (dd.isClipSource ? "true" : "false") +
          ",\"useClipMask\":" + (dd.useClipMask ? "true" : "false") + "}";
    emit(cmd, r);

    // Visibility (only if non-default)
    if (!dd.visible) {
      cmd = "{\"cmd\":\"setDrawItemVisible\",\"drawItemId\":" + std::to_string(id) +
            ",\"visible\":false}";
      emit(cmd, r);
    }

    // Texture
    if (dd.textureId != 0) {
      cmd = "{\"cmd\":\"setDrawItemTexture\",\"drawItemId\":" + std::to_string(id) +
            ",\"textureId\":" + std::to_string(dd.textureId) + "}";
      emit(cmd, r);
    }

    // Anchor
    if (!dd.anchorPoint.empty()) {
      cmd = "{\"cmd\":\"setDrawItemAnchor\",\"drawItemId\":" + std::to_string(id) +
            ",\"anchor\":\"" + dd.anchorPoint + "\"" +
            ",\"offsetX\":" + fmtFloat(dd.anchorOffsetX) +
            ",\"offsetY\":" + fmtFloat(dd.anchorOffsetY) + "}";
      emit(cmd, r);
    }

    // Gradient
    bool hasGradient = !dd.gradientType.empty() && dd.gradientType != "none";
    if (hasGradient) {
      cmd = "{\"cmd\":\"setDrawItemGradient\",\"drawItemId\":" + std::to_string(id) +
            ",\"type\":\"" + dd.gradientType + "\"" +
            ",\"angle\":" + fmtFloat(dd.gradientAngle) +
            ",\"color0\":{\"r\":" + fmtFloat(dd.gradientColor0[0]) +
            ",\"g\":" + fmtFloat(dd.gradientColor0[1]) +
            ",\"b\":" + fmtFloat(dd.gradientColor0[2]) +
            ",\"a\":" + fmtFloat(dd.gradientColor0[3]) + "}" +
            ",\"color1\":{\"r\":" + fmtFloat(dd.gradientColor1[0]) +
            ",\"g\":" + fmtFloat(dd.gradientColor1[1]) +
            ",\"b\":" + fmtFloat(dd.gradientColor1[2]) +
            ",\"a\":" + fmtFloat(dd.gradientColor1[3]) + "}" +
            ",\"center\":{\"x\":" + fmtFloat(dd.gradientCenter[0]) +
            ",\"y\":" + fmtFloat(dd.gradientCenter[1]) + "}" +
            ",\"radius\":" + fmtFloat(dd.gradientRadius) + "}";
      emit(cmd, r);
    }
  }
}

void SceneReconciler::deleteRemovedDrawItems(const SceneDocument& newDoc, const Scene& scene, ReconcileResult& r) {
  for (Id id : scene.drawItemIds()) {
    if (newDoc.drawItems.count(id) == 0) {
      std::string cmd = "{\"cmd\":\"delete\",\"id\":" + std::to_string(id) + "}";
      if (emit(cmd, r)) r.deleted++;
    }
  }
}

} // namespace dc
