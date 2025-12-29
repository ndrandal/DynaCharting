#pragma once
#include "dc/scene/Geometry.hpp"
#include <string>
#include <unordered_map>

namespace dc {

struct PipelineSpec {
  std::string name;   // "triSolid"
  int version{1};     // 1
  VertexFormat requiredVertexFormat{VertexFormat::Pos2_Clip};
};

inline std::string pipelineKey(const std::string& name, int version) {
  return name + "@" + std::to_string(version);
}

class PipelineCatalog {
public:
  PipelineCatalog();

  const PipelineSpec* find(const std::string& key) const;

private:
  std::unordered_map<std::string, PipelineSpec> specs_;
};

} // namespace dc
