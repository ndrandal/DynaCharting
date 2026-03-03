#pragma once
#include "dc/ids/Id.hpp"
#include "dc/scene/Scene.hpp"
#include <vector>
#include <unordered_map>

namespace dc {

struct LodLevel {
  float minPixelsPerUnit;
  Id geometryId;
};

struct LodGroup {
  Id drawItemId;
  std::vector<LodLevel> levels; // sorted by threshold descending
};

struct LodManagerConfig {
  float hysteresis{0.1f}; // fraction, prevents thrashing
};

class LodManager {
public:
  void setConfig(const LodManagerConfig& cfg);
  void setGroup(const LodGroup& group);
  void removeGroup(Id drawItemId);
  void update(float pixelsPerDataUnit, Scene& scene);
  int currentLevel(Id drawItemId) const;

private:
  LodManagerConfig config_;
  std::unordered_map<Id, LodGroup> groups_;
  std::unordered_map<Id, int> currentLevels_;
};

} // namespace dc
