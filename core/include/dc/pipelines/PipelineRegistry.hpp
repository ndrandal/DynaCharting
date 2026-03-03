#pragma once
#include "dc/pipelines/PipelineCatalog.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// D60: Configuration for user-defined custom pipelines.
// Carries shader source and vertex format so that the GL backend
// can compile/link on first use.
struct CustomPipelineConfig {
  std::string name;
  int version{1};
  std::string vertexShaderSrc;
  std::string fragmentShaderSrc;
  VertexFormat vertexFormat{VertexFormat::Pos2_Clip};
  DrawMode drawMode{DrawMode::Triangles};
  std::vector<std::string> uniformNames;
};

// D60: Wraps the built-in PipelineCatalog and adds registration of
// custom (user-defined) pipeline types at runtime.
class PipelineRegistry {
public:
  PipelineRegistry();

  // Lookup — checks custom specs first, then falls through to built-ins.
  const PipelineSpec* find(const std::string& key) const;

  // Register a custom pipeline.  Returns true on success, false if the
  // key already exists (either built-in or previously registered custom).
  bool registerCustom(const CustomPipelineConfig& config);

  // Query whether a key was registered as custom (not built-in).
  bool isCustom(const std::string& key) const;

  // Retrieve the full custom config (shader source, uniforms, etc.).
  // Returns nullptr for built-in or unknown keys.
  const CustomPipelineConfig* getCustomConfig(const std::string& key) const;

  // List all custom pipeline keys.
  std::vector<std::string> customKeys() const;

private:
  PipelineCatalog builtIn_;
  std::unordered_map<std::string, PipelineSpec> customSpecs_;
  std::unordered_map<std::string, CustomPipelineConfig> customConfigs_;
};

} // namespace dc
