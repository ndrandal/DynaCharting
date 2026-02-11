#include "dc/scene/Scene.hpp"
#include <algorithm>

namespace dc {

bool Scene::hasPane(Id id) const     { return panes_.find(id) != panes_.end(); }
bool Scene::hasLayer(Id id) const    { return layers_.find(id) != layers_.end(); }
bool Scene::hasDrawItem(Id id) const { return drawItems_.find(id) != drawItems_.end(); }
bool Scene::hasBuffer(Id id) const   { return buffers_.find(id) != buffers_.end(); }
bool Scene::hasGeometry(Id id) const { return geometries_.find(id) != geometries_.end(); }
bool Scene::hasTransform(Id id) const { return transforms_.find(id) != transforms_.end(); }

const Pane* Scene::getPane(Id id) const {
  auto it = panes_.find(id);
  return it == panes_.end() ? nullptr : &it->second;
}
const Layer* Scene::getLayer(Id id) const {
  auto it = layers_.find(id);
  return it == layers_.end() ? nullptr : &it->second;
}
const DrawItem* Scene::getDrawItem(Id id) const {
  auto it = drawItems_.find(id);
  return it == drawItems_.end() ? nullptr : &it->second;
}
const Buffer* Scene::getBuffer(Id id) const {
  auto it = buffers_.find(id);
  return it == buffers_.end() ? nullptr : &it->second;
}
const Geometry* Scene::getGeometry(Id id) const {
  auto it = geometries_.find(id);
  return it == geometries_.end() ? nullptr : &it->second;
}
const Transform* Scene::getTransform(Id id) const {
  auto it = transforms_.find(id);
  return it == transforms_.end() ? nullptr : &it->second;
}
Transform* Scene::getTransformMutable(Id id) {
  auto it = transforms_.find(id);
  return it == transforms_.end() ? nullptr : &it->second;
}
DrawItem* Scene::getDrawItemMutable(Id id) {
  auto it = drawItems_.find(id);
  return it == drawItems_.end() ? nullptr : &it->second;
}
Buffer* Scene::getBufferMutable(Id id) {
  auto it = buffers_.find(id);
  return it == buffers_.end() ? nullptr : &it->second;
}
Geometry* Scene::getGeometryMutable(Id id) {
  auto it = geometries_.find(id);
  return it == geometries_.end() ? nullptr : &it->second;
}


void Scene::addPane(Pane p)         { panes_[p.id] = std::move(p); }
void Scene::addLayer(Layer l)       { layers_[l.id] = std::move(l); }
void Scene::addDrawItem(DrawItem d) { drawItems_[d.id] = std::move(d); }
void Scene::addBuffer(Buffer b)      { buffers_[b.id] = std::move(b); }
void Scene::addGeometry(Geometry g)  { geometries_[g.id] = std::move(g); }
void Scene::addTransform(Transform t) { transforms_[t.id] = std::move(t); }

std::vector<Id> Scene::deleteDrawItem(Id drawItemId) {
  auto it = drawItems_.find(drawItemId);
  if (it == drawItems_.end()) return {};
  drawItems_.erase(it);
  return {drawItemId};
}

std::vector<Id> Scene::deleteLayer(Id layerId) {
  auto it = layers_.find(layerId);
  if (it == layers_.end()) return {};

  std::vector<Id> deleted;
  deleted.reserve(16);
  deleted.push_back(layerId);

  // cascade delete draw items
  std::vector<Id> toDelete;
  toDelete.reserve(16);
  for (auto& kv : drawItems_) {
    if (kv.second.layerId == layerId) toDelete.push_back(kv.first);
  }

  for (Id id : toDelete) {
    drawItems_.erase(id);
    deleted.push_back(id);
  }

  layers_.erase(it);
  return deleted;
}

std::vector<Id> Scene::deletePane(Id paneId) {
  auto it = panes_.find(paneId);
  if (it == panes_.end()) return {};

  std::vector<Id> deleted;
  deleted.reserve(32);
  deleted.push_back(paneId);

  // cascade delete layers (and their draw items)
  std::vector<Id> layersToDelete;
  layersToDelete.reserve(16);
  for (auto& kv : layers_) {
    if (kv.second.paneId == paneId) layersToDelete.push_back(kv.first);
  }

  for (Id lid : layersToDelete) {
    auto layerDeleted = deleteLayer(lid);
    // layerDeleted includes lid + drawItems
    deleted.insert(deleted.end(), layerDeleted.begin(), layerDeleted.end());
  }

  panes_.erase(it);
  return deleted;
}
std::vector<Id> Scene::deleteBuffer(Id bufferId) {
  auto it = buffers_.find(bufferId);
  if (it == buffers_.end()) return {};
  buffers_.erase(it);
  return {bufferId};
}

std::vector<Id> Scene::deleteGeometry(Id geometryId) {
  auto it = geometries_.find(geometryId);
  if (it == geometries_.end()) return {};
  geometries_.erase(it);
  return {geometryId};
}

std::vector<Id> Scene::deleteTransform(Id transformId) {
  auto it = transforms_.find(transformId);
  if (it == transforms_.end()) return {};
  transforms_.erase(it);
  return {transformId};
}



std::vector<Id> Scene::paneIds() const {
  std::vector<Id> out;
  out.reserve(panes_.size());
  for (auto& kv : panes_) out.push_back(kv.first);
  return out;
}
std::vector<Id> Scene::layerIds() const {
  std::vector<Id> out;
  out.reserve(layers_.size());
  for (auto& kv : layers_) out.push_back(kv.first);
  return out;
}
std::vector<Id> Scene::drawItemIds() const {
  std::vector<Id> out;
  out.reserve(drawItems_.size());
  for (auto& kv : drawItems_) out.push_back(kv.first);
  return out;
}
std::vector<Id> Scene::bufferIds() const {
  std::vector<Id> out;
  out.reserve(buffers_.size());
  for (auto& kv : buffers_) out.push_back(kv.first);
  return out;
}
std::vector<Id> Scene::geometryIds() const {
  std::vector<Id> out;
  out.reserve(geometries_.size());
  for (auto& kv : geometries_) out.push_back(kv.first);
  return out;
}

std::vector<Id> Scene::transformIds() const {
  std::vector<Id> out;
  out.reserve(transforms_.size());
  for (auto& kv : transforms_) out.push_back(kv.first);
  return out;
}

} // namespace dc
