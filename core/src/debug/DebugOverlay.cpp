#include "dc/debug/DebugOverlay.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <functional>

namespace dc {

// Helper: emit a JSON command string via RapidJSON writer.
static std::string makeCmd(const std::function<void(rapidjson::Writer<rapidjson::StringBuffer>&)>& fn) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);
  w.StartObject();
  fn(w);
  w.EndObject();
  return sb.GetString();
}

std::vector<std::string> DebugOverlay::generateCommands(
    const Scene& scene,
    const DebugOverlayConfig& config,
    int /*viewW*/, int /*viewH*/) {

  // First dispose any previous debug geometry.
  auto cleanup = disposeCommands();

  std::vector<std::string> cmds;
  cmds.insert(cmds.end(), cleanup.begin(), cleanup.end());

  nextOffset_ = 0;
  createdIds_.clear();

  std::uint32_t base = config.debugIdBase;

  // Allocate debug pane and layer.
  std::uint32_t debugPaneId = base + nextOffset_++;
  std::uint32_t debugLayerId = base + nextOffset_++;

  createdIds_.push_back(debugPaneId);
  createdIds_.push_back(debugLayerId);

  cmds.push_back(makeCmd([&](auto& w) {
    w.Key("cmd"); w.String("createPane");
    w.Key("id"); w.Uint(debugPaneId);
    w.Key("name"); w.String("__debug__");
  }));

  cmds.push_back(makeCmd([&](auto& w) {
    w.Key("cmd"); w.String("createLayer");
    w.Key("id"); w.Uint(debugLayerId);
    w.Key("paneId"); w.Uint(debugPaneId);
  }));

  // Helper: create a line2d wireframe for an AABB (4 lines = 8 vertices in clip space).
  auto emitWireframe = [&](float xMin, float yMin, float xMax, float yMax,
                           const float color[4]) {
    // 8 vertices (4 line segments): bottom, right, top, left
    float verts[16] = {
      xMin, yMin,  xMax, yMin,  // bottom
      xMax, yMin,  xMax, yMax,  // right
      xMax, yMax,  xMin, yMax,  // top
      xMin, yMax,  xMin, yMin   // left
    };

    std::uint32_t bufId = base + nextOffset_++;
    std::uint32_t geomId = base + nextOffset_++;
    std::uint32_t diId = base + nextOffset_++;

    createdIds_.push_back(bufId);
    createdIds_.push_back(geomId);
    createdIds_.push_back(diId);

    // createBuffer
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("createBuffer");
      w.Key("id"); w.Uint(bufId);
      w.Key("byteLength"); w.Uint(sizeof(verts));
    }));

    // createGeometry
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("createGeometry");
      w.Key("id"); w.Uint(geomId);
      w.Key("vertexBufferId"); w.Uint(bufId);
      w.Key("vertexCount"); w.Uint(8);
      w.Key("format"); w.String("pos2_clip");
    }));

    // createDrawItem
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("createDrawItem");
      w.Key("id"); w.Uint(diId);
      w.Key("layerId"); w.Uint(debugLayerId);
    }));

    // bindDrawItem
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("bindDrawItem");
      w.Key("drawItemId"); w.Uint(diId);
      w.Key("pipeline"); w.String("line2d@1");
      w.Key("geometryId"); w.Uint(geomId);
    }));

    // setDrawItemColor
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("setDrawItemColor");
      w.Key("drawItemId"); w.Uint(diId);
      w.Key("r"); w.Double(color[0]);
      w.Key("g"); w.Double(color[1]);
      w.Key("b"); w.Double(color[2]);
      w.Key("a"); w.Double(color[3]);
    }));
  };

  // Show pane region outlines.
  if (config.showPaneRegions) {
    auto paneIds = scene.paneIds();
    for (Id pid : paneIds) {
      const Pane* pane = scene.getPane(pid);
      if (!pane) continue;
      const auto& r = pane->region;
      emitWireframe(r.clipXMin, r.clipYMin, r.clipXMax, r.clipYMax,
                    config.paneColor);
    }
  }

  // Show geometry bounds (AABB wireframes for drawItems with boundsValid geometry).
  if (config.showBounds) {
    auto diIds = scene.drawItemIds();
    for (Id did : diIds) {
      const DrawItem* di = scene.getDrawItem(did);
      if (!di || di->geometryId == 0) continue;
      const Geometry* geom = scene.getGeometry(di->geometryId);
      if (!geom || !geom->boundsValid) continue;

      float xMin = geom->boundsMin[0];
      float yMin = geom->boundsMin[1];
      float xMax = geom->boundsMax[0];
      float yMax = geom->boundsMax[1];

      // Apply transform if present.
      if (di->transformId != 0) {
        const Transform* t = scene.getTransform(di->transformId);
        if (t) {
          float sx = t->params.sx;
          float sy = t->params.sy;
          float tx = t->params.tx;
          float ty = t->params.ty;
          xMin = xMin * sx + tx;
          xMax = xMax * sx + tx;
          yMin = yMin * sy + ty;
          yMax = yMax * sy + ty;
          // Ensure min/max ordering after transform
          if (xMin > xMax) std::swap(xMin, xMax);
          if (yMin > yMax) std::swap(yMin, yMax);
        }
      }

      emitWireframe(xMin, yMin, xMax, yMax, config.boundsColor);
    }
  }

  // Show transform axes (origin cross).
  if (config.showTransformAxes) {
    auto tIds = scene.transformIds();
    for (Id tid : tIds) {
      const Transform* t = scene.getTransform(tid);
      if (!t) continue;
      float cx = t->params.tx;
      float cy = t->params.ty;
      float len = 0.05f; // small cross in clip space

      // Horizontal line
      float hVerts[4] = {cx - len, cy, cx + len, cy};
      // Vertical line
      float vVerts[4] = {cx, cy - len, cx, cy + len};

      // Emit two line segments (each is 2 vertices for line2d).
      // Horizontal
      {
        std::uint32_t bufId = base + nextOffset_++;
        std::uint32_t geomId = base + nextOffset_++;
        std::uint32_t diId = base + nextOffset_++;
        createdIds_.push_back(bufId);
        createdIds_.push_back(geomId);
        createdIds_.push_back(diId);
        (void)hVerts;

        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createBuffer");
          w.Key("id"); w.Uint(bufId);
          w.Key("byteLength"); w.Uint(static_cast<unsigned>(sizeof(hVerts)));
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createGeometry");
          w.Key("id"); w.Uint(geomId);
          w.Key("vertexBufferId"); w.Uint(bufId);
          w.Key("vertexCount"); w.Uint(2);
          w.Key("format"); w.String("pos2_clip");
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createDrawItem");
          w.Key("id"); w.Uint(diId);
          w.Key("layerId"); w.Uint(debugLayerId);
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("bindDrawItem");
          w.Key("drawItemId"); w.Uint(diId);
          w.Key("pipeline"); w.String("line2d@1");
          w.Key("geometryId"); w.Uint(geomId);
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("setDrawItemColor");
          w.Key("drawItemId"); w.Uint(diId);
          w.Key("r"); w.Double(config.axisColor[0]);
          w.Key("g"); w.Double(config.axisColor[1]);
          w.Key("b"); w.Double(config.axisColor[2]);
          w.Key("a"); w.Double(config.axisColor[3]);
        }));
      }

      // Vertical
      {
        std::uint32_t bufId = base + nextOffset_++;
        std::uint32_t geomId = base + nextOffset_++;
        std::uint32_t diId = base + nextOffset_++;
        createdIds_.push_back(bufId);
        createdIds_.push_back(geomId);
        createdIds_.push_back(diId);
        (void)vVerts;

        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createBuffer");
          w.Key("id"); w.Uint(bufId);
          w.Key("byteLength"); w.Uint(static_cast<unsigned>(sizeof(vVerts)));
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createGeometry");
          w.Key("id"); w.Uint(geomId);
          w.Key("vertexBufferId"); w.Uint(bufId);
          w.Key("vertexCount"); w.Uint(2);
          w.Key("format"); w.String("pos2_clip");
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("createDrawItem");
          w.Key("id"); w.Uint(diId);
          w.Key("layerId"); w.Uint(debugLayerId);
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("bindDrawItem");
          w.Key("drawItemId"); w.Uint(diId);
          w.Key("pipeline"); w.String("line2d@1");
          w.Key("geometryId"); w.Uint(geomId);
        }));
        cmds.push_back(makeCmd([&](auto& w) {
          w.Key("cmd"); w.String("setDrawItemColor");
          w.Key("drawItemId"); w.Uint(diId);
          w.Key("r"); w.Double(config.axisColor[0]);
          w.Key("g"); w.Double(config.axisColor[1]);
          w.Key("b"); w.Double(config.axisColor[2]);
          w.Key("a"); w.Double(config.axisColor[3]);
        }));
      }
    }
  }

  return cmds;
}

std::vector<std::string> DebugOverlay::disposeCommands() {
  std::vector<std::string> cmds;
  // Delete in reverse order to clean up children before parents.
  for (auto it = createdIds_.rbegin(); it != createdIds_.rend(); ++it) {
    cmds.push_back(makeCmd([&](auto& w) {
      w.Key("cmd"); w.String("delete");
      w.Key("id"); w.Uint64(*it);
    }));
  }
  createdIds_.clear();
  nextOffset_ = 0;
  return cmds;
}

std::size_t DebugOverlay::debugItemCount() const {
  return createdIds_.size();
}

} // namespace dc
