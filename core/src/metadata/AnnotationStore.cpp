#include "dc/metadata/AnnotationStore.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace dc {

void AnnotationStore::set(Id drawItemId, const std::string& role,
                           const std::string& label, const std::string& value) {
  annotations_[drawItemId] = {drawItemId, role, label, value};
}

void AnnotationStore::remove(Id drawItemId) {
  annotations_.erase(drawItemId);
}

const Annotation* AnnotationStore::get(Id drawItemId) const {
  auto it = annotations_.find(drawItemId);
  return (it != annotations_.end()) ? &it->second : nullptr;
}

std::vector<Annotation> AnnotationStore::findByRole(const std::string& role) const {
  std::vector<Annotation> result;
  for (const auto& [id, ann] : annotations_) {
    if (ann.role == role) result.push_back(ann);
  }
  return result;
}

std::vector<Annotation> AnnotationStore::all() const {
  std::vector<Annotation> result;
  result.reserve(annotations_.size());
  for (const auto& [id, ann] : annotations_) {
    result.push_back(ann);
  }
  return result;
}

std::size_t AnnotationStore::count() const {
  return annotations_.size();
}

void AnnotationStore::clear() {
  annotations_.clear();
}

std::string AnnotationStore::toJSON() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);
  w.StartArray();
  for (const auto& [id, ann] : annotations_) {
    w.StartObject();
    w.Key("drawItemId"); w.Uint64(ann.drawItemId);
    w.Key("role"); w.String(ann.role.c_str());
    w.Key("label"); w.String(ann.label.c_str());
    w.Key("value"); w.String(ann.value.c_str());
    w.EndObject();
  }
  w.EndArray();
  return sb.GetString();
}

void AnnotationStore::loadJSON(const std::string& json) {
  rapidjson::Document d;
  d.Parse(json.c_str());
  if (!d.IsArray()) return;

  annotations_.clear();
  for (auto& v : d.GetArray()) {
    if (!v.IsObject()) continue;
    Annotation ann;
    if (v.HasMember("drawItemId") && v["drawItemId"].IsUint64())
      ann.drawItemId = static_cast<Id>(v["drawItemId"].GetUint64());
    if (v.HasMember("role") && v["role"].IsString())
      ann.role = v["role"].GetString();
    if (v.HasMember("label") && v["label"].IsString())
      ann.label = v["label"].GetString();
    if (v.HasMember("value") && v["value"].IsString())
      ann.value = v["value"].GetString();
    if (ann.drawItemId != 0) {
      annotations_[ann.drawItemId] = ann;
    }
  }
}

} // namespace dc
