// D77: SceneDocument — declarative JSON scene parsing & serialization
#include "dc/document/SceneDocument.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace dc {

// --- Helpers ---

static Id parseIdKey(const char* s) {
  Id v = 0;
  for (const char* p = s; *p; ++p) {
    if (*p < '0' || *p > '9') return 0;
    v = v * 10 + static_cast<Id>(*p - '0');
  }
  return v;
}

static float getFloat(const rapidjson::Value& obj, const char* key, float def) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return def;
  const auto& v = it->value;
  if (v.IsFloat() || v.IsDouble()) return static_cast<float>(v.GetDouble());
  if (v.IsInt()) return static_cast<float>(v.GetInt());
  return def;
}

static std::string getString(const rapidjson::Value& obj, const char* key, const char* def = "") {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsString()) return def;
  return it->value.GetString();
}

static Id getId(const rapidjson::Value& obj, const char* key) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return 0;
  const auto& v = it->value;
  if (v.IsUint64()) return static_cast<Id>(v.GetUint64());
  if (v.IsInt64() && v.GetInt64() > 0) return static_cast<Id>(v.GetInt64());
  if (v.IsString()) return parseIdKey(v.GetString());
  return 0;
}

static std::uint32_t getUint(const rapidjson::Value& obj, const char* key, std::uint32_t def = 0) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return def;
  const auto& v = it->value;
  if (v.IsUint()) return v.GetUint();
  if (v.IsInt() && v.GetInt() >= 0) return static_cast<std::uint32_t>(v.GetInt());
  return def;
}

static bool getBool(const rapidjson::Value& obj, const char* key, bool def) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsBool()) return def;
  return it->value.GetBool();
}

static void parseColor4(const rapidjson::Value& obj, const char* key, float out[4]) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsArray()) return;
  const auto& arr = it->value.GetArray();
  if (arr.Size() >= 4) {
    for (int i = 0; i < 4; i++) {
      if (arr[i].IsNumber()) out[i] = static_cast<float>(arr[i].GetDouble());
    }
  }
}

static void parseColor2(const rapidjson::Value& obj, const char* key, float out[2]) {
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd() || !it->value.IsArray()) return;
  const auto& arr = it->value.GetArray();
  if (arr.Size() >= 2) {
    for (int i = 0; i < 2; i++) {
      if (arr[i].IsNumber()) out[i] = static_cast<float>(arr[i].GetDouble());
    }
  }
}

// --- Parse ---

bool parseSceneDocument(const std::string& json, SceneDocument& out) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) return false;

  out = SceneDocument{};

  // version
  out.version = static_cast<int>(getUint(doc, "version", 1));

  // viewport
  if (doc.HasMember("viewport") && doc["viewport"].IsObject()) {
    const auto& vp = doc["viewport"];
    out.viewportWidth = static_cast<int>(getUint(vp, "width", 0));
    out.viewportHeight = static_cast<int>(getUint(vp, "height", 0));
  }

  // buffers
  if (doc.HasMember("buffers") && doc["buffers"].IsObject()) {
    for (auto it = doc["buffers"].MemberBegin(); it != doc["buffers"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      DocBuffer b;
      b.byteLength = getUint(it->value, "byteLength", 0);
      // Inline float data
      auto dit = it->value.FindMember("data");
      if (dit != it->value.MemberEnd() && dit->value.IsArray()) {
        const auto& arr = dit->value.GetArray();
        b.data.reserve(arr.Size());
        for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
          if (arr[i].IsNumber()) b.data.push_back(static_cast<float>(arr[i].GetDouble()));
        }
        // Derive byteLength from data if not explicitly set
        if (b.byteLength == 0 && !b.data.empty()) {
          b.byteLength = static_cast<std::uint32_t>(b.data.size() * sizeof(float));
        }
      }
      out.buffers[id] = std::move(b);
    }
  }

  // transforms
  if (doc.HasMember("transforms") && doc["transforms"].IsObject()) {
    for (auto it = doc["transforms"].MemberBegin(); it != doc["transforms"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      DocTransform t;
      t.tx = getFloat(it->value, "tx", 0);
      t.ty = getFloat(it->value, "ty", 0);
      t.sx = getFloat(it->value, "sx", 1);
      t.sy = getFloat(it->value, "sy", 1);
      out.transforms[id] = t;
    }
  }

  // panes
  if (doc.HasMember("panes") && doc["panes"].IsObject()) {
    for (auto it = doc["panes"].MemberBegin(); it != doc["panes"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      DocPane p;
      p.name = getString(it->value, "name");
      if (it->value.HasMember("region") && it->value["region"].IsObject()) {
        const auto& r = it->value["region"];
        p.region.clipYMin = getFloat(r, "clipYMin", -1.0f);
        p.region.clipYMax = getFloat(r, "clipYMax", 1.0f);
        p.region.clipXMin = getFloat(r, "clipXMin", -1.0f);
        p.region.clipXMax = getFloat(r, "clipXMax", 1.0f);
      }
      parseColor4(it->value, "clearColor", p.clearColor);
      p.hasClearColor = getBool(it->value, "hasClearColor", false);
      out.panes[id] = p;
    }
  }

  // layers
  if (doc.HasMember("layers") && doc["layers"].IsObject()) {
    for (auto it = doc["layers"].MemberBegin(); it != doc["layers"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      DocLayer l;
      l.paneId = getId(it->value, "paneId");
      l.name = getString(it->value, "name");
      out.layers[id] = l;
    }
  }

  // geometries
  if (doc.HasMember("geometries") && doc["geometries"].IsObject()) {
    for (auto it = doc["geometries"].MemberBegin(); it != doc["geometries"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      DocGeometry g;
      g.vertexBufferId = getId(it->value, "vertexBufferId");
      g.format = getString(it->value, "format", "pos2_clip");
      g.vertexCount = getUint(it->value, "vertexCount", 1);
      g.indexBufferId = getId(it->value, "indexBufferId");
      g.indexCount = getUint(it->value, "indexCount", 0);
      out.geometries[id] = g;
    }
  }

  // drawItems
  if (doc.HasMember("drawItems") && doc["drawItems"].IsObject()) {
    for (auto it = doc["drawItems"].MemberBegin(); it != doc["drawItems"].MemberEnd(); ++it) {
      Id id = parseIdKey(it->name.GetString());
      if (id == 0 || !it->value.IsObject()) continue;
      const auto& v = it->value;
      DocDrawItem d;
      d.layerId = getId(v, "layerId");
      d.name = getString(v, "name");
      d.pipeline = getString(v, "pipeline");
      d.geometryId = getId(v, "geometryId");
      d.transformId = getId(v, "transformId");

      parseColor4(v, "color", d.color);
      parseColor4(v, "colorUp", d.colorUp);
      parseColor4(v, "colorDown", d.colorDown);

      d.pointSize = getFloat(v, "pointSize", 4.0f);
      d.lineWidth = getFloat(v, "lineWidth", 1.0f);
      d.dashLength = getFloat(v, "dashLength", 0.0f);
      d.gapLength = getFloat(v, "gapLength", 0.0f);
      d.cornerRadius = getFloat(v, "cornerRadius", 0.0f);

      d.blendMode = getString(v, "blendMode", "normal");
      d.isClipSource = getBool(v, "isClipSource", false);
      d.useClipMask = getBool(v, "useClipMask", false);

      d.textureId = getUint(v, "textureId", 0);

      d.anchorPoint = getString(v, "anchorPoint");
      d.anchorOffsetX = getFloat(v, "anchorOffsetX", 0);
      d.anchorOffsetY = getFloat(v, "anchorOffsetY", 0);

      d.visible = getBool(v, "visible", true);

      d.gradientType = getString(v, "gradientType");
      d.gradientAngle = getFloat(v, "gradientAngle", 0.0f);
      parseColor4(v, "gradientColor0", d.gradientColor0);
      parseColor4(v, "gradientColor1", d.gradientColor1);
      parseColor2(v, "gradientCenter", d.gradientCenter);
      d.gradientRadius = getFloat(v, "gradientRadius", 0.5f);

      out.drawItems[id] = d;
    }
  }

  // viewports
  if (doc.HasMember("viewports") && doc["viewports"].IsObject()) {
    for (auto it = doc["viewports"].MemberBegin(); it != doc["viewports"].MemberEnd(); ++it) {
      if (!it->value.IsObject()) continue;
      std::string name = it->name.GetString();
      DocViewport vp;
      vp.transformId = getId(it->value, "transformId");
      vp.paneId = getId(it->value, "paneId");
      vp.xMin = static_cast<double>(getFloat(it->value, "xMin", 0));
      vp.xMax = static_cast<double>(getFloat(it->value, "xMax", 1));
      vp.yMin = static_cast<double>(getFloat(it->value, "yMin", 0));
      vp.yMax = static_cast<double>(getFloat(it->value, "yMax", 1));
      vp.linkGroup = getString(it->value, "linkGroup");
      vp.panX = getBool(it->value, "panX", true);
      vp.panY = getBool(it->value, "panY", true);
      vp.zoomX = getBool(it->value, "zoomX", true);
      vp.zoomY = getBool(it->value, "zoomY", true);
      out.viewports[name] = vp;
    }
  }

  // textOverlay
  if (doc.HasMember("textOverlay") && doc["textOverlay"].IsObject()) {
    const auto& ov = doc["textOverlay"];
    out.textOverlay.fontSize = static_cast<int>(getUint(ov, "fontSize", 12));
    out.textOverlay.color = getString(ov, "color", "#b2b5bc");
    auto lit = ov.FindMember("labels");
    if (lit != ov.MemberEnd() && lit->value.IsArray()) {
      const auto& arr = lit->value.GetArray();
      out.textOverlay.labels.reserve(arr.Size());
      for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
        if (!arr[i].IsObject()) continue;
        DocTextLabel lbl;
        lbl.clipX = getFloat(arr[i], "clipX", 0);
        lbl.clipY = getFloat(arr[i], "clipY", 0);
        lbl.text = getString(arr[i], "text");
        lbl.align = getString(arr[i], "align", "l");
        lbl.color = getString(arr[i], "color");
        lbl.fontSize = static_cast<int>(getUint(arr[i], "fontSize", 0));
        out.textOverlay.labels.push_back(std::move(lbl));
      }
    }
  }

  return true;
}

// --- Serialize helpers ---

using RjWriter = rapidjson::Writer<rapidjson::StringBuffer>;

static void writeColor4(RjWriter& w, const float c[4]) {
  w.StartArray();
  for (int i = 0; i < 4; i++) w.Double(static_cast<double>(c[i]));
  w.EndArray();
}

static void writeColor2(RjWriter& w, const float c[2]) {
  w.StartArray();
  for (int i = 0; i < 2; i++) w.Double(static_cast<double>(c[i]));
  w.EndArray();
}

static bool feq(float a, float b) { return std::fabs(a - b) < 1e-6f; }

static bool color4Eq(const float a[4], const float b[4]) {
  return feq(a[0], b[0]) && feq(a[1], b[1]) && feq(a[2], b[2]) && feq(a[3], b[3]);
}

static bool color2Eq(const float a[2], const float b[2]) {
  return feq(a[0], b[0]) && feq(a[1], b[1]);
}

std::string serializeSceneDocument(const SceneDocument& doc, bool compact) {
  // Defaults for comparison in compact mode
  const DocDrawItem defDI;
  const DocGeometry defGeom;
  const DocPaneRegion defReg;
  const float defClearColor[4] = {0, 0, 0, 1};

  rapidjson::StringBuffer sb;
  RjWriter w(sb);

  w.StartObject();

  w.Key("version"); w.Int(doc.version);

  if (!compact || doc.viewportWidth != 0 || doc.viewportHeight != 0) {
    w.Key("viewport");
    w.StartObject();
    w.Key("width"); w.Int(doc.viewportWidth);
    w.Key("height"); w.Int(doc.viewportHeight);
    w.EndObject();
  }

  // buffers
  if (!compact || !doc.buffers.empty()) {
    w.Key("buffers");
    w.StartObject();
    for (const auto& [id, b] : doc.buffers) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();
      if (!b.data.empty()) {
        w.Key("data");
        w.StartArray();
        for (float f : b.data) w.Double(static_cast<double>(f));
        w.EndArray();
      }
      if (!compact || b.data.empty()) {
        w.Key("byteLength"); w.Uint(b.byteLength);
      }
      w.EndObject();
    }
    w.EndObject();
  }

  // transforms
  if (!compact || !doc.transforms.empty()) {
    w.Key("transforms");
    w.StartObject();
    for (const auto& [id, t] : doc.transforms) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();
      w.Key("tx"); w.Double(static_cast<double>(t.tx));
      w.Key("ty"); w.Double(static_cast<double>(t.ty));
      w.Key("sx"); w.Double(static_cast<double>(t.sx));
      w.Key("sy"); w.Double(static_cast<double>(t.sy));
      w.EndObject();
    }
    w.EndObject();
  }

  // panes
  if (!compact || !doc.panes.empty()) {
    w.Key("panes");
    w.StartObject();
    for (const auto& [id, p] : doc.panes) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();
      if (!compact || !p.name.empty()) { w.Key("name"); w.String(p.name.c_str()); }

      bool regionNonDefault = !feq(p.region.clipYMin, defReg.clipYMin) ||
                              !feq(p.region.clipYMax, defReg.clipYMax) ||
                              !feq(p.region.clipXMin, defReg.clipXMin) ||
                              !feq(p.region.clipXMax, defReg.clipXMax);
      if (!compact || regionNonDefault) {
        w.Key("region");
        w.StartObject();
        w.Key("clipYMin"); w.Double(static_cast<double>(p.region.clipYMin));
        w.Key("clipYMax"); w.Double(static_cast<double>(p.region.clipYMax));
        w.Key("clipXMin"); w.Double(static_cast<double>(p.region.clipXMin));
        w.Key("clipXMax"); w.Double(static_cast<double>(p.region.clipXMax));
        w.EndObject();
      }

      if (!compact || p.hasClearColor) {
        if (!compact || !color4Eq(p.clearColor, defClearColor)) {
          w.Key("clearColor"); writeColor4(w, p.clearColor);
        }
        w.Key("hasClearColor"); w.Bool(p.hasClearColor);
      }
      w.EndObject();
    }
    w.EndObject();
  }

  // layers
  if (!compact || !doc.layers.empty()) {
    w.Key("layers");
    w.StartObject();
    for (const auto& [id, l] : doc.layers) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();
      w.Key("paneId"); w.Uint64(l.paneId);
      if (!compact || !l.name.empty()) { w.Key("name"); w.String(l.name.c_str()); }
      w.EndObject();
    }
    w.EndObject();
  }

  // geometries
  if (!compact || !doc.geometries.empty()) {
    w.Key("geometries");
    w.StartObject();
    for (const auto& [id, g] : doc.geometries) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();
      w.Key("vertexBufferId"); w.Uint64(g.vertexBufferId);
      if (!compact || g.format != defGeom.format) { w.Key("format"); w.String(g.format.c_str()); }
      w.Key("vertexCount"); w.Uint(g.vertexCount);
      if (!compact || g.indexBufferId != 0) { w.Key("indexBufferId"); w.Uint64(g.indexBufferId); }
      if (!compact || g.indexCount != 0)    { w.Key("indexCount"); w.Uint(g.indexCount); }
      w.EndObject();
    }
    w.EndObject();
  }

  // drawItems
  if (!compact || !doc.drawItems.empty()) {
    w.Key("drawItems");
    w.StartObject();
    for (const auto& [id, d] : doc.drawItems) {
      w.Key(std::to_string(id).c_str());
      w.StartObject();

      // Always write structural fields
      w.Key("layerId"); w.Uint64(d.layerId);
      if (!compact || !d.name.empty())     { w.Key("name"); w.String(d.name.c_str()); }
      if (!compact || !d.pipeline.empty()) { w.Key("pipeline"); w.String(d.pipeline.c_str()); }
      if (!compact || d.geometryId != 0)   { w.Key("geometryId"); w.Uint64(d.geometryId); }
      if (!compact || d.transformId != 0)  { w.Key("transformId"); w.Uint64(d.transformId); }

      // Colors
      if (!compact || !color4Eq(d.color, defDI.color))       { w.Key("color"); writeColor4(w, d.color); }
      if (!compact || !color4Eq(d.colorUp, defDI.colorUp))   { w.Key("colorUp"); writeColor4(w, d.colorUp); }
      if (!compact || !color4Eq(d.colorDown, defDI.colorDown)) { w.Key("colorDown"); writeColor4(w, d.colorDown); }

      // Numeric style
      if (!compact || !feq(d.pointSize, defDI.pointSize))       { w.Key("pointSize"); w.Double(static_cast<double>(d.pointSize)); }
      if (!compact || !feq(d.lineWidth, defDI.lineWidth))       { w.Key("lineWidth"); w.Double(static_cast<double>(d.lineWidth)); }
      if (!compact || !feq(d.dashLength, defDI.dashLength))     { w.Key("dashLength"); w.Double(static_cast<double>(d.dashLength)); }
      if (!compact || !feq(d.gapLength, defDI.gapLength))       { w.Key("gapLength"); w.Double(static_cast<double>(d.gapLength)); }
      if (!compact || !feq(d.cornerRadius, defDI.cornerRadius)) { w.Key("cornerRadius"); w.Double(static_cast<double>(d.cornerRadius)); }

      // Blend / clip
      if (!compact || d.blendMode != defDI.blendMode)   { w.Key("blendMode"); w.String(d.blendMode.c_str()); }
      if (!compact || d.isClipSource != defDI.isClipSource) { w.Key("isClipSource"); w.Bool(d.isClipSource); }
      if (!compact || d.useClipMask != defDI.useClipMask)   { w.Key("useClipMask"); w.Bool(d.useClipMask); }

      // Texture
      if (!compact || d.textureId != defDI.textureId) { w.Key("textureId"); w.Uint(d.textureId); }

      // Anchor
      if (!compact || !d.anchorPoint.empty()) {
        w.Key("anchorPoint"); w.String(d.anchorPoint.c_str());
        if (!compact || !feq(d.anchorOffsetX, 0)) { w.Key("anchorOffsetX"); w.Double(static_cast<double>(d.anchorOffsetX)); }
        if (!compact || !feq(d.anchorOffsetY, 0)) { w.Key("anchorOffsetY"); w.Double(static_cast<double>(d.anchorOffsetY)); }
      }

      // Visible
      if (!compact || !d.visible) { w.Key("visible"); w.Bool(d.visible); }

      // Gradient
      bool hasGradient = !d.gradientType.empty() && d.gradientType != "none";
      if (!compact || hasGradient) {
        w.Key("gradientType"); w.String(d.gradientType.c_str());
        if (!compact || hasGradient) {
          if (!compact || !feq(d.gradientAngle, defDI.gradientAngle)) { w.Key("gradientAngle"); w.Double(static_cast<double>(d.gradientAngle)); }
          if (!compact || !color4Eq(d.gradientColor0, defDI.gradientColor0)) { w.Key("gradientColor0"); writeColor4(w, d.gradientColor0); }
          if (!compact || !color4Eq(d.gradientColor1, defDI.gradientColor1)) { w.Key("gradientColor1"); writeColor4(w, d.gradientColor1); }
          if (!compact || !color2Eq(d.gradientCenter, defDI.gradientCenter)) { w.Key("gradientCenter"); writeColor2(w, d.gradientCenter); }
          if (!compact || !feq(d.gradientRadius, defDI.gradientRadius)) { w.Key("gradientRadius"); w.Double(static_cast<double>(d.gradientRadius)); }
        }
      }

      w.EndObject();
    }
    w.EndObject();
  }

  // viewports
  if (!compact || !doc.viewports.empty()) {
    w.Key("viewports");
    w.StartObject();
    for (const auto& [name, vp] : doc.viewports) {
      w.Key(name.c_str());
      w.StartObject();
      w.Key("transformId"); w.Uint64(vp.transformId);
      w.Key("paneId"); w.Uint64(vp.paneId);
      w.Key("xMin"); w.Double(vp.xMin);
      w.Key("xMax"); w.Double(vp.xMax);
      w.Key("yMin"); w.Double(vp.yMin);
      w.Key("yMax"); w.Double(vp.yMax);
      if (!compact || !vp.linkGroup.empty()) { w.Key("linkGroup"); w.String(vp.linkGroup.c_str()); }
      if (!compact || !vp.panX)  { w.Key("panX"); w.Bool(vp.panX); }
      if (!compact || !vp.panY)  { w.Key("panY"); w.Bool(vp.panY); }
      if (!compact || !vp.zoomX) { w.Key("zoomX"); w.Bool(vp.zoomX); }
      if (!compact || !vp.zoomY) { w.Key("zoomY"); w.Bool(vp.zoomY); }
      w.EndObject();
    }
    w.EndObject();
  }

  // textOverlay
  const DocTextOverlay defOv;
  bool overlayNonDefault = doc.textOverlay.fontSize != defOv.fontSize ||
                           doc.textOverlay.color != defOv.color ||
                           !doc.textOverlay.labels.empty();
  if (!compact || overlayNonDefault) {
    w.Key("textOverlay");
    w.StartObject();
    w.Key("fontSize"); w.Int(doc.textOverlay.fontSize);
    w.Key("color"); w.String(doc.textOverlay.color.c_str());
    if (!compact || !doc.textOverlay.labels.empty()) {
      w.Key("labels");
      w.StartArray();
      const DocTextLabel defLbl;
      for (const auto& lbl : doc.textOverlay.labels) {
        w.StartObject();
        w.Key("clipX"); w.Double(static_cast<double>(lbl.clipX));
        w.Key("clipY"); w.Double(static_cast<double>(lbl.clipY));
        w.Key("text"); w.String(lbl.text.c_str());
        if (!compact || lbl.align != defLbl.align) { w.Key("align"); w.String(lbl.align.c_str()); }
        if (!compact || !lbl.color.empty()) { w.Key("color"); w.String(lbl.color.c_str()); }
        if (!compact || lbl.fontSize != 0) { w.Key("fontSize"); w.Int(lbl.fontSize); }
        w.EndObject();
      }
      w.EndArray();
    }
    w.EndObject();
  }

  w.EndObject();
  return sb.GetString();
}

} // namespace dc
