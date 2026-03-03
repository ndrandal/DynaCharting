#include "dc/data/LodManager.hpp"

namespace dc {

void LodManager::setConfig(const LodManagerConfig& cfg) {
  config_ = cfg;
}

void LodManager::setGroup(const LodGroup& group) {
  groups_[group.drawItemId] = group;
  // Initialize to -1 (no level selected) if not already tracked
  if (currentLevels_.find(group.drawItemId) == currentLevels_.end()) {
    currentLevels_[group.drawItemId] = -1;
  }
}

void LodManager::removeGroup(Id drawItemId) {
  groups_.erase(drawItemId);
  currentLevels_.erase(drawItemId);
}

void LodManager::update(float pixelsPerDataUnit, Scene& scene) {
  for (auto& [drawItemId, group] : groups_) {
    if (group.levels.empty()) continue;

    // Find the appropriate level: first level where pixelsPerDataUnit >= minPixelsPerUnit
    // (levels are sorted descending by threshold)
    int bestLevel = -1;
    for (int i = 0; i < static_cast<int>(group.levels.size()); ++i) {
      if (pixelsPerDataUnit >= group.levels[i].minPixelsPerUnit) {
        bestLevel = i;
        break;
      }
    }

    // If no level matched, use the last (lowest threshold) level
    if (bestLevel < 0) {
      bestLevel = static_cast<int>(group.levels.size()) - 1;
    }

    int current = currentLevels_[drawItemId];

    // Apply hysteresis: only switch if we've crossed the threshold
    // by the hysteresis fraction beyond the boundary
    if (current >= 0 && current != bestLevel) {
      float currentThreshold = group.levels[current].minPixelsPerUnit;
      float targetThreshold = group.levels[bestLevel].minPixelsPerUnit;

      if (bestLevel < current) {
        // Switching to higher detail (lower index, higher threshold):
        // require pixelsPerDataUnit >= targetThreshold * (1 - hysteresis)
        float required = targetThreshold * (1.0f - config_.hysteresis);
        if (pixelsPerDataUnit < required) {
          bestLevel = current; // stay at current level
        }
      } else {
        // Switching to lower detail (higher index, lower threshold):
        // only switch if we've dropped well below the current level's threshold
        float dropRequired = currentThreshold * (1.0f - config_.hysteresis);
        if (pixelsPerDataUnit >= dropRequired) {
          bestLevel = current; // stay at current level
        }
      }
    }

    if (bestLevel != current) {
      currentLevels_[drawItemId] = bestLevel;

      // Update the DrawItem's geometryId via scene
      DrawItem* di = scene.getDrawItemMutable(drawItemId);
      if (di) {
        di->geometryId = group.levels[bestLevel].geometryId;
      }
    }
  }
}

int LodManager::currentLevel(Id drawItemId) const {
  auto it = currentLevels_.find(drawItemId);
  if (it != currentLevels_.end()) return it->second;
  return -1;
}

} // namespace dc
