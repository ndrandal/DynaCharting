#include "dc/pipelines/PipelineRegistry.hpp"

namespace dc {

PipelineRegistry::PipelineRegistry() = default;

const PipelineSpec* PipelineRegistry::find(const std::string& key) const {
  // Custom specs take priority.
  auto it = customSpecs_.find(key);
  if (it != customSpecs_.end()) return &it->second;
  // Fall through to built-in catalog.
  return builtIn_.find(key);
}

bool PipelineRegistry::registerCustom(const CustomPipelineConfig& config) {
  std::string key = pipelineKey(config.name, config.version);

  // Reject if the key already exists as built-in.
  if (builtIn_.find(key)) return false;

  // Reject if already registered as custom.
  if (customSpecs_.count(key)) return false;

  PipelineSpec spec;
  spec.name = config.name;
  spec.version = config.version;
  spec.requiredVertexFormat = config.vertexFormat;
  spec.drawMode = config.drawMode;
  spec.verticesPerInstance = 0; // custom pipelines default to non-instanced

  customSpecs_.emplace(key, std::move(spec));
  customConfigs_.emplace(key, config);
  return true;
}

bool PipelineRegistry::isCustom(const std::string& key) const {
  return customConfigs_.count(key) > 0;
}

const CustomPipelineConfig* PipelineRegistry::getCustomConfig(const std::string& key) const {
  auto it = customConfigs_.find(key);
  return it == customConfigs_.end() ? nullptr : &it->second;
}

std::vector<std::string> PipelineRegistry::customKeys() const {
  std::vector<std::string> keys;
  keys.reserve(customConfigs_.size());
  for (auto& kv : customConfigs_) {
    keys.push_back(kv.first);
  }
  return keys;
}

} // namespace dc
