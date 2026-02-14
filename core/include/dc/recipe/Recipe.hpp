#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scene/Geometry.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace dc {

// A single JSON command string to be applied via CommandProcessor.
using CmdString = std::string;

// Declares a data feed that a recipe consumes via LiveIngestLoop.
struct DataSubscription {
  Id bufferId;
  Id geometryId;
  VertexFormat format;
  Id drawItemId{0};  // D8.5: for aggregation rebinding
};

// Result of building a recipe — the commands to create/dispose it.
struct RecipeBuildResult {
  std::vector<CmdString> createCommands;
  std::vector<CmdString> disposeCommands; // applied in reverse to tear down
  std::vector<DataSubscription> subscriptions;
};

// D14.3: Series metadata for legend display + visibility control.
struct SeriesInfo {
  std::string name;
  float colorHint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  bool defaultVisible{true};
  std::vector<Id> drawItemIds;  // DrawItems to toggle for visibility
};

// Base class for all recipes. A recipe translates a declarative description
// into engine commands using deterministic ID allocation (idBase + offset).
class Recipe {
public:
  explicit Recipe(Id idBase) : idBase_(idBase) {}
  virtual ~Recipe() = default;

  Id idBase() const { return idBase_; }

  // Build the recipe: produce create + dispose commands + subscriptions.
  virtual RecipeBuildResult build() const = 0;

  // Return IDs of all DrawItems created by this recipe.
  virtual std::vector<Id> drawItemIds() const { return {}; }

  // D14.3: Return series metadata for legend display.
  virtual std::vector<SeriesInfo> seriesInfoList() const { return {}; }

protected:
  Id idBase_;

  // Deterministic ID: idBase_ + offset
  Id rid(std::uint32_t offset) const {
    return idBase_ + static_cast<Id>(offset);
  }
};

} // namespace dc
