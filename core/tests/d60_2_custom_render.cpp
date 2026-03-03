// D60.2 -- PipelineRegistry: custom pipeline with shader sources,
//          verifying registration + lookup (pure C++, no actual GL dispatch)

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
  std::printf("=== D60.2 Custom Render Registration Tests ===\n");

  dc::PipelineRegistry reg;

  // Register a custom pipeline with realistic shader sources
  dc::CustomPipelineConfig cfg;
  cfg.name = "wireframe";
  cfg.version = 1;
  cfg.vertexShaderSrc =
    "#version 330 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "uniform mat3 u_transform;\n"
    "void main() {\n"
    "  vec3 p = u_transform * vec3(a_pos, 1.0);\n"
    "  gl_Position = vec4(p.xy, 0.0, 1.0);\n"
    "}\n";
  cfg.fragmentShaderSrc =
    "#version 330 core\n"
    "out vec4 fragColor;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "  fragColor = u_color;\n"
    "}\n";
  cfg.vertexFormat = dc::VertexFormat::Pos2_Clip;
  cfg.drawMode = dc::DrawMode::Lines;
  cfg.uniformNames = {"u_transform", "u_color"};

  // Test 1: Registration succeeds
  {
    bool ok = reg.registerCustom(cfg);
    check(ok, "wireframe@1 registered");
  }

  // Test 2: Spec is findable and correct
  {
    const dc::PipelineSpec* spec = reg.find("wireframe@1");
    check(spec != nullptr, "wireframe@1 found");
    check(spec && spec->name == "wireframe", "spec name");
    check(spec && spec->version == 1, "spec version");
    check(spec && spec->requiredVertexFormat == dc::VertexFormat::Pos2_Clip, "spec vertex format");
    check(spec && spec->drawMode == dc::DrawMode::Lines, "spec draw mode is Lines");
  }

  // Test 3: Custom config preserves shader sources
  {
    const dc::CustomPipelineConfig* cc = reg.getCustomConfig("wireframe@1");
    check(cc != nullptr, "custom config exists");
    check(cc && cc->vertexShaderSrc.find("#version 330") != std::string::npos,
          "vertex shader has #version 330");
    check(cc && cc->fragmentShaderSrc.find("fragColor") != std::string::npos,
          "fragment shader has fragColor");
    check(cc && cc->uniformNames.size() == 2, "2 uniforms");
    check(cc && cc->uniformNames[0] == "u_transform", "first uniform");
    check(cc && cc->uniformNames[1] == "u_color", "second uniform");
  }

  // Test 4: Custom pipeline coexists with all built-ins
  {
    check(reg.find("triSolid@1") != nullptr, "triSolid@1 still accessible");
    check(reg.find("line2d@1") != nullptr, "line2d@1 still accessible");
    check(reg.find("points@1") != nullptr, "points@1 still accessible");
    check(reg.find("instancedRect@1") != nullptr, "instancedRect@1 still accessible");
    check(reg.find("instancedCandle@1") != nullptr, "instancedCandle@1 still accessible");
    check(reg.find("textSDF@1") != nullptr, "textSDF@1 still accessible");
    check(reg.find("lineAA@1") != nullptr, "lineAA@1 still accessible");
    check(reg.find("triAA@1") != nullptr, "triAA@1 still accessible");
    check(reg.find("triGradient@1") != nullptr, "triGradient@1 still accessible");
    check(reg.find("texturedQuad@1") != nullptr, "texturedQuad@1 still accessible");
  }

  // Test 5: Multiple custom pipelines
  {
    dc::CustomPipelineConfig cfg2;
    cfg2.name = "particles";
    cfg2.version = 1;
    cfg2.vertexShaderSrc = "// particle vert";
    cfg2.fragmentShaderSrc = "// particle frag";
    cfg2.vertexFormat = dc::VertexFormat::Pos2Color4;
    cfg2.drawMode = dc::DrawMode::Points;
    cfg2.uniformNames = {"u_time", "u_gravity"};

    check(reg.registerCustom(cfg2), "particles@1 registered");

    auto keys = reg.customKeys();
    check(keys.size() == 2, "2 custom pipelines total");

    check(reg.isCustom("wireframe@1"), "wireframe@1 is custom");
    check(reg.isCustom("particles@1"), "particles@1 is custom");
    check(!reg.isCustom("triSolid@1"), "triSolid@1 is not custom");
  }

  // Test 6: Custom with instanced draw mode
  {
    dc::CustomPipelineConfig cfg3;
    cfg3.name = "customInstanced";
    cfg3.version = 1;
    cfg3.vertexShaderSrc = "// inst vert";
    cfg3.fragmentShaderSrc = "// inst frag";
    cfg3.vertexFormat = dc::VertexFormat::Rect4;
    cfg3.drawMode = dc::DrawMode::InstancedTriangles;

    check(reg.registerCustom(cfg3), "customInstanced@1 registered");

    const dc::PipelineSpec* spec = reg.find("customInstanced@1");
    check(spec != nullptr, "customInstanced@1 found");
    check(spec && spec->drawMode == dc::DrawMode::InstancedTriangles,
          "custom instanced draw mode correct");
    check(spec && spec->requiredVertexFormat == dc::VertexFormat::Rect4,
          "custom instanced vertex format correct");
  }

  std::printf("=== D60.2: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
