#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <iostream>
#include <string>

static void requireOk(const dc::CmdResult& r, const char* what) {
  if (!r.ok) {
    std::cerr << "FAIL: " << what
              << " code=" << r.err.code
              << " msg=" << r.err.message
              << " details=" << r.err.details << "\n";
    std::exit(1);
  }
}

static void requireFailCode(const dc::CmdResult& r, const char* code, const char* what) {
  if (r.ok) {
    std::cerr << "FAIL: expected failure for " << what << "\n";
    std::exit(1);
  }
  if (r.err.code != code) {
    std::cerr << "FAIL: expected code=" << code
              << " got=" << r.err.code
              << " for " << what << "\n";
    std::exit(1);
  }
}

int main() {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // Active baseline: create one pane.
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"Main"})"), "createPane");
  if (!scene.hasPane(1)) {
    std::cerr << "FAIL: expected paneId=1 to exist\n";
    return 1;
  }

  // Start a transaction frame.
  requireOk(cp.applyJsonText(R"({"cmd":"beginFrame","frameId":123})"), "beginFrame");

  // Bad command inside the frame: invalid paneId -> should fail and poison the frame.
  auto bad = cp.applyJsonText(R"({"cmd":"createLayer","paneId":999,"name":"L_BAD"})");
  requireFailCode(bad, "VALIDATION_INVALID_PARENT", "bad createLayer");

  // Any subsequent commands are rejected (strict mode).
  auto afterFail = cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L_GOOD"})");
  requireFailCode(afterFail, "FRAME_REJECTED", "command after frame failure");

  // Commit must reject and NOT change active scene.
  auto commit = cp.applyJsonText(R"({"cmd":"commitFrame","frameId":123})");
  requireFailCode(commit, "FRAME_REJECTED", "commitFrame after failure");

  // Active state must be unchanged:
  // - Pane still exists
  // - No layers were added
  if (!scene.hasPane(1)) {
    std::cerr << "FAIL: active pane missing after rejected commit\n";
    return 1;
  }
  if (!scene.layerIds().empty()) {
    std::cerr << "FAIL: active layers changed after rejected commit\n";
    return 1;
  }
  if (!reg.list(dc::ResourceKind::Layer).empty()) {
    std::cerr << "FAIL: registry layers changed after rejected commit\n";
    return 1;
  }

  std::cout << "D1.3 atomic frame PASS\n";
  return 0;
}
