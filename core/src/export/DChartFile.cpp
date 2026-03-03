// D62: .dchart snapshot import/export
#include "dc/export/DChartFile.hpp"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>
#include <sstream>

namespace dc {

std::string DChartFileIO::serialize(const DChartFile& file) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> w(sb);

  w.StartObject();

  w.Key("formatVersion");
  w.Int(file.formatVersion);

  w.Key("sceneJSON");
  w.String(file.sceneJSON.c_str());

  w.Key("annotationsJSON");
  w.String(file.annotationsJSON.c_str());

  w.Key("themeJSON");
  w.String(file.themeJSON.c_str());

  w.Key("metadata");
  w.String(file.metadata.c_str());

  w.EndObject();

  return sb.GetString();
}

bool DChartFileIO::deserialize(const std::string& json, DChartFile& out) {
  rapidjson::Document doc;
  doc.Parse(json.c_str());
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }

  if (doc.HasMember("formatVersion") && doc["formatVersion"].IsInt()) {
    out.formatVersion = doc["formatVersion"].GetInt();
    if (out.formatVersion > 1) {
      return false; // unsupported format version
    }
  }

  if (doc.HasMember("sceneJSON") && doc["sceneJSON"].IsString()) {
    out.sceneJSON = doc["sceneJSON"].GetString();
  }

  if (doc.HasMember("annotationsJSON") && doc["annotationsJSON"].IsString()) {
    out.annotationsJSON = doc["annotationsJSON"].GetString();
  }

  if (doc.HasMember("themeJSON") && doc["themeJSON"].IsString()) {
    out.themeJSON = doc["themeJSON"].GetString();
  }

  if (doc.HasMember("metadata") && doc["metadata"].IsString()) {
    out.metadata = doc["metadata"].GetString();
  }

  return true;
}

bool DChartFileIO::save(const std::string& path, const DChartFile& file) {
  std::string json = serialize(file);
  std::ofstream ofs(path, std::ios::out | std::ios::trunc);
  if (!ofs.is_open()) return false;
  ofs << json;
  return ofs.good();
}

bool DChartFileIO::load(const std::string& path, DChartFile& out) {
  std::ifstream ifs(path, std::ios::in);
  if (!ifs.is_open()) return false;

  std::ostringstream ss;
  ss << ifs.rdbuf();
  std::string json = ss.str();

  return deserialize(json, out);
}

} // namespace dc
