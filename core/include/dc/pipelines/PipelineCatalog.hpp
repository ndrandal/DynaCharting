#pragma once
#include "dc/scene/Geometry.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

enum class DrawMode : std::uint8_t {
  Triangles          = 0,
  Lines              = 1,
  Points             = 2,
  InstancedTriangles = 3
};

struct PipelineSpec {
  std::string name;   // "triSolid"
  int version{1};     // 1
  VertexFormat requiredVertexFormat{VertexFormat::Pos2_Clip};
  DrawMode drawMode{DrawMode::Triangles};
  std::uint32_t verticesPerInstance{0}; // 0=non-instanced, 6=rect, 12=candle
};

inline std::string pipelineKey(const std::string& name, int version) {
  return name + "@" + std::to_string(version);
}

class PipelineCatalog {
public:
  PipelineCatalog();

  const PipelineSpec* find(const std::string& key) const;

  // Enumerate every registered pipeline key (e.g. "instancedCandle@1"), sorted for
  // a stable, reproducible order. Lets a tool (the AI grammar card, ENC-620c) emit
  // the pipeline->required-vertex-format table DIRECTLY from the catalog so it can
  // never drift from the registered set.
  std::vector<std::string> keys() const;

private:
  std::unordered_map<std::string, PipelineSpec> specs_;
};

} // namespace dc
