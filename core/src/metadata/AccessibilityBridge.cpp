#include "dc/metadata/AccessibilityBridge.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>

namespace dc {

std::vector<AccessibleNode> AccessibilityBridge::buildTree(
    const Scene& scene,
    const AnnotationStore& annotations,
    const AccessibilityConfig& config) {

  std::vector<AccessibleNode> roots;

  auto paneIds = scene.paneIds();
  for (Id pid : paneIds) {
    const Pane* pane = scene.getPane(pid);
    if (!pane) continue;

    AccessibleNode paneNode;
    paneNode.id = pid;
    paneNode.role = "group";
    paneNode.name = pane->name.empty() ? ("Pane " + std::to_string(pid)) : pane->name;

    // Compute pane bounding box from region in pixel space.
    const auto& r = pane->region;
    float x = (r.clipXMin + 1.0f) * 0.5f * static_cast<float>(config.viewW);
    float y = (1.0f - r.clipYMax) * 0.5f * static_cast<float>(config.viewH);
    float w = (r.clipXMax - r.clipXMin) * 0.5f * static_cast<float>(config.viewW);
    float h = (r.clipYMax - r.clipYMin) * 0.5f * static_cast<float>(config.viewH);
    paneNode.boundingBox[0] = x;
    paneNode.boundingBox[1] = y;
    paneNode.boundingBox[2] = w;
    paneNode.boundingBox[3] = h;

    // Collect all draw items in this pane (through layers).
    auto layerIds = scene.layerIds();
    for (Id lid : layerIds) {
      const Layer* layer = scene.getLayer(lid);
      if (!layer || layer->paneId != pid) continue;

      auto drawItemIds = scene.drawItemIds();
      // Sort to ensure deterministic ordering.
      std::sort(drawItemIds.begin(), drawItemIds.end());

      for (Id did : drawItemIds) {
        const DrawItem* di = scene.getDrawItem(did);
        if (!di || di->layerId != lid) continue;

        const Annotation* ann = annotations.get(did);
        if (!ann && !config.includeUnannotated) continue;

        AccessibleNode child;
        child.id = did;

        if (ann) {
          child.role = ann->role;
          child.name = ann->label;
          child.value = ann->value;
        } else {
          child.role = config.defaultRole;
          child.name = di->name.empty() ? ("DrawItem " + std::to_string(did)) : di->name;
        }

        // Compute bounding box from geometry bounds if available.
        if (di->geometryId != 0) {
          const Geometry* geom = scene.getGeometry(di->geometryId);
          if (geom && geom->boundsValid) {
            float gxMin = geom->boundsMin[0];
            float gyMin = geom->boundsMin[1];
            float gxMax = geom->boundsMax[0];
            float gyMax = geom->boundsMax[1];

            // Apply transform if present.
            if (di->transformId != 0) {
              const Transform* t = scene.getTransform(di->transformId);
              if (t) {
                float sx = t->params.sx;
                float sy = t->params.sy;
                float tx = t->params.tx;
                float ty = t->params.ty;
                gxMin = gxMin * sx + tx;
                gxMax = gxMax * sx + tx;
                gyMin = gyMin * sy + ty;
                gyMax = gyMax * sy + ty;
                if (gxMin > gxMax) std::swap(gxMin, gxMax);
                if (gyMin > gyMax) std::swap(gyMin, gyMax);
              }
            }

            // Convert clip space to pixel space.
            child.boundingBox[0] = (gxMin + 1.0f) * 0.5f * static_cast<float>(config.viewW);
            child.boundingBox[1] = (1.0f - gyMax) * 0.5f * static_cast<float>(config.viewH);
            child.boundingBox[2] = (gxMax - gxMin) * 0.5f * static_cast<float>(config.viewW);
            child.boundingBox[3] = (gyMax - gyMin) * 0.5f * static_cast<float>(config.viewH);
          }
        }

        paneNode.children.push_back(std::move(child));
      }
    }

    roots.push_back(std::move(paneNode));
  }

  return roots;
}

// Recursive JSON serialization helper.
static void writeNode(rapidjson::Writer<rapidjson::StringBuffer>& w,
                      const AccessibleNode& node) {
  w.StartObject();
  w.Key("id"); w.Uint64(node.id);
  w.Key("role"); w.String(node.role.c_str());
  w.Key("name"); w.String(node.name.c_str());
  w.Key("value"); w.String(node.value.c_str());
  w.Key("boundingBox");
  w.StartArray();
  w.Double(node.boundingBox[0]);
  w.Double(node.boundingBox[1]);
  w.Double(node.boundingBox[2]);
  w.Double(node.boundingBox[3]);
  w.EndArray();
  if (!node.children.empty()) {
    w.Key("children");
    w.StartArray();
    for (const auto& child : node.children) {
      writeNode(w, child);
    }
    w.EndArray();
  }
  w.EndObject();
}

std::string AccessibilityBridge::toJSON(const std::vector<AccessibleNode>& tree) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);
  w.StartArray();
  for (const auto& node : tree) {
    writeNode(w, node);
  }
  w.EndArray();
  return sb.GetString();
}

} // namespace dc
