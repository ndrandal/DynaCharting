#include "dc/commands/CommandProcessor.hpp"

#include "dc/pipelines/PipelineCatalog.hpp"
#include "dc/scene/Geometry.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>
#include <vector>

namespace dc {

CommandProcessor::CommandProcessor(Scene& scene, ResourceRegistry& registry)
  : scene_(scene), reg_(registry) {}

CmdResult CommandProcessor::fail(const std::string& code,
                                 const std::string& message,
                                 const std::string& detailsJson) {
  CmdResult r;
  r.ok = false;
  r.err.code = code;
  r.err.message = message;
  r.err.details = detailsJson.empty() ? "{}" : detailsJson;
  r.createdId = 0;
  return r;
}

const rapidjson::Value* CommandProcessor::getMember(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) return nullptr;
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return nullptr;
  return &it->value;
}

std::string CommandProcessor::getStringOrEmpty(const rapidjson::Value& obj, const char* key) {
  const auto* v = getMember(obj, key);
  if (!v) return {};
  if (v->IsString()) return v->GetString();
  return {};
}

Id CommandProcessor::getIdOrZero(const rapidjson::Value& obj, const char* key) {
  const auto* v = getMember(obj, key);
  if (!v) return 0;

  if (v->IsUint64()) return static_cast<Id>(v->GetUint64());
  if (v->IsInt64() && v->GetInt64() > 0) return static_cast<Id>(v->GetInt64());
  if (v->IsString()) return parseIdString(v->GetString());
  return 0;
}

CmdResult CommandProcessor::applyJsonText(const std::string& jsonText) {
  rapidjson::Document d;
  d.Parse(jsonText.c_str());

  if (d.HasParseError() || !d.IsObject()) {
    return fail("BAD_COMMAND", "CommandProcessor: invalid JSON object");
  }

  return applyJson(d);
}

CmdResult CommandProcessor::applyJson(const rapidjson::Value& obj) {
  const auto* cmdV = getMember(obj, "cmd");
  if (!cmdV || !cmdV->IsString()) {
    return fail("BAD_COMMAND", "Missing string field: cmd");
  }

  const std::string cmd = cmdV->GetString();

  // D1.1
  if (cmd == "hello") return cmdHello(obj);
  if (cmd == "beginFrame") return cmdBeginFrame(obj);
  if (cmd == "commitFrame") return cmdCommitFrame(obj);

  if (cmd == "createPane") return cmdCreatePane(obj);
  if (cmd == "createLayer") return cmdCreateLayer(obj);
  if (cmd == "createDrawItem") return cmdCreateDrawItem(obj);
  if (cmd == "delete") return cmdDelete(obj);

  // triSolid deliverable plumbing
  if (cmd == "createBuffer") return cmdCreateBuffer(obj);
  if (cmd == "createGeometry") return cmdCreateGeometry(obj);
  if (cmd == "bindDrawItem") return cmdBindDrawItem(obj);

  return fail("UNKNOWN_COMMAND",
              "Unknown cmd",
              std::string(R"({"cmd":")") + cmd + R"("})");
}

// -------------------- D1.1 handlers --------------------

CmdResult CommandProcessor::cmdHello(const rapidjson::Value&) {
  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdBeginFrame(const rapidjson::Value&) {
  if (inFrame_) {
    return fail("BAD_COMMAND", "beginFrame: already in frame");
  }

  inFrame_ = true;
  frameCounter_++;

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdCommitFrame(const rapidjson::Value&) {
  if (!inFrame_) {
    return fail("BAD_COMMAND", "commitFrame: not in frame");
  }

  inFrame_ = false;

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdCreatePane(const rapidjson::Value& obj) {
  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Pane)) {
      return fail("ID_TAKEN", "createPane: id already exists");
    }
  } else {
    id = reg_.allocate(ResourceKind::Pane);
  }

  Pane p;
  p.id = id;
  p.name = getStringOrEmpty(obj, "name");
  scene_.addPane(std::move(p));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateLayer(const rapidjson::Value& obj) {
  const Id paneId = getIdOrZero(obj, "paneId");
  if (paneId == 0 || !scene_.hasPane(paneId)) {
    return fail("VALIDATION_INVALID_PARENT",
                "createLayer: invalid paneId",
                std::string(R"({"field":"paneId","paneId":)") + std::to_string(paneId) + "}");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Layer)) {
      return fail("ID_TAKEN", "createLayer: id already exists");
    }
  } else {
    id = reg_.allocate(ResourceKind::Layer);
  }

  Layer l;
  l.id = id;
  l.paneId = paneId;
  l.name = getStringOrEmpty(obj, "name");
  scene_.addLayer(std::move(l));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateDrawItem(const rapidjson::Value& obj) {
  const Id layerId = getIdOrZero(obj, "layerId");
  if (layerId == 0 || !scene_.hasLayer(layerId)) {
    return fail("VALIDATION_INVALID_PARENT",
                "createDrawItem: invalid layerId",
                std::string(R"({"field":"layerId","layerId":)") + std::to_string(layerId) + "}");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::DrawItem)) {
      return fail("ID_TAKEN", "createDrawItem: id already exists");
    }
  } else {
    id = reg_.allocate(ResourceKind::DrawItem);
  }

  DrawItem d;
  d.id = id;
  d.layerId = layerId;
  d.name = getStringOrEmpty(obj, "name");
  // pipeline + geometry bindings default empty/0; set by bindDrawItem
  scene_.addDrawItem(std::move(d));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdDelete(const rapidjson::Value& obj) {
  const Id id = getIdOrZero(obj, "id");
  if (id == 0) {
    return fail("BAD_COMMAND", "delete: missing/invalid id");
  }

  if (!reg_.exists(id)) {
    return fail("NOT_FOUND",
                "delete: id does not exist",
                std::string(R"({"id":)") + std::to_string(id) + "}");
  }

  const auto kind = reg_.kindOf(id);

  std::vector<Id> deleted;
  switch (kind) {
    case ResourceKind::Pane:
      deleted = scene_.deletePane(id);
      break;
    case ResourceKind::Layer:
      deleted = scene_.deleteLayer(id);
      break;
    case ResourceKind::DrawItem:
      deleted = scene_.deleteDrawItem(id);
      break;
    case ResourceKind::Buffer:
      deleted = scene_.deleteBuffer(id);
      break;
    case ResourceKind::Geometry:
      deleted = scene_.deleteGeometry(id);
      break;
    default:
      deleted.clear();
      break;
  }

  if (deleted.empty()) {
    return fail("DELETE_FAILED",
                "delete: failed",
                std::string(R"({"id":)") + std::to_string(id) + "}");
  }

  for (Id did : deleted) {
    reg_.release(did);
  }

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

// -------------------- triSolid plumbing --------------------

CmdResult CommandProcessor::cmdCreateBuffer(const rapidjson::Value& obj) {
  const auto* bl = getMember(obj, "byteLength");
  if (!bl || !bl->IsUint()) {
    return fail("BAD_COMMAND", "createBuffer: missing uint byteLength");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Buffer)) {
      return fail("ID_TAKEN", "createBuffer: id already exists");
    }
  } else {
    id = reg_.allocate(ResourceKind::Buffer);
  }

  Buffer b;
  b.id = id;
  b.byteLength = bl->GetUint();
  scene_.addBuffer(std::move(b));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateGeometry(const rapidjson::Value& obj) {
  const Id vb = getIdOrZero(obj, "vertexBufferId");
  if (vb == 0 || !scene_.hasBuffer(vb)) {
    return fail("MISSING_BUFFER",
                "createGeometry: invalid vertexBufferId",
                std::string(R"({"field":"vertexBufferId","vertexBufferId":)") + std::to_string(vb) + "}");
  }

  const auto* vc = getMember(obj, "vertexCount");
  if (!vc || !vc->IsUint()) {
    return fail("BAD_COMMAND", "createGeometry: missing uint vertexCount");
  }

  // v1 supports only pos2_clip (vec2 in clip space)
  VertexFormat fmt = VertexFormat::Pos2_Clip;
  if (const auto* f = getMember(obj, "format"); f && f->IsString()) {
    const std::string s = f->GetString();
    if (s != "pos2_clip") {
      return fail("UNSUPPORTED_VERTEX_FORMAT",
                  "createGeometry: only format=pos2_clip supported",
                  R"({"supported":["pos2_clip"]})");
    }
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Geometry)) {
      return fail("ID_TAKEN", "createGeometry: id already exists");
    }
  } else {
    id = reg_.allocate(ResourceKind::Geometry);
  }

  Geometry g;
  g.id = id;
  g.vertexBufferId = vb;
  g.format = fmt;
  g.vertexCount = vc->GetUint();
  scene_.addGeometry(std::move(g));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdBindDrawItem(const rapidjson::Value& obj) {
  const Id drawItemId = getIdOrZero(obj, "drawItemId");
  if (drawItemId == 0) {
    return fail("BAD_COMMAND", "bindDrawItem: missing/invalid drawItemId");
  }

  DrawItem* di = scene_.getDrawItemMutable(drawItemId);
  if (!di) {
    return fail("MISSING_DRAWITEM",
                "bindDrawItem: drawItemId does not exist",
                std::string(R"({"drawItemId":)") + std::to_string(drawItemId) + "}");
  }

  const std::string pipeline = getStringOrEmpty(obj, "pipeline");
  if (pipeline.empty()) {
    return fail("BAD_COMMAND", "bindDrawItem: missing pipeline");
  }

  const Id geomId = getIdOrZero(obj, "geometryId");
  if (geomId == 0) {
    return fail("BAD_COMMAND", "bindDrawItem: missing geometryId");
  }

  di->pipeline = pipeline;
  di->geometryId = geomId;

  // Core validation required by deliverable
  return validateDrawItem(*di);
}

CmdResult CommandProcessor::validateDrawItem(const DrawItem& di) const {
  const PipelineSpec* spec = catalog_.find(di.pipeline);
  if (!spec) {
    return fail("UNKNOWN_PIPELINE",
                "drawItem pipeline not found",
                std::string(R"({"pipeline":")") + di.pipeline + R"("})");
  }

  if (di.geometryId == 0) {
    return fail("VALIDATION_MISSING_GEOMETRY",
                "drawItem must bind geometryId",
                std::string(R"({"drawItemId":)") + std::to_string(di.id) + "}");
  }

  const Geometry* g = scene_.getGeometry(di.geometryId);
  if (!g) {
    return fail("VALIDATION_BAD_GEOMETRY",
                "drawItem geometryId does not exist",
                std::string(R"({"geometryId":)") + std::to_string(di.geometryId) + "}");
  }

  const Buffer* b = scene_.getBuffer(g->vertexBufferId);
  if (!b) {
    return fail("VALIDATION_MISSING_BUFFER",
                "geometry must reference an existing vertexBufferId",
                std::string(R"({"vertexBufferId":)") + std::to_string(g->vertexBufferId) + "}");
  }

  if (g->format != spec->requiredVertexFormat) {
    return fail("VALIDATION_VERTEX_FORMAT_MISMATCH",
                "geometry vertex format does not match pipeline requirement",
                std::string(R"({"pipeline":")") + di.pipeline +
                  R"(","required":")" + toString(spec->requiredVertexFormat) +
                  R"(","got":")" + toString(g->format) + R"("})");
  }

  // If you want one more strict check for v1 without adding new features:
  // ensure vertexCount is a multiple of 3 (triangles).
  // This is consistent with “draw triangles” and still minimal.
  if ((g->vertexCount % 3u) != 0u) {
    return fail("VALIDATION_BAD_VERTEX_COUNT",
                "triSolid@1 requires vertexCount multiple of 3",
                std::string(R"({"vertexCount":)") + std::to_string(g->vertexCount) + "}");
  }

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

// -------------------- Query --------------------

std::string CommandProcessor::listResourcesJson() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();

  w.Key("panes");
  w.StartArray();
  for (Id id : reg_.list(ResourceKind::Pane)) w.Uint64(id);
  w.EndArray();

  w.Key("layers");
  w.StartArray();
  for (Id id : reg_.list(ResourceKind::Layer)) w.Uint64(id);
  w.EndArray();

  w.Key("drawItems");
  w.StartArray();
  for (Id id : reg_.list(ResourceKind::DrawItem)) w.Uint64(id);
  w.EndArray();

  w.Key("buffers");
  w.StartArray();
  for (Id id : reg_.list(ResourceKind::Buffer)) w.Uint64(id);
  w.EndArray();

  w.Key("geometries");
  w.StartArray();
  for (Id id : reg_.list(ResourceKind::Geometry)) w.Uint64(id);
  w.EndArray();

  w.Key("frame");
  w.Uint64(frameCounter_);

  w.Key("inFrame");
  w.Bool(inFrame_);

  w.EndObject();

  return sb.GetString();
}

} // namespace dc
