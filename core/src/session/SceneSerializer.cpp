// D45: Scene serialization / deserialization
#include "dc/session/SceneSerializer.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace dc {

// ---- helpers for BlendMode <-> string ----

static const char* blendModeToString(BlendMode bm) {
  switch (bm) {
    case BlendMode::Additive: return "additive";
    case BlendMode::Multiply: return "multiply";
    case BlendMode::Screen:   return "screen";
    default:                  return "normal";
  }
}

static const char* anchorPointToString(std::uint8_t ap) {
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
    default: return "topLeft";
  }
}

// ---- formatFloat: %.9g as a string ----
static std::string fmtFloat(float v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
  return buf;
}

// ---- Serialization ----

std::string serializeScene(const Scene& scene) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();

  // -- buffers --
  w.Key("buffers");
  w.StartArray();
  {
    auto ids = scene.bufferIds();
    std::sort(ids.begin(), ids.end());
    for (Id id : ids) {
      const Buffer* b = scene.getBuffer(id);
      if (!b) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(b->id);
      w.Key("byteLength"); w.Uint(b->byteLength);
      w.EndObject();
    }
  }
  w.EndArray();

  // -- transforms --
  w.Key("transforms");
  w.StartArray();
  {
    auto ids = scene.transformIds();
    std::sort(ids.begin(), ids.end());
    for (Id id : ids) {
      const Transform* t = scene.getTransform(id);
      if (!t) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(t->id);
      w.Key("tx"); w.Double(static_cast<double>(t->params.tx));
      w.Key("ty"); w.Double(static_cast<double>(t->params.ty));
      w.Key("sx"); w.Double(static_cast<double>(t->params.sx));
      w.Key("sy"); w.Double(static_cast<double>(t->params.sy));
      w.EndObject();
    }
  }
  w.EndArray();

  // -- panes --
  w.Key("panes");
  w.StartArray();
  {
    auto ids = scene.paneIds();
    for (Id id : ids) {
      const Pane* p = scene.getPane(id);
      if (!p) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(p->id);
      w.Key("name"); w.String(p->name.c_str());
      w.Key("region");
      w.StartObject();
        w.Key("clipYMin"); w.Double(static_cast<double>(p->region.clipYMin));
        w.Key("clipYMax"); w.Double(static_cast<double>(p->region.clipYMax));
        w.Key("clipXMin"); w.Double(static_cast<double>(p->region.clipXMin));
        w.Key("clipXMax"); w.Double(static_cast<double>(p->region.clipXMax));
      w.EndObject();
      w.Key("clearColor");
      w.StartArray();
        w.Double(static_cast<double>(p->clearColor[0]));
        w.Double(static_cast<double>(p->clearColor[1]));
        w.Double(static_cast<double>(p->clearColor[2]));
        w.Double(static_cast<double>(p->clearColor[3]));
      w.EndArray();
      w.Key("hasClearColor"); w.Bool(p->hasClearColor);
      w.EndObject();
    }
  }
  w.EndArray();

  // -- layers --
  w.Key("layers");
  w.StartArray();
  {
    auto ids = scene.layerIds();
    for (Id id : ids) {
      const Layer* l = scene.getLayer(id);
      if (!l) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(l->id);
      w.Key("paneId"); w.Uint64(l->paneId);
      w.Key("name"); w.String(l->name.c_str());
      w.EndObject();
    }
  }
  w.EndArray();

  // -- geometries --
  w.Key("geometries");
  w.StartArray();
  {
    auto ids = scene.geometryIds();
    std::sort(ids.begin(), ids.end());
    for (Id id : ids) {
      const Geometry* g = scene.getGeometry(id);
      if (!g) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(g->id);
      w.Key("vertexBufferId"); w.Uint64(g->vertexBufferId);
      w.Key("format"); w.String(toString(g->format));
      w.Key("vertexCount"); w.Uint(g->vertexCount);
      w.Key("indexBufferId"); w.Uint64(g->indexBufferId);
      w.Key("indexCount"); w.Uint(g->indexCount);
      w.Key("boundsMin");
      w.StartArray();
        w.Double(static_cast<double>(g->boundsMin[0]));
        w.Double(static_cast<double>(g->boundsMin[1]));
      w.EndArray();
      w.Key("boundsMax");
      w.StartArray();
        w.Double(static_cast<double>(g->boundsMax[0]));
        w.Double(static_cast<double>(g->boundsMax[1]));
      w.EndArray();
      w.Key("boundsValid"); w.Bool(g->boundsValid);
      w.EndObject();
    }
  }
  w.EndArray();

  // -- drawItems --
  w.Key("drawItems");
  w.StartArray();
  {
    auto ids = scene.drawItemIds();
    for (Id id : ids) {
      const DrawItem* di = scene.getDrawItem(id);
      if (!di) continue;
      w.StartObject();
      w.Key("id"); w.Uint64(di->id);
      w.Key("layerId"); w.Uint64(di->layerId);
      w.Key("name"); w.String(di->name.c_str());
      w.Key("pipeline"); w.String(di->pipeline.c_str());
      w.Key("geometryId"); w.Uint64(di->geometryId);
      w.Key("transformId"); w.Uint64(di->transformId);

      w.Key("color");
      w.StartArray();
        for (int i = 0; i < 4; i++) w.Double(static_cast<double>(di->color[i]));
      w.EndArray();

      w.Key("colorUp");
      w.StartArray();
        for (int i = 0; i < 4; i++) w.Double(static_cast<double>(di->colorUp[i]));
      w.EndArray();

      w.Key("colorDown");
      w.StartArray();
        for (int i = 0; i < 4; i++) w.Double(static_cast<double>(di->colorDown[i]));
      w.EndArray();

      w.Key("pointSize"); w.Double(static_cast<double>(di->pointSize));
      w.Key("lineWidth"); w.Double(static_cast<double>(di->lineWidth));
      w.Key("dashLength"); w.Double(static_cast<double>(di->dashLength));
      w.Key("gapLength"); w.Double(static_cast<double>(di->gapLength));
      w.Key("cornerRadius"); w.Double(static_cast<double>(di->cornerRadius));

      w.Key("blendMode"); w.String(blendModeToString(di->blendMode));
      w.Key("isClipSource"); w.Bool(di->isClipSource);
      w.Key("useClipMask"); w.Bool(di->useClipMask);

      w.Key("textureId"); w.Uint(di->textureId);

      w.Key("anchorPoint"); w.Uint(di->anchorPoint);
      w.Key("anchorOffsetX"); w.Double(static_cast<double>(di->anchorOffsetX));
      w.Key("anchorOffsetY"); w.Double(static_cast<double>(di->anchorOffsetY));
      w.Key("hasAnchor"); w.Bool(di->hasAnchor);

      w.Key("visible"); w.Bool(di->visible);

      // D46: gradient fill
      w.Key("gradientType"); w.Uint(di->gradientType);
      w.Key("gradientAngle"); w.Double(static_cast<double>(di->gradientAngle));
      w.Key("gradientColor0");
      w.StartArray();
        for (int i = 0; i < 4; i++) w.Double(static_cast<double>(di->gradientColor0[i]));
      w.EndArray();
      w.Key("gradientColor1");
      w.StartArray();
        for (int i = 0; i < 4; i++) w.Double(static_cast<double>(di->gradientColor1[i]));
      w.EndArray();
      w.Key("gradientCenter");
      w.StartArray();
        w.Double(static_cast<double>(di->gradientCenter[0]));
        w.Double(static_cast<double>(di->gradientCenter[1]));
      w.EndArray();
      w.Key("gradientRadius"); w.Double(static_cast<double>(di->gradientRadius));
      w.EndObject();
    }
  }
  w.EndArray();

  w.EndObject();
  return sb.GetString();
}

// ---- Deserialization helpers ----

static std::string escapeJson(const std::string& s) {
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

bool deserializeScene(const std::string& json, Scene& /*scene*/, CommandProcessor& cp) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }

  // 1. Buffers
  if (doc.HasMember("buffers") && doc["buffers"].IsArray()) {
    for (auto& v : doc["buffers"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      std::uint32_t bl = v.HasMember("byteLength") ? v["byteLength"].GetUint() : 0;
      std::string cmd = "{\"cmd\":\"createBuffer\",\"id\":" + std::to_string(id) +
                         ",\"byteLength\":" + std::to_string(bl) + "}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;
    }
  }

  // 2. Transforms
  if (doc.HasMember("transforms") && doc["transforms"].IsArray()) {
    for (auto& v : doc["transforms"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      double tx = v.HasMember("tx") ? v["tx"].GetDouble() : 0.0;
      double ty = v.HasMember("ty") ? v["ty"].GetDouble() : 0.0;
      double sx = v.HasMember("sx") ? v["sx"].GetDouble() : 1.0;
      double sy = v.HasMember("sy") ? v["sy"].GetDouble() : 1.0;

      // Create
      std::string cmd = "{\"cmd\":\"createTransform\",\"id\":" + std::to_string(id) + "}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;

      // Set params
      cmd = "{\"cmd\":\"setTransform\",\"id\":" + std::to_string(id) +
            ",\"tx\":" + fmtFloat(static_cast<float>(tx)) +
            ",\"ty\":" + fmtFloat(static_cast<float>(ty)) +
            ",\"sx\":" + fmtFloat(static_cast<float>(sx)) +
            ",\"sy\":" + fmtFloat(static_cast<float>(sy)) + "}";
      r = cp.applyJsonText(cmd);
      if (!r.ok) return false;
    }
  }

  // 3. Panes
  if (doc.HasMember("panes") && doc["panes"].IsArray()) {
    for (auto& v : doc["panes"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      std::string name = v.HasMember("name") ? v["name"].GetString() : "";

      std::string cmd = "{\"cmd\":\"createPane\",\"id\":" + std::to_string(id) +
                         ",\"name\":\"" + escapeJson(name) + "\"}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;

      // Set region
      if (v.HasMember("region") && v["region"].IsObject()) {
        const auto& reg = v["region"];
        double yMin = reg.HasMember("clipYMin") ? reg["clipYMin"].GetDouble() : -1.0;
        double yMax = reg.HasMember("clipYMax") ? reg["clipYMax"].GetDouble() : 1.0;
        double xMin = reg.HasMember("clipXMin") ? reg["clipXMin"].GetDouble() : -1.0;
        double xMax = reg.HasMember("clipXMax") ? reg["clipXMax"].GetDouble() : 1.0;

        cmd = "{\"cmd\":\"setPaneRegion\",\"id\":" + std::to_string(id) +
              ",\"clipYMin\":" + fmtFloat(static_cast<float>(yMin)) +
              ",\"clipYMax\":" + fmtFloat(static_cast<float>(yMax)) +
              ",\"clipXMin\":" + fmtFloat(static_cast<float>(xMin)) +
              ",\"clipXMax\":" + fmtFloat(static_cast<float>(xMax)) + "}";
        r = cp.applyJsonText(cmd);
        if (!r.ok) return false;
      }

      // Set clear color
      bool hasClear = v.HasMember("hasClearColor") && v["hasClearColor"].GetBool();
      if (hasClear && v.HasMember("clearColor") && v["clearColor"].IsArray()) {
        const auto& cc = v["clearColor"].GetArray();
        if (cc.Size() >= 4) {
          cmd = "{\"cmd\":\"setPaneClearColor\",\"id\":" + std::to_string(id) +
                ",\"r\":" + fmtFloat(static_cast<float>(cc[0].GetDouble())) +
                ",\"g\":" + fmtFloat(static_cast<float>(cc[1].GetDouble())) +
                ",\"b\":" + fmtFloat(static_cast<float>(cc[2].GetDouble())) +
                ",\"a\":" + fmtFloat(static_cast<float>(cc[3].GetDouble())) + "}";
          r = cp.applyJsonText(cmd);
          if (!r.ok) return false;
        }
      }
    }
  }

  // 4. Layers
  if (doc.HasMember("layers") && doc["layers"].IsArray()) {
    for (auto& v : doc["layers"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      std::uint64_t paneId = v.HasMember("paneId") ? v["paneId"].GetUint64() : 0;
      std::string name = v.HasMember("name") ? v["name"].GetString() : "";

      std::string cmd = "{\"cmd\":\"createLayer\",\"id\":" + std::to_string(id) +
                         ",\"paneId\":" + std::to_string(paneId) +
                         ",\"name\":\"" + escapeJson(name) + "\"}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;
    }
  }

  // 5. Geometries
  if (doc.HasMember("geometries") && doc["geometries"].IsArray()) {
    for (auto& v : doc["geometries"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      std::uint64_t vbId = v.HasMember("vertexBufferId") ? v["vertexBufferId"].GetUint64() : 0;
      std::string fmt = v.HasMember("format") ? v["format"].GetString() : "pos2_clip";
      std::uint32_t vc = v.HasMember("vertexCount") ? v["vertexCount"].GetUint() : 1;

      // Ensure vertexCount >= 1 for createGeometry validation
      if (vc < 1) vc = 1;

      std::string cmd = "{\"cmd\":\"createGeometry\",\"id\":" + std::to_string(id) +
                         ",\"vertexBufferId\":" + std::to_string(vbId) +
                         ",\"format\":\"" + fmt + "\"" +
                         ",\"vertexCount\":" + std::to_string(vc);

      // Index buffer
      std::uint64_t ibId = v.HasMember("indexBufferId") ? v["indexBufferId"].GetUint64() : 0;
      if (ibId != 0) {
        cmd += ",\"indexBufferId\":" + std::to_string(ibId);
      }
      std::uint32_t ic = v.HasMember("indexCount") ? v["indexCount"].GetUint() : 0;
      if (ic != 0) {
        cmd += ",\"indexCount\":" + std::to_string(ic);
      }

      cmd += "}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;

      // Set bounds if valid
      bool boundsValid = v.HasMember("boundsValid") && v["boundsValid"].GetBool();
      if (boundsValid && v.HasMember("boundsMin") && v.HasMember("boundsMax")) {
        const auto& bMin = v["boundsMin"].GetArray();
        const auto& bMax = v["boundsMax"].GetArray();
        if (bMin.Size() >= 2 && bMax.Size() >= 2) {
          cmd = "{\"cmd\":\"setGeometryBounds\",\"geometryId\":" + std::to_string(id) +
                ",\"minX\":" + fmtFloat(static_cast<float>(bMin[0].GetDouble())) +
                ",\"minY\":" + fmtFloat(static_cast<float>(bMin[1].GetDouble())) +
                ",\"maxX\":" + fmtFloat(static_cast<float>(bMax[0].GetDouble())) +
                ",\"maxY\":" + fmtFloat(static_cast<float>(bMax[1].GetDouble())) + "}";
          r = cp.applyJsonText(cmd);
          if (!r.ok) return false;
        }
      }
    }
  }

  // 6. DrawItems
  if (doc.HasMember("drawItems") && doc["drawItems"].IsArray()) {
    for (auto& v : doc["drawItems"].GetArray()) {
      if (!v.IsObject()) continue;
      std::uint64_t id = v.HasMember("id") ? v["id"].GetUint64() : 0;
      std::uint64_t layerId = v.HasMember("layerId") ? v["layerId"].GetUint64() : 0;
      std::string name = v.HasMember("name") ? v["name"].GetString() : "";

      // Create
      std::string cmd = "{\"cmd\":\"createDrawItem\",\"id\":" + std::to_string(id) +
                         ",\"layerId\":" + std::to_string(layerId) +
                         ",\"name\":\"" + escapeJson(name) + "\"}";
      auto r = cp.applyJsonText(cmd);
      if (!r.ok) return false;

      // Bind pipeline + geometry
      std::string pipeline = v.HasMember("pipeline") ? v["pipeline"].GetString() : "";
      std::uint64_t geomId = v.HasMember("geometryId") ? v["geometryId"].GetUint64() : 0;
      if (!pipeline.empty() && geomId != 0) {
        cmd = "{\"cmd\":\"bindDrawItem\",\"drawItemId\":" + std::to_string(id) +
              ",\"pipeline\":\"" + pipeline + "\"" +
              ",\"geometryId\":" + std::to_string(geomId) + "}";
        r = cp.applyJsonText(cmd);
        if (!r.ok) return false;
      }

      // Attach transform
      std::uint64_t txId = v.HasMember("transformId") ? v["transformId"].GetUint64() : 0;
      if (txId != 0) {
        cmd = "{\"cmd\":\"attachTransform\",\"drawItemId\":" + std::to_string(id) +
              ",\"transformId\":" + std::to_string(txId) + "}";
        r = cp.applyJsonText(cmd);
        if (!r.ok) return false;
      }

      // Set style (colors, blend mode, clipping, etc.)
      {
        std::ostringstream ss;
        ss << "{\"cmd\":\"setDrawItemStyle\",\"drawItemId\":" << id;

        // color
        if (v.HasMember("color") && v["color"].IsArray()) {
          const auto& c = v["color"].GetArray();
          if (c.Size() >= 4) {
            ss << ",\"r\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
               << ",\"g\":" << fmtFloat(static_cast<float>(c[1].GetDouble()))
               << ",\"b\":" << fmtFloat(static_cast<float>(c[2].GetDouble()))
               << ",\"a\":" << fmtFloat(static_cast<float>(c[3].GetDouble()));
          }
        }

        // colorUp
        if (v.HasMember("colorUp") && v["colorUp"].IsArray()) {
          const auto& c = v["colorUp"].GetArray();
          if (c.Size() >= 4) {
            ss << ",\"colorUpR\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
               << ",\"colorUpG\":" << fmtFloat(static_cast<float>(c[1].GetDouble()))
               << ",\"colorUpB\":" << fmtFloat(static_cast<float>(c[2].GetDouble()))
               << ",\"colorUpA\":" << fmtFloat(static_cast<float>(c[3].GetDouble()));
          }
        }

        // colorDown
        if (v.HasMember("colorDown") && v["colorDown"].IsArray()) {
          const auto& c = v["colorDown"].GetArray();
          if (c.Size() >= 4) {
            ss << ",\"colorDownR\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
               << ",\"colorDownG\":" << fmtFloat(static_cast<float>(c[1].GetDouble()))
               << ",\"colorDownB\":" << fmtFloat(static_cast<float>(c[2].GetDouble()))
               << ",\"colorDownA\":" << fmtFloat(static_cast<float>(c[3].GetDouble()));
          }
        }

        ss << ",\"pointSize\":" << fmtFloat(static_cast<float>(v.HasMember("pointSize") ? v["pointSize"].GetDouble() : 4.0));
        ss << ",\"lineWidth\":" << fmtFloat(static_cast<float>(v.HasMember("lineWidth") ? v["lineWidth"].GetDouble() : 1.0));
        ss << ",\"dashLength\":" << fmtFloat(static_cast<float>(v.HasMember("dashLength") ? v["dashLength"].GetDouble() : 0.0));
        ss << ",\"gapLength\":" << fmtFloat(static_cast<float>(v.HasMember("gapLength") ? v["gapLength"].GetDouble() : 0.0));
        ss << ",\"cornerRadius\":" << fmtFloat(static_cast<float>(v.HasMember("cornerRadius") ? v["cornerRadius"].GetDouble() : 0.0));

        // blendMode
        std::string bm = v.HasMember("blendMode") ? v["blendMode"].GetString() : "normal";
        ss << ",\"blendMode\":\"" << bm << "\"";

        // clipping
        bool isClip = v.HasMember("isClipSource") && v["isClipSource"].GetBool();
        bool useClip = v.HasMember("useClipMask") && v["useClipMask"].GetBool();
        ss << ",\"isClipSource\":" << (isClip ? "true" : "false");
        ss << ",\"useClipMask\":" << (useClip ? "true" : "false");

        ss << "}";
        r = cp.applyJsonText(ss.str());
        if (!r.ok) return false;
      }

      // Visibility
      {
        bool visible = !v.HasMember("visible") || v["visible"].GetBool();
        cmd = "{\"cmd\":\"setDrawItemVisible\",\"drawItemId\":" + std::to_string(id) +
              ",\"visible\":" + (visible ? "true" : "false") + "}";
        r = cp.applyJsonText(cmd);
        if (!r.ok) return false;
      }

      // Texture
      {
        std::uint32_t texId = v.HasMember("textureId") ? v["textureId"].GetUint() : 0;
        if (texId != 0) {
          cmd = "{\"cmd\":\"setDrawItemTexture\",\"drawItemId\":" + std::to_string(id) +
                ",\"textureId\":" + std::to_string(texId) + "}";
          r = cp.applyJsonText(cmd);
          if (!r.ok) return false;
        }
      }

      // D46: gradient fill
      {
        std::uint8_t gt = v.HasMember("gradientType") ? static_cast<std::uint8_t>(v["gradientType"].GetUint()) : 0;
        if (gt != 0) {
          std::ostringstream gs;
          gs << "{\"cmd\":\"setDrawItemGradient\",\"drawItemId\":" << id;

          if (gt == 1) gs << ",\"type\":\"linear\"";
          else if (gt == 2) gs << ",\"type\":\"radial\"";

          gs << ",\"angle\":" << fmtFloat(static_cast<float>(v.HasMember("gradientAngle") ? v["gradientAngle"].GetDouble() : 0.0));

          if (v.HasMember("gradientColor0") && v["gradientColor0"].IsArray()) {
            const auto& c = v["gradientColor0"].GetArray();
            if (c.Size() >= 4) {
              gs << ",\"color0\":{\"r\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
                 << ",\"g\":" << fmtFloat(static_cast<float>(c[1].GetDouble()))
                 << ",\"b\":" << fmtFloat(static_cast<float>(c[2].GetDouble()))
                 << ",\"a\":" << fmtFloat(static_cast<float>(c[3].GetDouble())) << "}";
            }
          }

          if (v.HasMember("gradientColor1") && v["gradientColor1"].IsArray()) {
            const auto& c = v["gradientColor1"].GetArray();
            if (c.Size() >= 4) {
              gs << ",\"color1\":{\"r\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
                 << ",\"g\":" << fmtFloat(static_cast<float>(c[1].GetDouble()))
                 << ",\"b\":" << fmtFloat(static_cast<float>(c[2].GetDouble()))
                 << ",\"a\":" << fmtFloat(static_cast<float>(c[3].GetDouble())) << "}";
            }
          }

          if (v.HasMember("gradientCenter") && v["gradientCenter"].IsArray()) {
            const auto& c = v["gradientCenter"].GetArray();
            if (c.Size() >= 2) {
              gs << ",\"center\":{\"x\":" << fmtFloat(static_cast<float>(c[0].GetDouble()))
                 << ",\"y\":" << fmtFloat(static_cast<float>(c[1].GetDouble())) << "}";
            }
          }

          gs << ",\"radius\":" << fmtFloat(static_cast<float>(v.HasMember("gradientRadius") ? v["gradientRadius"].GetDouble() : 0.5));
          gs << "}";
          r = cp.applyJsonText(gs.str());
          if (!r.ok) return false;
        }
      }

      // Anchor
      {
        bool hasAnchor = v.HasMember("hasAnchor") && v["hasAnchor"].GetBool();
        if (hasAnchor) {
          std::uint8_t ap = v.HasMember("anchorPoint") ? static_cast<std::uint8_t>(v["anchorPoint"].GetUint()) : 0;
          float ox = v.HasMember("anchorOffsetX") ? static_cast<float>(v["anchorOffsetX"].GetDouble()) : 0.0f;
          float oy = v.HasMember("anchorOffsetY") ? static_cast<float>(v["anchorOffsetY"].GetDouble()) : 0.0f;

          cmd = "{\"cmd\":\"setDrawItemAnchor\",\"drawItemId\":" + std::to_string(id) +
                ",\"anchor\":\"" + anchorPointToString(ap) + "\"" +
                ",\"offsetX\":" + fmtFloat(ox) +
                ",\"offsetY\":" + fmtFloat(oy) + "}";
          r = cp.applyJsonText(cmd);
          if (!r.ok) return false;
        }
      }
    }
  }

  return true;
}

} // namespace dc
