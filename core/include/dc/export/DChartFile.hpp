#pragma once
// D62: .dchart snapshot import/export
#include <string>

namespace dc {

struct DChartFile {
  int formatVersion{1};
  std::string sceneJSON;
  std::string annotationsJSON;
  std::string themeJSON;
  std::string metadata;
};

class DChartFileIO {
public:
  static std::string serialize(const DChartFile& file);
  static bool deserialize(const std::string& json, DChartFile& out);
  static bool save(const std::string& path, const DChartFile& file);
  static bool load(const std::string& path, DChartFile& out);
};

} // namespace dc
