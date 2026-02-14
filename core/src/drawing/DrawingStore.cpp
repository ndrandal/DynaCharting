#include "dc/drawing/DrawingStore.hpp"
#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace dc {

std::uint32_t DrawingStore::addTrendline(double x0, double y0, double x1, double y1) {
  Drawing d;
  d.id = nextId_++;
  d.type = DrawingType::Trendline;
  d.x0 = x0; d.y0 = y0;
  d.x1 = x1; d.y1 = y1;
  drawings_.push_back(d);
  return d.id;
}

std::uint32_t DrawingStore::addHorizontalLevel(double price) {
  Drawing d;
  d.id = nextId_++;
  d.type = DrawingType::HorizontalLevel;
  d.y0 = price;
  // x0/x1 not used; line extends full width
  drawings_.push_back(d);
  return d.id;
}

std::uint32_t DrawingStore::addVerticalLine(double x) {
  Drawing d;
  d.id = nextId_++;
  d.type = DrawingType::VerticalLine;
  d.x0 = x;
  // y0/x1/y1 not used; line extends full height
  drawings_.push_back(d);
  return d.id;
}

std::uint32_t DrawingStore::addRectangle(double x0, double y0, double x1, double y1) {
  Drawing d;
  d.id = nextId_++;
  d.type = DrawingType::Rectangle;
  d.x0 = x0; d.y0 = y0;
  d.x1 = x1; d.y1 = y1;
  drawings_.push_back(d);
  return d.id;
}

std::uint32_t DrawingStore::addFibRetracement(double x0, double y0, double x1, double y1) {
  Drawing d;
  d.id = nextId_++;
  d.type = DrawingType::FibRetracement;
  d.x0 = x0; d.y0 = y0;
  d.x1 = x1; d.y1 = y1;
  drawings_.push_back(d);
  return d.id;
}

void DrawingStore::setColor(std::uint32_t id, float r, float g, float b, float a) {
  for (auto& d : drawings_) {
    if (d.id == id) {
      d.color[0] = r; d.color[1] = g;
      d.color[2] = b; d.color[3] = a;
      return;
    }
  }
}

void DrawingStore::setLineWidth(std::uint32_t id, float width) {
  for (auto& d : drawings_) {
    if (d.id == id) {
      d.lineWidth = width;
      return;
    }
  }
}

void DrawingStore::remove(std::uint32_t id) {
  drawings_.erase(
    std::remove_if(drawings_.begin(), drawings_.end(),
      [id](const Drawing& d) { return d.id == id; }),
    drawings_.end());
}

void DrawingStore::clear() {
  drawings_.clear();
}

const Drawing* DrawingStore::get(std::uint32_t id) const {
  for (const auto& d : drawings_) {
    if (d.id == id) return &d;
  }
  return nullptr;
}

// D19.2: Serialization

std::string DrawingStore::toJSON() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();
  w.Key("drawings");
  w.StartArray();
  for (const auto& d : drawings_) {
    w.StartObject();
    w.Key("id");      w.Uint(d.id);
    w.Key("type");    w.Uint(static_cast<unsigned>(d.type));
    w.Key("x0");      w.Double(d.x0);
    w.Key("y0");      w.Double(d.y0);
    w.Key("x1");      w.Double(d.x1);
    w.Key("y1");      w.Double(d.y1);
    w.Key("color");
    w.StartArray();
    for (int i = 0; i < 4; ++i) w.Double(static_cast<double>(d.color[i]));
    w.EndArray();
    w.Key("lineWidth"); w.Double(static_cast<double>(d.lineWidth));
    w.EndObject();
  }
  w.EndArray();
  w.EndObject();

  return sb.GetString();
}

bool DrawingStore::loadJSON(const std::string& json) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError()) return false;
  if (!doc.IsObject()) return false;
  if (!doc.HasMember("drawings") || !doc["drawings"].IsArray()) return false;

  const auto& arr = doc["drawings"].GetArray();

  std::vector<Drawing> loaded;
  loaded.reserve(arr.Size());
  std::uint32_t maxId = 0;

  for (const auto& v : arr) {
    if (!v.IsObject()) return false;
    Drawing d;

    if (v.HasMember("id") && v["id"].IsUint())
      d.id = v["id"].GetUint();
    else
      return false;

    if (v.HasMember("type") && v["type"].IsUint())
      d.type = static_cast<DrawingType>(v["type"].GetUint());

    if (v.HasMember("x0") && v["x0"].IsNumber()) d.x0 = v["x0"].GetDouble();
    if (v.HasMember("y0") && v["y0"].IsNumber()) d.y0 = v["y0"].GetDouble();
    if (v.HasMember("x1") && v["x1"].IsNumber()) d.x1 = v["x1"].GetDouble();
    if (v.HasMember("y1") && v["y1"].IsNumber()) d.y1 = v["y1"].GetDouble();

    if (v.HasMember("color") && v["color"].IsArray()) {
      const auto& ca = v["color"].GetArray();
      for (unsigned i = 0; i < 4 && i < ca.Size(); ++i) {
        if (ca[i].IsNumber())
          d.color[i] = static_cast<float>(ca[i].GetDouble());
      }
    }

    if (v.HasMember("lineWidth") && v["lineWidth"].IsNumber())
      d.lineWidth = static_cast<float>(v["lineWidth"].GetDouble());

    if (d.id > maxId) maxId = d.id;
    loaded.push_back(d);
  }

  drawings_ = std::move(loaded);
  nextId_ = maxId + 1;
  return true;
}

} // namespace dc
