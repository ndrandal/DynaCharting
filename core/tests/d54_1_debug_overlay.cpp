// D54.1 — DebugOverlay: command generation and disposal
#include "dc/debug/DebugOverlay.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdio>
#include <string>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    ++failed;
  }
}

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
  std::printf("=== D54.1 DebugOverlay Tests ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Build a simple scene: 1 pane, 1 layer, 1 drawItem with boundsValid geometry.
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Main"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "drawItem");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"), "geom");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"), "bind");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setGeometryBounds","geometryId":100,"minX":-0.5,"minY":-0.5,"maxX":0.5,"maxY":0.5})"), "bounds");

  // Add a transform for axis visualization.
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":200})"), "transform");
  requireOk(cp.applyJsonText(
    R"({"cmd":"setTransform","id":200,"tx":0.2,"ty":0.3,"sx":1.0,"sy":1.0})"), "setTx");
  requireOk(cp.applyJsonText(R"({"cmd":"attachTransform","drawItemId":3,"transformId":200})"), "attach");

  // Test 1: Generate debug overlay commands with all features enabled.
  dc::DebugOverlay overlay;
  dc::DebugOverlayConfig config;
  config.showBounds = true;
  config.showPaneRegions = true;
  config.showTransformAxes = true;

  auto cmds = overlay.generateCommands(scene, config, 800, 600);
  check(!cmds.empty(), "generateCommands returns non-empty");
  std::printf("    debug commands generated: %zu\n", cmds.size());

  // Test 2: debugItemCount should be non-zero after generation.
  check(overlay.debugItemCount() > 0, "debugItemCount > 0 after generate");
  std::printf("    debug item count: %zu\n", overlay.debugItemCount());

  // Test 3: Commands should contain createPane and createLayer for debug.
  bool hasPaneCmd = false;
  bool hasLayerCmd = false;
  bool hasDrawItemCmd = false;
  for (const auto& c : cmds) {
    if (c.find("\"createPane\"") != std::string::npos) hasPaneCmd = true;
    if (c.find("\"createLayer\"") != std::string::npos) hasLayerCmd = true;
    if (c.find("\"createDrawItem\"") != std::string::npos) hasDrawItemCmd = true;
  }
  check(hasPaneCmd, "commands contain createPane");
  check(hasLayerCmd, "commands contain createLayer");
  check(hasDrawItemCmd, "commands contain createDrawItem");

  // Test 4: Commands should contain line2d@1 pipeline binding.
  bool hasLine2d = false;
  for (const auto& c : cmds) {
    if (c.find("\"line2d@1\"") != std::string::npos) hasLine2d = true;
  }
  check(hasLine2d, "commands contain line2d@1 pipeline");

  // Test 5: Dispose returns delete commands.
  auto disposeCmds = overlay.disposeCommands();
  check(!disposeCmds.empty(), "disposeCommands returns non-empty");
  bool hasDeleteCmd = false;
  for (const auto& c : disposeCmds) {
    if (c.find("\"delete\"") != std::string::npos) hasDeleteCmd = true;
  }
  check(hasDeleteCmd, "dispose commands contain delete");

  // Test 6: After dispose, debugItemCount is 0.
  check(overlay.debugItemCount() == 0, "debugItemCount is 0 after dispose");

  // Test 7: Generate with only pane regions enabled.
  {
    dc::DebugOverlay overlay2;
    dc::DebugOverlayConfig cfg2;
    cfg2.showBounds = false;
    cfg2.showTransformAxes = false;
    cfg2.showPaneRegions = true;
    auto cmds2 = overlay2.generateCommands(scene, cfg2, 800, 600);
    check(!cmds2.empty(), "pane-only commands are non-empty");
    // Expect: createPane + createLayer + (createBuffer + createGeometry + createDrawItem + bind + color) per pane
    // 1 pane -> 2 + 5 = 7 commands
    std::printf("    pane-only commands: %zu\n", cmds2.size());
    check(cmds2.size() >= 7, "pane-only has at least 7 commands (1 pane wireframe)");
  }

  // Test 8: Generate with empty scene.
  {
    dc::Scene emptyScene;
    dc::DebugOverlay overlay3;
    auto cmds3 = overlay3.generateCommands(emptyScene, config, 800, 600);
    // Should still contain pane + layer creation, but no wireframes
    check(cmds3.size() >= 2, "empty scene still creates debug pane + layer");
  }

  // Test 9: Double-generate cleans up previous.
  {
    dc::DebugOverlay overlay4;
    auto cmds4a = overlay4.generateCommands(scene, config, 800, 600);
    std::size_t count1 = overlay4.debugItemCount();
    auto cmds4b = overlay4.generateCommands(scene, config, 800, 600);
    // Should include dispose commands from first run + new commands.
    bool hasCleanup = false;
    for (const auto& c : cmds4b) {
      if (c.find("\"delete\"") != std::string::npos) { hasCleanup = true; break; }
    }
    check(hasCleanup, "re-generate includes cleanup from previous");
    (void)count1;
    (void)cmds4a;
  }

  // Test 10: Custom debugIdBase.
  {
    dc::DebugOverlay overlay5;
    dc::DebugOverlayConfig cfg5;
    cfg5.debugIdBase = 500000;
    cfg5.showBounds = true;
    cfg5.showPaneRegions = true;
    cfg5.showTransformAxes = false;
    auto cmds5 = overlay5.generateCommands(scene, cfg5, 800, 600);
    bool hasCustomId = false;
    for (const auto& c : cmds5) {
      if (c.find("500000") != std::string::npos) { hasCustomId = true; break; }
    }
    check(hasCustomId, "custom debugIdBase appears in commands");
  }

  std::printf("=== D54.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
