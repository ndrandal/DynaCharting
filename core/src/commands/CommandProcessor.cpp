#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include "dc/pipelines/PipelineCatalog.hpp"
#include "dc/scene/Geometry.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <string>
#include <vector>

namespace dc {

CommandProcessor::CommandProcessor(Scene& scene, ResourceRegistry& registry)
  : activeScene_(scene)
  , activeReg_(registry) {}

void CommandProcessor::setIngestProcessor(IngestProcessor* ingest) {
  ingest_ = ingest;
}

void CommandProcessor::setGlyphAtlas(GlyphAtlas* atlas) {
  atlas_ = atlas;
}

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

std::uint64_t CommandProcessor::getU64OrZero(const rapidjson::Value& obj, const char* key) {
  const auto* v = getMember(obj, key);
  if (!v) return 0;
  if (v->IsUint64()) return v->GetUint64();
  if (v->IsInt64() && v->GetInt64() > 0) return static_cast<std::uint64_t>(v->GetInt64());
  return 0;
}

Scene& CommandProcessor::curScene() {
  return inFrame_ ? pendingScene_ : activeScene_;
}
ResourceRegistry& CommandProcessor::curReg() {
  return inFrame_ ? pendingReg_ : activeReg_;
}
const Scene& CommandProcessor::curScene() const {
  return inFrame_ ? pendingScene_ : activeScene_;
}
const ResourceRegistry& CommandProcessor::curReg() const {
  return inFrame_ ? pendingReg_ : activeReg_;
}

CmdResult CommandProcessor::rejectIfFrameFailed(const char* cmdName) const {
  if (inFrame_ && frameFailed_) {
    return fail("FRAME_REJECTED",
                std::string(cmdName) + ": frame already failed; no further commands accepted until commitFrame",
                std::string(R"({"pendingFrameId":)") + std::to_string(pendingFrameId_) + "}");
  }
  CmdResult ok;
  ok.ok = true;
  ok.createdId = 0;
  return ok;
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

  // Always allow these (even if a frame already failed).
  if (cmd == "hello") return cmdHello(obj);
  if (cmd == "beginFrame") return cmdBeginFrame(obj);
  if (cmd == "commitFrame") return cmdCommitFrame(obj);

  // If a command fails inside a frame, reject subsequent commands to avoid “half-built” pending edits.
  // (Still no partial commit either way, but this keeps behavior strict + simple.)
  if (inFrame_ && frameFailed_) {
    return fail("FRAME_REJECTED",
                "frame already failed; only commitFrame is allowed",
                std::string(R"({"pendingFrameId":)") + std::to_string(pendingFrameId_) + "}");
  }

  CmdResult r;

  if (cmd == "createPane") r = cmdCreatePane(obj);
  else if (cmd == "createLayer") r = cmdCreateLayer(obj);
  else if (cmd == "createDrawItem") r = cmdCreateDrawItem(obj);
  else if (cmd == "delete") r = cmdDelete(obj);
  else if (cmd == "createBuffer") r = cmdCreateBuffer(obj);
  else if (cmd == "createGeometry") r = cmdCreateGeometry(obj);
  else if (cmd == "bindDrawItem") r = cmdBindDrawItem(obj);
  else if (cmd == "createTransform") r = cmdCreateTransform(obj);
  else if (cmd == "setTransform") r = cmdSetTransform(obj);
  else if (cmd == "attachTransform") r = cmdAttachTransform(obj);
  else if (cmd == "bufferSetMaxBytes") r = cmdBufferSetMaxBytes(obj);
  else if (cmd == "bufferEvictFront") r = cmdBufferEvictFront(obj);
  else if (cmd == "bufferKeepLast") r = cmdBufferKeepLast(obj);
  else if (cmd == "setDrawItemPipeline") r = cmdSetDrawItemPipeline(obj);
  else if (cmd == "setGeometryVertexCount") r = cmdSetGeometryVertexCount(obj);
  else if (cmd == "setGeometryBuffer") r = cmdSetGeometryBuffer(obj);
  else if (cmd == "ensureGlyphs") r = cmdEnsureGlyphs(obj);
  else if (cmd == "setDrawItemColor") r = cmdSetDrawItemColor(obj);
  else {
    r = fail("UNKNOWN_COMMAND",
             "Unknown cmd",
             std::string(R"({"cmd":")") + cmd + R"("})");
  }

  // Transaction semantics: if we're in a frame and a command fails, mark frameFailed_.
  if (inFrame_ && !r.ok) {
    frameFailed_ = true;
  }

  return r;
}

// -------------------- D1.3 handlers --------------------

CmdResult CommandProcessor::cmdHello(const rapidjson::Value&) {
  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdBeginFrame(const rapidjson::Value& obj) {
  if (inFrame_) {
    return fail("BAD_COMMAND", "beginFrame: already in frame");
  }

  std::uint64_t requested = getU64OrZero(obj, "frameId");
  if (requested == 0) {
    requested = activeFrameId_ + 1;
  }

  // Snapshot active -> pending (transaction start)
  pendingScene_ = activeScene_;
  pendingReg_   = activeReg_;

  inFrame_ = true;
  frameFailed_ = false;
  pendingFrameId_ = requested;

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdCommitFrame(const rapidjson::Value& obj) {
  if (!inFrame_) {
    return fail("BAD_COMMAND", "commitFrame: not in frame");
  }

  std::uint64_t requested = getU64OrZero(obj, "frameId");
  if (requested != 0 && requested != pendingFrameId_) {
    // Frame id mismatch: do not commit.
    inFrame_ = false; // end frame (transaction closed)
    frameFailed_ = true;
    return fail("FRAME_REJECTED",
                "commitFrame: frameId mismatch",
                std::string(R"({"pendingFrameId":)") + std::to_string(pendingFrameId_) +
                  R"(,"commitFrameId":)" + std::to_string(requested) + "}");
  }

  if (frameFailed_) {
    inFrame_ = false; // end frame without commit
    return fail("FRAME_REJECTED",
                "commitFrame: frame failed; nothing committed",
                std::string(R"({"pendingFrameId":)") + std::to_string(pendingFrameId_) + "}");
  }

  // Atomic swap (single-threaded semantics here): pending -> active
  activeScene_ = pendingScene_;
  activeReg_   = pendingReg_;
  activeFrameId_ = pendingFrameId_;

  inFrame_ = false;

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdCreatePane(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::Pane)) {
      return fail("ID_TAKEN", "createPane: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::Pane);
  }

  Pane p;
  p.id = id;
  p.name = getStringOrEmpty(obj, "name");
  scene.addPane(std::move(p));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateLayer(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  const Id paneId = getIdOrZero(obj, "paneId");
  if (paneId == 0 || !scene.hasPane(paneId)) {
    return fail("VALIDATION_INVALID_PARENT",
                "createLayer: invalid paneId",
                std::string(R"({"field":"paneId","paneId":)") + std::to_string(paneId) + "}");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::Layer)) {
      return fail("ID_TAKEN", "createLayer: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::Layer);
  }

  Layer l;
  l.id = id;
  l.paneId = paneId;
  l.name = getStringOrEmpty(obj, "name");
  scene.addLayer(std::move(l));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateDrawItem(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  const Id layerId = getIdOrZero(obj, "layerId");
  if (layerId == 0 || !scene.hasLayer(layerId)) {
    return fail("VALIDATION_INVALID_PARENT",
                "createDrawItem: invalid layerId",
                std::string(R"({"field":"layerId","layerId":)") + std::to_string(layerId) + "}");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::DrawItem)) {
      return fail("ID_TAKEN", "createDrawItem: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::DrawItem);
  }

  DrawItem d;
  d.id = id;
  d.layerId = layerId;
  d.name = getStringOrEmpty(obj, "name");
  scene.addDrawItem(std::move(d));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdDelete(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  const Id id = getIdOrZero(obj, "id");
  if (id == 0) {
    return fail("BAD_COMMAND", "delete: missing/invalid id");
  }

  if (!reg.exists(id)) {
    return fail("NOT_FOUND",
                "delete: id does not exist",
                std::string(R"({"id":)") + std::to_string(id) + "}");
  }

  const auto kind = reg.kindOf(id);

  std::vector<Id> deleted;
  switch (kind) {
    case ResourceKind::Pane:
      deleted = scene.deletePane(id);
      break;
    case ResourceKind::Layer:
      deleted = scene.deleteLayer(id);
      break;
    case ResourceKind::DrawItem:
      deleted = scene.deleteDrawItem(id);
      break;
    case ResourceKind::Buffer:
      deleted = scene.deleteBuffer(id);
      break;
    case ResourceKind::Geometry:
      deleted = scene.deleteGeometry(id);
      break;
    case ResourceKind::Transform:
      deleted = scene.deleteTransform(id);
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
    reg.release(did);
  }

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

// -------------------- triSolid plumbing --------------------

CmdResult CommandProcessor::cmdCreateBuffer(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  const auto* bl = getMember(obj, "byteLength");
  if (!bl || !bl->IsUint()) {
    return fail("BAD_COMMAND", "createBuffer: missing uint byteLength");
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::Buffer)) {
      return fail("ID_TAKEN", "createBuffer: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::Buffer);
  }

  Buffer b;
  b.id = id;
  b.byteLength = bl->GetUint();
  scene.addBuffer(std::move(b));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdCreateGeometry(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  const Id vb = getIdOrZero(obj, "vertexBufferId");
  if (vb == 0 || !scene.hasBuffer(vb)) {
    return fail("MISSING_BUFFER",
                "createGeometry: invalid vertexBufferId",
                std::string(R"({"field":"vertexBufferId","vertexBufferId":)") + std::to_string(vb) + "}");
  }

  const auto* vc = getMember(obj, "vertexCount");
  if (!vc || !vc->IsUint()) {
    return fail("BAD_COMMAND", "createGeometry: missing uint vertexCount");
  }

  VertexFormat fmt = VertexFormat::Pos2_Clip;
  if (const auto* f = getMember(obj, "format"); f && f->IsString()) {
    const std::string s = f->GetString();
    if (s == "pos2_clip")     fmt = VertexFormat::Pos2_Clip;
    else if (s == "rect4")    fmt = VertexFormat::Rect4;
    else if (s == "candle6")  fmt = VertexFormat::Candle6;
    else if (s == "glyph8")   fmt = VertexFormat::Glyph8;
    else {
      return fail("UNSUPPORTED_VERTEX_FORMAT",
                  "createGeometry: unknown format",
                  std::string(R"({"format":")") + s + R"("})");
    }
  }

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::Geometry)) {
      return fail("ID_TAKEN", "createGeometry: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::Geometry);
  }

  Geometry g;
  g.id = id;
  g.vertexBufferId = vb;
  g.format = fmt;
  g.vertexCount = vc->GetUint();
  scene.addGeometry(std::move(g));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdBindDrawItem(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id drawItemId = getIdOrZero(obj, "drawItemId");
  if (drawItemId == 0) {
    return fail("BAD_COMMAND", "bindDrawItem: missing/invalid drawItemId");
  }

  DrawItem* di = scene.getDrawItemMutable(drawItemId);
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

  const Geometry* g = curScene().getGeometry(di.geometryId);
  if (!g) {
    return fail("VALIDATION_BAD_GEOMETRY",
                "drawItem geometryId does not exist",
                std::string(R"({"geometryId":)") + std::to_string(di.geometryId) + "}");
  }

  const Buffer* b = curScene().getBuffer(g->vertexBufferId);
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

  // Pipeline-aware vertex count validation
  bool vcOk = true;
  std::string vcMsg;
  switch (spec->drawMode) {
    case DrawMode::Triangles:
      vcOk = (g->vertexCount % 3u) == 0u;
      vcMsg = "vertexCount must be multiple of 3 for triangle pipelines";
      break;
    case DrawMode::Lines:
      vcOk = (g->vertexCount % 2u) == 0u;
      vcMsg = "vertexCount must be multiple of 2 for line pipelines";
      break;
    case DrawMode::Points:
    case DrawMode::InstancedTriangles:
      vcOk = g->vertexCount >= 1u;
      vcMsg = "vertexCount must be >= 1";
      break;
  }
  if (!vcOk) {
    return fail("VALIDATION_BAD_VERTEX_COUNT", vcMsg,
                std::string(R"({"vertexCount":)") + std::to_string(g->vertexCount) + "}");
  }

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

// -------------------- Transforms --------------------

float CommandProcessor::getFloatOr(const rapidjson::Value& obj, const char* key, float def) {
  const auto* v = getMember(obj, key);
  if (!v) return def;
  if (v->IsFloat() || v->IsDouble()) return static_cast<float>(v->GetDouble());
  if (v->IsInt()) return static_cast<float>(v->GetInt());
  return def;
}

CmdResult CommandProcessor::cmdCreateTransform(const rapidjson::Value& obj) {
  Scene& scene = curScene();
  ResourceRegistry& reg = curReg();

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg.reserve(id, ResourceKind::Transform)) {
      return fail("ID_TAKEN", "createTransform: id already exists");
    }
  } else {
    id = reg.allocate(ResourceKind::Transform);
  }

  Transform t;
  t.id = id;
  // identity params by default
  recomputeMat3(t);
  scene.addTransform(std::move(t));

  CmdResult r;
  r.ok = true;
  r.createdId = id;
  return r;
}

CmdResult CommandProcessor::cmdSetTransform(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id id = getIdOrZero(obj, "id");
  if (id == 0) {
    return fail("BAD_COMMAND", "setTransform: missing/invalid id");
  }

  Transform* t = scene.getTransformMutable(id);
  if (!t) {
    return fail("NOT_FOUND", "setTransform: transform not found",
                std::string(R"({"id":)") + std::to_string(id) + "}");
  }

  // Merge only provided fields
  t->params.tx = getFloatOr(obj, "tx", t->params.tx);
  t->params.ty = getFloatOr(obj, "ty", t->params.ty);
  t->params.sx = getFloatOr(obj, "sx", t->params.sx);
  t->params.sy = getFloatOr(obj, "sy", t->params.sy);
  recomputeMat3(*t);

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

CmdResult CommandProcessor::cmdAttachTransform(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id drawItemId = getIdOrZero(obj, "drawItemId");
  if (drawItemId == 0) {
    return fail("BAD_COMMAND", "attachTransform: missing/invalid drawItemId");
  }

  DrawItem* di = scene.getDrawItemMutable(drawItemId);
  if (!di) {
    return fail("MISSING_DRAWITEM", "attachTransform: drawItemId not found",
                std::string(R"({"drawItemId":)") + std::to_string(drawItemId) + "}");
  }

  const Id transformId = getIdOrZero(obj, "transformId");
  // 0 = detach
  if (transformId != 0 && !scene.hasTransform(transformId)) {
    return fail("NOT_FOUND", "attachTransform: transformId not found",
                std::string(R"({"transformId":)") + std::to_string(transformId) + "}");
  }

  di->transformId = transformId;

  CmdResult r;
  r.ok = true;
  r.createdId = 0;
  return r;
}

// -------------------- Buffer cache commands --------------------

CmdResult CommandProcessor::cmdBufferSetMaxBytes(const rapidjson::Value& obj) {
  if (!ingest_) {
    return fail("NO_INGEST", "bufferSetMaxBytes: no IngestProcessor attached");
  }
  const Id bufferId = getIdOrZero(obj, "bufferId");
  if (bufferId == 0) {
    return fail("BAD_COMMAND", "bufferSetMaxBytes: missing/invalid bufferId");
  }
  const auto* mb = getMember(obj, "maxBytes");
  if (!mb || !mb->IsUint()) {
    return fail("BAD_COMMAND", "bufferSetMaxBytes: missing uint maxBytes");
  }
  ingest_->setMaxBytes(bufferId, mb->GetUint());
  CmdResult r;
  r.ok = true;
  return r;
}

CmdResult CommandProcessor::cmdBufferEvictFront(const rapidjson::Value& obj) {
  if (!ingest_) {
    return fail("NO_INGEST", "bufferEvictFront: no IngestProcessor attached");
  }
  const Id bufferId = getIdOrZero(obj, "bufferId");
  if (bufferId == 0) {
    return fail("BAD_COMMAND", "bufferEvictFront: missing/invalid bufferId");
  }
  const auto* b = getMember(obj, "bytes");
  if (!b || !b->IsUint()) {
    return fail("BAD_COMMAND", "bufferEvictFront: missing uint bytes");
  }
  ingest_->evictFront(bufferId, b->GetUint());
  CmdResult r;
  r.ok = true;
  return r;
}

CmdResult CommandProcessor::cmdBufferKeepLast(const rapidjson::Value& obj) {
  if (!ingest_) {
    return fail("NO_INGEST", "bufferKeepLast: no IngestProcessor attached");
  }
  const Id bufferId = getIdOrZero(obj, "bufferId");
  if (bufferId == 0) {
    return fail("BAD_COMMAND", "bufferKeepLast: missing/invalid bufferId");
  }
  const auto* b = getMember(obj, "bytes");
  if (!b || !b->IsUint()) {
    return fail("BAD_COMMAND", "bufferKeepLast: missing uint bytes");
  }
  ingest_->keepLast(bufferId, b->GetUint());
  CmdResult r;
  r.ok = true;
  return r;
}

// -------------------- Draw item mutation --------------------

CmdResult CommandProcessor::cmdSetDrawItemPipeline(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id drawItemId = getIdOrZero(obj, "drawItemId");
  if (drawItemId == 0) {
    return fail("BAD_COMMAND", "setDrawItemPipeline: missing/invalid drawItemId");
  }

  DrawItem* di = scene.getDrawItemMutable(drawItemId);
  if (!di) {
    return fail("MISSING_DRAWITEM", "setDrawItemPipeline: drawItemId not found",
                std::string(R"({"drawItemId":)") + std::to_string(drawItemId) + "}");
  }

  const std::string pipeline = getStringOrEmpty(obj, "pipeline");
  if (!pipeline.empty()) {
    di->pipeline = pipeline;
  }

  const Id geomId = getIdOrZero(obj, "geometryId");
  if (geomId != 0) {
    di->geometryId = geomId;
  }

  if (!di->pipeline.empty() && di->geometryId != 0) {
    return validateDrawItem(*di);
  }

  CmdResult r;
  r.ok = true;
  return r;
}

CmdResult CommandProcessor::cmdSetGeometryVertexCount(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id geomId = getIdOrZero(obj, "geometryId");
  if (geomId == 0) {
    return fail("BAD_COMMAND", "setGeometryVertexCount: missing/invalid geometryId");
  }

  Geometry* g = scene.getGeometryMutable(geomId);
  if (!g) {
    return fail("NOT_FOUND", "setGeometryVertexCount: geometryId not found",
                std::string(R"({"geometryId":)") + std::to_string(geomId) + "}");
  }

  const auto* vc = getMember(obj, "vertexCount");
  if (!vc || !vc->IsUint()) {
    return fail("BAD_COMMAND", "setGeometryVertexCount: missing uint vertexCount");
  }

  g->vertexCount = vc->GetUint();

  CmdResult r;
  r.ok = true;
  return r;
}

CmdResult CommandProcessor::cmdSetGeometryBuffer(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id geomId = getIdOrZero(obj, "geometryId");
  if (geomId == 0) {
    return fail("BAD_COMMAND", "setGeometryBuffer: missing/invalid geometryId");
  }

  Geometry* g = scene.getGeometryMutable(geomId);
  if (!g) {
    return fail("NOT_FOUND", "setGeometryBuffer: geometryId not found",
                std::string(R"({"geometryId":)") + std::to_string(geomId) + "}");
  }

  const Id vb = getIdOrZero(obj, "vertexBufferId");
  if (vb == 0 || !scene.hasBuffer(vb)) {
    return fail("MISSING_BUFFER", "setGeometryBuffer: invalid vertexBufferId",
                std::string(R"({"vertexBufferId":)") + std::to_string(vb) + "}");
  }

  g->vertexBufferId = vb;

  CmdResult r;
  r.ok = true;
  return r;
}

// -------------------- Glyph atlas commands --------------------

CmdResult CommandProcessor::cmdEnsureGlyphs(const rapidjson::Value& obj) {
  if (!atlas_) {
    return fail("NO_ATLAS", "ensureGlyphs: no GlyphAtlas attached");
  }

  const std::string text = getStringOrEmpty(obj, "text");
  if (text.empty()) {
    return fail("BAD_COMMAND", "ensureGlyphs: missing/empty text field");
  }

  // Extract unique codepoints (ASCII for now)
  std::vector<std::uint32_t> cps;
  for (unsigned char ch : text) {
    cps.push_back(static_cast<std::uint32_t>(ch));
  }

  atlas_->ensureGlyphs(cps.data(), static_cast<std::uint32_t>(cps.size()));

  CmdResult r;
  r.ok = true;
  return r;
}

// -------------------- Draw item color --------------------

CmdResult CommandProcessor::cmdSetDrawItemColor(const rapidjson::Value& obj) {
  Scene& scene = curScene();

  const Id drawItemId = getIdOrZero(obj, "drawItemId");
  if (drawItemId == 0) {
    return fail("BAD_COMMAND", "setDrawItemColor: missing/invalid drawItemId");
  }

  DrawItem* di = scene.getDrawItemMutable(drawItemId);
  if (!di) {
    return fail("MISSING_DRAWITEM", "setDrawItemColor: drawItemId not found",
                std::string(R"({"drawItemId":)") + std::to_string(drawItemId) + "}");
  }

  di->color[0] = getFloatOr(obj, "r", di->color[0]);
  di->color[1] = getFloatOr(obj, "g", di->color[1]);
  di->color[2] = getFloatOr(obj, "b", di->color[2]);
  di->color[3] = getFloatOr(obj, "a", di->color[3]);

  CmdResult r;
  r.ok = true;
  return r;
}

// -------------------- Query --------------------

std::string CommandProcessor::listResourcesJson() const {
  // Important: list ACTIVE resources (renderer-visible), even if we're in a pending frame.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();

  w.Key("panes");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::Pane)) w.Uint64(id);
  w.EndArray();

  w.Key("layers");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::Layer)) w.Uint64(id);
  w.EndArray();

  w.Key("drawItems");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::DrawItem)) w.Uint64(id);
  w.EndArray();

  w.Key("buffers");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::Buffer)) w.Uint64(id);
  w.EndArray();

  w.Key("geometries");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::Geometry)) w.Uint64(id);
  w.EndArray();

  w.Key("transforms");
  w.StartArray();
  for (Id id : activeReg_.list(ResourceKind::Transform)) w.Uint64(id);
  w.EndArray();

  w.Key("activeFrameId");
  w.Uint64(activeFrameId_);

  w.Key("inFrame");
  w.Bool(inFrame_);

  w.Key("pendingFrameId");
  w.Uint64(inFrame_ ? pendingFrameId_ : 0);

  w.Key("frameFailed");
  w.Bool(inFrame_ ? frameFailed_ : false);

  w.EndObject();

  return sb.GetString();
}

} // namespace dc
