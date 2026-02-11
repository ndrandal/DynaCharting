#pragma once
#include "dc/scene/Types.hpp"
#include "dc/scene/Geometry.hpp"

#include <unordered_map>
#include <vector>

namespace dc {

class Scene {
public:
  bool hasPane(Id id) const;
  bool hasLayer(Id id) const;
  bool hasDrawItem(Id id) const;
  bool hasBuffer(Id id) const;
  bool hasGeometry(Id id) const;
  bool hasTransform(Id id) const;

  const Pane*     getPane(Id id) const;
  const Layer*    getLayer(Id id) const;
  const DrawItem* getDrawItem(Id id) const;
  const Buffer*   getBuffer(Id id) const;
  const Geometry* getGeometry(Id id) const;
  const Transform* getTransform(Id id) const;
  Transform* getTransformMutable(Id id);

  // Mutable access
  DrawItem* getDrawItemMutable(Id id);
  Buffer* getBufferMutable(Id id);

  // Create
  void addPane(Pane p);
  void addLayer(Layer l);
  void addDrawItem(DrawItem d);
  void addBuffer(Buffer b);
  void addGeometry(Geometry g);
  void addTransform(Transform t);

  // Delete (cascades) — returns full list of deleted IDs (empty if nothing deleted).
  std::vector<Id> deletePane(Id paneId);
  std::vector<Id> deleteLayer(Id layerId);
  std::vector<Id> deleteDrawItem(Id drawItemId);

  // Minimal non-cascading deletes for new resources
  std::vector<Id> deleteBuffer(Id bufferId);
  std::vector<Id> deleteGeometry(Id geometryId);
  std::vector<Id> deleteTransform(Id transformId);

  // Enumeration for listResources()
  std::vector<Id> paneIds() const;
  std::vector<Id> layerIds() const;
  std::vector<Id> drawItemIds() const;
  std::vector<Id> bufferIds() const;
  std::vector<Id> geometryIds() const;
  std::vector<Id> transformIds() const;

private:
  std::unordered_map<Id, Pane> panes_;
  std::unordered_map<Id, Layer> layers_;
  std::unordered_map<Id, DrawItem> drawItems_;
  std::unordered_map<Id, Buffer> buffers_;
  std::unordered_map<Id, Geometry> geometries_;
  std::unordered_map<Id, Transform> transforms_;
};

} // namespace dc
