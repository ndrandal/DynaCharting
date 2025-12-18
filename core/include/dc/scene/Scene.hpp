#pragma once
#include "dc/scene/Types.hpp"
#include <unordered_map>
#include <vector>

namespace dc {

// Persistent scene state.
// For D1.1 it’s just a graph: Pane -> Layer -> DrawItem.
class Scene {
public:
  bool hasPane(Id id) const;
  bool hasLayer(Id id) const;
  bool hasDrawItem(Id id) const;

  const Pane*     getPane(Id id) const;
  const Layer*    getLayer(Id id) const;
  const DrawItem* getDrawItem(Id id) const;

  // Create (caller ensures IDs are unique / valid in registry)
  void addPane(Pane p);
  void addLayer(Layer l);
  void addDrawItem(DrawItem d);

  // Delete (cascades) — returns full list of deleted IDs (empty if nothing deleted).
  // - deletePane => {paneId, layerIds..., drawItemIds...}
  // - deleteLayer => {layerId, drawItemIds...}
  // - deleteDrawItem => {drawItemId}
  std::vector<Id> deletePane(Id paneId);
  std::vector<Id> deleteLayer(Id layerId);
  std::vector<Id> deleteDrawItem(Id drawItemId);

  // Enumeration for listResources()
  std::vector<Id> paneIds() const;
  std::vector<Id> layerIds() const;
  std::vector<Id> drawItemIds() const;

private:
  std::unordered_map<Id, Pane> panes_;
  std::unordered_map<Id, Layer> layers_;
  std::unordered_map<Id, DrawItem> drawItems_;
};

} // namespace dc
