#include "dc/pipelines/PipelineCatalog.hpp"

namespace dc {

PipelineCatalog::PipelineCatalog() {
  // triSolid@1
  PipelineSpec tri;
  tri.name = "triSolid";
  tri.version = 1;
  tri.requiredVertexFormat = VertexFormat::Pos2_Clip;
  specs_.emplace(pipelineKey(tri.name, tri.version), std::move(tri));
}

const PipelineSpec* PipelineCatalog::find(const std::string& key) const {
  auto it = specs_.find(key);
  return it == specs_.end() ? nullptr : &it->second;
}

} // namespace dc
