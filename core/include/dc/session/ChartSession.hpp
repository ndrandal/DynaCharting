#pragma once
#include "dc/ids/Id.hpp"
#include "dc/recipe/Recipe.hpp"
#include "dc/data/LiveIngestLoop.hpp"
#include "dc/data/AggregationManager.hpp"
#include "dc/scene/Geometry.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dc {

class CommandProcessor;
class IngestProcessor;
class Viewport;
class DataSource;

using RecipeHandle = std::uint32_t;

struct RetentionPolicy {
  std::uint32_t maxBytesPerBuffer{4 * 1024 * 1024};
};

struct SmartRetentionConfig {
  float retentionMultiplier{3.0f};     // keep 3× visible data worth of bytes
  std::uint32_t minRetention{64 * 1024};    // floor: 64KB
  std::uint32_t maxRetention{8 * 1024 * 1024}; // ceiling: 8MB
};

struct ChartSessionConfig {
  RetentionPolicy retention;
  AggregationManagerConfig aggregation;
  SmartRetentionConfig smartRetention;
  bool enableAggregation{false};
  bool enableSmartRetention{false};
};

struct FrameResult {
  std::vector<Id> touchedBufferIds;
  bool dataChanged{false};
  bool viewportChanged{false};
  bool resolutionChanged{false};
};

class ChartSession {
public:
  ChartSession(CommandProcessor& cp, IngestProcessor& ingest);

  void setConfig(const ChartSessionConfig& cfg);
  void setViewport(Viewport* vp);

  // Mount: applies createCommands, sets up LiveIngestLoop bindings,
  // attaches shared transform to all drawItemIds, applies retention caps.
  RecipeHandle mount(std::unique_ptr<Recipe> recipe, Id sharedTransformId = 0);

  // Unmount: applies disposeCommands, removes bindings.
  void unmount(RecipeHandle handle);
  void unmountAll();

  bool isMounted(RecipeHandle handle) const;
  Recipe* getRecipe(RecipeHandle handle);

  // Compute recipe wiring: caller-provided callback returns
  // IDs of buffers that were modified by the compute step.
  void setComputeCallback(RecipeHandle handle,
                          std::function<std::vector<Id>()> cb);
  void addComputeDependency(RecipeHandle handle, Id upstreamBufferId);

  // Per-frame update: drains data, runs compute callbacks, syncs transforms.
  FrameResult update(DataSource& source);

  // Issue setTransform command for a managed transform.
  void syncTransform(Id transformId);

  // Access the internal loop (for tests / advanced callers).
  LiveIngestLoop& loop() { return loop_; }

private:
  struct MountedSlot {
    std::unique_ptr<Recipe> recipe;
    RecipeBuildResult buildResult;
    Id sharedTransformId{0};
    std::function<std::vector<Id>()> computeCallback;
  };

  void rebuildBindings();

  void setupAggregation(const MountedSlot& slot);

  CommandProcessor& cp_;
  IngestProcessor& ingest_;
  LiveIngestLoop loop_;
  AggregationManager aggManager_;
  Viewport* viewport_{nullptr};
  ChartSessionConfig config_;

  RecipeHandle nextHandle_{1};
  std::unordered_map<RecipeHandle, MountedSlot> slots_;

  // upstream buffer ID -> dependent recipe handles
  std::unordered_map<Id, std::vector<RecipeHandle>> computeDeps_;

  // All managed transform IDs (from sharedTransformId on mount)
  std::unordered_set<Id> managedTransforms_;
};

} // namespace dc
