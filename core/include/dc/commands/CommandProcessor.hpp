#pragma once
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/ids/Id.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <rapidjson/document.h>

namespace dc { class IngestProcessor; class GlyphAtlas; }

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

  // Optional: wire up IngestProcessor for cache-policy commands.
  void setIngestProcessor(IngestProcessor* ingest);

  // Optional: wire up GlyphAtlas for ensureGlyphs command.
  void setGlyphAtlas(GlyphAtlas* atlas);

  // Apply a single JSON command object.
  CmdResult applyJson(const rapidjson::Value& obj);

  // Convenience: parse string then apply.
  CmdResult applyJsonText(const std::string& jsonText);

  // For pass criteria: query resources and see persistence.
  // Returns a JSON string (for logging / tests).
  std::string listResourcesJson() const;

private:
  // Active (renderer-visible) state is owned by caller.
  Scene& activeScene_;
  ResourceRegistry& activeReg_;

  // Pending state exists only inside beginFrame/commitFrame.
  Scene pendingScene_{};
  ResourceRegistry pendingReg_{};

  bool inFrame_{false};
  bool frameFailed_{false};
  std::uint64_t activeFrameId_{0};
  std::uint64_t pendingFrameId_{0};

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

  CmdResult cmdCreateTransform(const rapidjson::Value& obj);
  CmdResult cmdSetTransform(const rapidjson::Value& obj);
  CmdResult cmdAttachTransform(const rapidjson::Value& obj);

  CmdResult cmdBufferSetMaxBytes(const rapidjson::Value& obj);
  CmdResult cmdBufferEvictFront(const rapidjson::Value& obj);
  CmdResult cmdBufferKeepLast(const rapidjson::Value& obj);
  CmdResult cmdSetDrawItemPipeline(const rapidjson::Value& obj);
  CmdResult cmdSetGeometryVertexCount(const rapidjson::Value& obj);
  CmdResult cmdSetGeometryBuffer(const rapidjson::Value& obj);
  CmdResult cmdEnsureGlyphs(const rapidjson::Value& obj);
  CmdResult cmdSetDrawItemColor(const rapidjson::Value& obj);
  CmdResult cmdSetDrawItemStyle(const rapidjson::Value& obj);
  CmdResult cmdSetPaneRegion(const rapidjson::Value& obj);
  CmdResult cmdSetPaneClearColor(const rapidjson::Value& obj);
  CmdResult cmdSetGeometryBounds(const rapidjson::Value& obj);
  CmdResult cmdSetDrawItemVisible(const rapidjson::Value& obj);

  PipelineCatalog catalog_;
  IngestProcessor* ingest_{nullptr};
  GlyphAtlas* atlas_{nullptr};

  // helpers
  static const rapidjson::Value* getMember(const rapidjson::Value& obj, const char* key);
  static std::string getStringOrEmpty(const rapidjson::Value& obj, const char* key);
  static Id getIdOrZero(const rapidjson::Value& obj, const char* key);
  static std::uint64_t getU64OrZero(const rapidjson::Value& obj, const char* key);
  static float getFloatOr(const rapidjson::Value& obj, const char* key, float def);

  static CmdResult fail(const std::string& code,
                        const std::string& message,
                        const std::string& detailsJson = "{}");

  CmdResult validateDrawItem(const DrawItem& di) const;

  // Transaction routing
  Scene& curScene();
  ResourceRegistry& curReg();
  const Scene& curScene() const;
  const ResourceRegistry& curReg() const;

  // If we're in a frame and it already failed, block additional mutations.
  CmdResult rejectIfFrameFailed(const char* cmdName) const;
};

} // namespace dc
