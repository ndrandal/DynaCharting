#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scene/Geometry.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// A single JSON command string to be applied via CommandProcessor.
using CmdString = std::string;

// Declares a data feed that a recipe consumes via LiveIngestLoop.
struct DataSubscription {
  Id bufferId;
  Id geometryId;
  VertexFormat format;
};

// Result of building a recipe — the commands to create/dispose it.
struct RecipeBuildResult {
  std::vector<CmdString> createCommands;
  std::vector<CmdString> disposeCommands; // applied in reverse to tear down
  std::vector<DataSubscription> subscriptions;
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

protected:
  Id idBase_;

  // Deterministic ID: idBase_ + offset
  Id rid(std::uint32_t offset) const {
    return idBase_ + static_cast<Id>(offset);
  }
};

} // namespace dc
