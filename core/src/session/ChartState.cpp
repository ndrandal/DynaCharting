#include "dc/session/ChartState.hpp"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace dc {

std::string serializeChartState(const ChartState& state) {
  rapidjson::Document doc(rapidjson::kObjectType);
  auto& alloc = doc.GetAllocator();

  doc.AddMember("version",
                rapidjson::Value(state.version.c_str(), alloc), alloc);

  // Viewport
  rapidjson::Value vp(rapidjson::kObjectType);
  vp.AddMember("xMin", state.viewport.xMin, alloc);
  vp.AddMember("xMax", state.viewport.xMax, alloc);
  vp.AddMember("yMin", state.viewport.yMin, alloc);
  vp.AddMember("yMax", state.viewport.yMax, alloc);
  doc.AddMember("viewport", vp, alloc);

  // Drawings — parse and embed as nested object
  if (!state.drawingsJSON.empty()) {
    rapidjson::Document drawDoc;
    drawDoc.Parse(state.drawingsJSON.c_str());
    if (!drawDoc.HasParseError()) {
      rapidjson::Value drawCopy(drawDoc, alloc);
      doc.AddMember("drawings", drawCopy, alloc);
    }
  }

  doc.AddMember("theme",
                rapidjson::Value(state.themeName.c_str(), alloc), alloc);
  doc.AddMember("symbol",
                rapidjson::Value(state.symbol.c_str(), alloc), alloc);
  doc.AddMember("timeframe",
                rapidjson::Value(state.timeframe.c_str(), alloc), alloc);

  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  doc.Accept(writer);
  return sb.GetString();
}

bool deserializeChartState(const std::string& json, ChartState& out) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) return false;

  // Version
  if (doc.HasMember("version") && doc["version"].IsString())
    out.version = doc["version"].GetString();

  // Viewport
  if (doc.HasMember("viewport") && doc["viewport"].IsObject()) {
    const auto& vp = doc["viewport"];
    if (vp.HasMember("xMin") && vp["xMin"].IsNumber())
      out.viewport.xMin = vp["xMin"].GetDouble();
    if (vp.HasMember("xMax") && vp["xMax"].IsNumber())
      out.viewport.xMax = vp["xMax"].GetDouble();
    if (vp.HasMember("yMin") && vp["yMin"].IsNumber())
      out.viewport.yMin = vp["yMin"].GetDouble();
    if (vp.HasMember("yMax") && vp["yMax"].IsNumber())
      out.viewport.yMax = vp["yMax"].GetDouble();
  }

  // Drawings — re-serialize the embedded object back to a JSON string
  if (doc.HasMember("drawings") && doc["drawings"].IsObject()) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc["drawings"].Accept(writer);
    out.drawingsJSON = sb.GetString();
  }

  // Theme
  if (doc.HasMember("theme") && doc["theme"].IsString())
    out.themeName = doc["theme"].GetString();

  // Symbol
  if (doc.HasMember("symbol") && doc["symbol"].IsString())
    out.symbol = doc["symbol"].GetString();

  // Timeframe
  if (doc.HasMember("timeframe") && doc["timeframe"].IsString())
    out.timeframe = doc["timeframe"].GetString();

  return true;
}

} // namespace dc
