#include "dc/pipelines/PipelineCatalog.hpp"

namespace dc {

PipelineCatalog::PipelineCatalog() {
  auto reg = [&](const char* n, int v, VertexFormat fmt, DrawMode dm, uint32_t vpi) {
    PipelineSpec s;
    s.name = n;
    s.version = v;
    s.requiredVertexFormat = fmt;
    s.drawMode = dm;
    s.verticesPerInstance = vpi;
    specs_.emplace(pipelineKey(s.name, s.version), std::move(s));
  };

  reg("triSolid",        1, VertexFormat::Pos2_Clip, DrawMode::Triangles,          0);
  reg("line2d",          1, VertexFormat::Pos2_Clip, DrawMode::Lines,              0);
  reg("points",          1, VertexFormat::Pos2_Clip, DrawMode::Points,             0);
  reg("instancedRect",   1, VertexFormat::Rect4,    DrawMode::InstancedTriangles,  6);
  reg("instancedCandle", 1, VertexFormat::Candle6,  DrawMode::InstancedTriangles, 12);
}

const PipelineSpec* PipelineCatalog::find(const std::string& key) const {
  auto it = specs_.find(key);
  return it == specs_.end() ? nullptr : &it->second;
}

} // namespace dc
