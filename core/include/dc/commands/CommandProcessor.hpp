#pragma once
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/ids/Id.hpp"

#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace dc {

struct CmdResult {
  bool ok{true};
  std::string error;

  // For create commands, we return the created ID (0 if none).
  Id createdId{0};
};

class CommandProcessor {
public:
  CommandProcessor(Scene& scene, ResourceRegistry& registry);

  // Apply a single JSON command object.
  CmdResult applyJson(const rapidjson::Value& obj);

  // Convenience: parse string then apply.
  CmdResult applyJsonText(const std::string& jsonText);

  // For pass criteria: query resources and see persistence.
  // Returns a JSON string (for logging / tests).
  std::string listResourcesJson() const;

private:
  Scene& scene_;
  ResourceRegistry& reg_;

  bool inFrame_{false};
  std::uint64_t frameCounter_{0};

  // ---- handlers ----
  CmdResult cmdHello(const rapidjson::Value& obj);
  CmdResult cmdBeginFrame(const rapidjson::Value& obj);
  CmdResult cmdCommitFrame(const rapidjson::Value& obj);

  CmdResult cmdCreatePane(const rapidjson::Value& obj);
  CmdResult cmdCreateLayer(const rapidjson::Value& obj);
  CmdResult cmdCreateDrawItem(const rapidjson::Value& obj);

  CmdResult cmdDelete(const rapidjson::Value& obj);

  // helpers
  static const rapidjson::Value* getMember(const rapidjson::Value& obj, const char* key);
  static std::string getStringOrEmpty(const rapidjson::Value& obj, const char* key);
  static Id getIdOrZero(const rapidjson::Value& obj, const char* key);
};

} // namespace dc
