// D60.1 -- PipelineRegistry: custom pipeline registration and lookup

#include "dc/pipelines/PipelineRegistry.hpp"

#include <cstdio>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

int main() {
  std::printf("=== D60.1 Custom Pipeline Tests ===\n");

  // Test 1: Built-in pipelines are still accessible through the registry
  {
    dc::PipelineRegistry reg;
    const dc::PipelineSpec* tri = reg.find("triSolid@1");
    check(tri != nullptr, "find triSolid@1 via registry");
    check(tri && tri->name == "triSolid", "triSolid name correct");
    check(tri && tri->drawMode == dc::DrawMode::Triangles, "triSolid draw mode");

    const dc::PipelineSpec* line = reg.find("line2d@1");
    check(line != nullptr, "find line2d@1 via registry");

    const dc::PipelineSpec* rect = reg.find("instancedRect@1");
    check(rect != nullptr, "find instancedRect@1 via registry");

    check(!reg.isCustom("triSolid@1"), "triSolid@1 is not custom");
    check(!reg.isCustom("line2d@1"), "line2d@1 is not custom");
  }

  // Test 2: Register a custom pipeline
  {
    dc::PipelineRegistry reg;

    dc::CustomPipelineConfig cfg;
    cfg.name = "heatmap";
    cfg.version = 1;
    cfg.vertexShaderSrc = "void main() { gl_Position = vec4(0); }";
    cfg.fragmentShaderSrc = "void main() { fragColor = vec4(1); }";
    cfg.vertexFormat = dc::VertexFormat::Pos2Color4;
    cfg.drawMode = dc::DrawMode::Triangles;
    cfg.uniformNames = {"u_intensity", "u_threshold"};

    bool ok = reg.registerCustom(cfg);
    check(ok, "registerCustom returns true");

    const dc::PipelineSpec* spec = reg.find("heatmap@1");
    check(spec != nullptr, "find custom heatmap@1");
    check(spec && spec->name == "heatmap", "custom name correct");
    check(spec && spec->version == 1, "custom version correct");
    check(spec && spec->requiredVertexFormat == dc::VertexFormat::Pos2Color4, "custom vertex format");
    check(spec && spec->drawMode == dc::DrawMode::Triangles, "custom draw mode");

    check(reg.isCustom("heatmap@1"), "heatmap@1 is custom");
  }

  // Test 3: Retrieve custom config
  {
    dc::PipelineRegistry reg;

    dc::CustomPipelineConfig cfg;
    cfg.name = "flowField";
    cfg.version = 2;
    cfg.vertexShaderSrc = "// vert";
    cfg.fragmentShaderSrc = "// frag";
    cfg.vertexFormat = dc::VertexFormat::Pos2_Clip;
    cfg.drawMode = dc::DrawMode::Points;
    cfg.uniformNames = {"u_time", "u_speed", "u_scale"};

    reg.registerCustom(cfg);

    const dc::CustomPipelineConfig* got = reg.getCustomConfig("flowField@2");
    check(got != nullptr, "getCustomConfig returns config");
    check(got && got->name == "flowField", "config name");
    check(got && got->version == 2, "config version");
    check(got && got->vertexShaderSrc == "// vert", "config vertex shader source");
    check(got && got->fragmentShaderSrc == "// frag", "config fragment shader source");
    check(got && got->uniformNames.size() == 3, "config has 3 uniform names");
    check(got && got->uniformNames[0] == "u_time", "first uniform is u_time");
    check(got && got->uniformNames[2] == "u_scale", "third uniform is u_scale");

    // Built-in has no custom config
    check(reg.getCustomConfig("triSolid@1") == nullptr, "built-in has no custom config");
  }

  // Test 4: Cannot override built-in
  {
    dc::PipelineRegistry reg;

    dc::CustomPipelineConfig cfg;
    cfg.name = "triSolid";
    cfg.version = 1;
    cfg.vertexShaderSrc = "// evil override";
    cfg.fragmentShaderSrc = "// evil override";

    bool ok = reg.registerCustom(cfg);
    check(!ok, "cannot override built-in triSolid@1");
    check(!reg.isCustom("triSolid@1"), "triSolid@1 still not custom");
  }

  // Test 5: Cannot register duplicate custom
  {
    dc::PipelineRegistry reg;

    dc::CustomPipelineConfig cfg;
    cfg.name = "myPipeline";
    cfg.version = 1;
    cfg.vertexShaderSrc = "// v1";
    cfg.fragmentShaderSrc = "// f1";

    check(reg.registerCustom(cfg), "first registration succeeds");
    check(!reg.registerCustom(cfg), "duplicate registration fails");

    // Different version is fine
    cfg.version = 2;
    check(reg.registerCustom(cfg), "different version succeeds");
  }

  // Test 6: customKeys()
  {
    dc::PipelineRegistry reg;

    check(reg.customKeys().empty(), "no custom keys initially");

    dc::CustomPipelineConfig c1;
    c1.name = "alpha";
    c1.version = 1;
    c1.vertexShaderSrc = "//";
    c1.fragmentShaderSrc = "//";
    reg.registerCustom(c1);

    dc::CustomPipelineConfig c2;
    c2.name = "beta";
    c2.version = 1;
    c2.vertexShaderSrc = "//";
    c2.fragmentShaderSrc = "//";
    reg.registerCustom(c2);

    auto keys = reg.customKeys();
    check(keys.size() == 2, "2 custom keys registered");

    // Check both keys are present (order not guaranteed)
    bool hasAlpha = false, hasBeta = false;
    for (auto& k : keys) {
      if (k == "alpha@1") hasAlpha = true;
      if (k == "beta@1") hasBeta = true;
    }
    check(hasAlpha, "customKeys contains alpha@1");
    check(hasBeta, "customKeys contains beta@1");
  }

  // Test 7: Unknown key returns nullptr
  {
    dc::PipelineRegistry reg;
    check(reg.find("nonexistent@1") == nullptr, "unknown key returns nullptr");
    check(!reg.isCustom("nonexistent@1"), "unknown key is not custom");
    check(reg.getCustomConfig("nonexistent@1") == nullptr, "unknown has no config");
  }

  std::printf("=== D60.1: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
