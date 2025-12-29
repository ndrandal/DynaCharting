#pragma once
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/ids/Id.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"

#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace dc {

struct CmdError {
  std::string code;     // e.g. "VALIDATION_MISSING_GEOMETRY"
  std::string message;  // human text
  std::string details;  // small JSON string with fields (kept minimal for now)
};

struct CmdResult {
  bool ok{true};
  CmdError err{};
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

  CmdResult cmdCreateBuffer(const rapidjson::Value& obj);
  CmdResult cmdCreateGeometry(const rapidjson::Value& obj);
  CmdResult cmdBindDrawItem(const rapidjson::Value& obj);

  PipelineCatalog catalog_;



  // helpers
  static const rapidjson::Value* getMember(const rapidjson::Value& obj, const char* key);
  static std::string getStringOrEmpty(const rapidjson::Value& obj, const char* key);
  static Id getIdOrZero(const rapidjson::Value& obj, const char* key);
  static CmdResult fail(const std::string& code,
                      const std::string& message,
                      const std::string& detailsJson = "{}");
  CmdResult validateDrawItem(const DrawItem& di) const;


};

} // namespace dc
