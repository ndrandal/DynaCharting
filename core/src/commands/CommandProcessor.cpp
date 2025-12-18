#include "dc/commands/CommandProcessor.hpp"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <stdexcept>

namespace dc {

CommandProcessor::CommandProcessor(Scene& scene, ResourceRegistry& registry)
  : scene_(scene), reg_(registry) {}

const rapidjson::Value* CommandProcessor::getMember(const rapidjson::Value& obj, const char* key) {
  if (!obj.IsObject()) return nullptr;
  auto it = obj.FindMember(key);
  if (it == obj.MemberEnd()) return nullptr;
  return &it->value;
}

std::string CommandProcessor::getStringOrEmpty(const rapidjson::Value& obj, const char* key) {
  auto* v = getMember(obj, key);
  if (!v) return {};
  if (v->IsString()) return v->GetString();
  return {};
}

Id CommandProcessor::getIdOrZero(const rapidjson::Value& obj, const char* key) {
  auto* v = getMember(obj, key);
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
    return {false, "CommandProcessor: invalid JSON object", 0};
  }
  return applyJson(d);
}

CmdResult CommandProcessor::applyJson(const rapidjson::Value& obj) {
  auto* cmdV = getMember(obj, "cmd");
  if (!cmdV || !cmdV->IsString()) return {false, "Missing string field: cmd", 0};

  const std::string cmd = cmdV->GetString();

  if (cmd == "hello") return cmdHello(obj);
  if (cmd == "beginFrame") return cmdBeginFrame(obj);
  if (cmd == "commitFrame") return cmdCommitFrame(obj);

  if (cmd == "createPane") return cmdCreatePane(obj);
  if (cmd == "createLayer") return cmdCreateLayer(obj);
  if (cmd == "createDrawItem") return cmdCreateDrawItem(obj);

  if (cmd == "delete") return cmdDelete(obj);

  return {false, "Unknown cmd: " + cmd, 0};
}

CmdResult CommandProcessor::cmdHello(const rapidjson::Value&) {
  // no-op, just proves parsing + dispatch works
  return {true, "", 0};
}

CmdResult CommandProcessor::cmdBeginFrame(const rapidjson::Value&) {
  if (inFrame_) return {false, "beginFrame: already in frame", 0};
  inFrame_ = true;
  frameCounter_++;
  return {true, "", 0};
}

CmdResult CommandProcessor::cmdCommitFrame(const rapidjson::Value&) {
  if (!inFrame_) return {false, "commitFrame: not in frame", 0};
  inFrame_ = false;
  return {true, "", 0};
}

CmdResult CommandProcessor::cmdCreatePane(const rapidjson::Value& obj) {
  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Pane)) return {false, "createPane: id already exists", 0};
  } else {
    id = reg_.allocate(ResourceKind::Pane);
  }

  Pane p;
  p.id = id;
  p.name = getStringOrEmpty(obj, "name");

  scene_.addPane(std::move(p));
  return {true, "", id};
}

CmdResult CommandProcessor::cmdCreateLayer(const rapidjson::Value& obj) {
  const Id paneId = getIdOrZero(obj, "paneId");
  if (paneId == 0 || !scene_.hasPane(paneId)) return {false, "createLayer: invalid paneId", 0};

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::Layer)) return {false, "createLayer: id already exists", 0};
  } else {
    id = reg_.allocate(ResourceKind::Layer);
  }

  Layer l;
  l.id = id;
  l.paneId = paneId;
  l.name = getStringOrEmpty(obj, "name");

  scene_.addLayer(std::move(l));
  return {true, "", id};
}

CmdResult CommandProcessor::cmdCreateDrawItem(const rapidjson::Value& obj) {
  const Id layerId = getIdOrZero(obj, "layerId");
  if (layerId == 0 || !scene_.hasLayer(layerId)) return {false, "createDrawItem: invalid layerId", 0};

  Id id = getIdOrZero(obj, "id");
  if (id != 0) {
    if (!reg_.reserve(id, ResourceKind::DrawItem)) return {false, "createDrawItem: id already exists", 0};
  } else {
    id = reg_.allocate(ResourceKind::DrawItem);
  }

  DrawItem d;
  d.id = id;
  d.layerId = layerId;
  d.name = getStringOrEmpty(obj, "name");

  scene_.addDrawItem(std::move(d));
  return {true, "", id};
}

CmdResult CommandProcessor::cmdDelete(const rapidjson::Value& obj) {
  const Id id = getIdOrZero(obj, "id");
  if (id == 0) return {false, "delete: missing/invalid id", 0};

  if (!reg_.exists(id)) return {false, "delete: id does not exist", 0};

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
    default:
      deleted.clear();
      break;
  }

  if (deleted.empty()) return {false, "delete: failed", 0};

  // Keep registry consistent with Scene (including cascades).
  for (Id did : deleted) {
    reg_.release(did);
  }

  return {true, "", 0};
}

std::string CommandProcessor::listResourcesJson() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();

  // Registry is the ID source of truth for this deliverable.
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

  w.Key("frame");
  w.Uint64(frameCounter_);

  w.Key("inFrame");
  w.Bool(inFrame_);

  w.EndObject();

  return sb.GetString();
}


} // namespace dc
