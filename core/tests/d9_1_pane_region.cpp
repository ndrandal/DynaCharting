// D9.1 — Pane region test (pure C++, no GL)
// Tests: default region, setPaneRegion command, partial update, error on non-existent pane.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void requireClose(float a, float b, float eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // --- Test 1: Default region ---
  {
    auto r = cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P1"})");
    requireOk(r, "createPane");

    const dc::Pane* p = scene.getPane(1);
    requireTrue(p != nullptr, "pane exists");
    requireClose(p->region.clipYMin, -1.0f, 1e-6f, "default clipYMin");
    requireClose(p->region.clipYMax, 1.0f, 1e-6f, "default clipYMax");
    requireClose(p->region.clipXMin, -1.0f, 1e-6f, "default clipXMin");
    requireClose(p->region.clipXMax, 1.0f, 1e-6f, "default clipXMax");
    std::printf("  Test 1 (default region) PASS\n");
  }

  // --- Test 2: setPaneRegion with all four fields ---
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":-0.5,"clipYMax":0.5,"clipXMin":-0.9,"clipXMax":0.9})");
    requireOk(r, "setPaneRegion all fields");

    const dc::Pane* p = scene.getPane(1);
    requireClose(p->region.clipYMin, -0.5f, 1e-6f, "clipYMin updated");
    requireClose(p->region.clipYMax, 0.5f, 1e-6f, "clipYMax updated");
    requireClose(p->region.clipXMin, -0.9f, 1e-6f, "clipXMin updated");
    requireClose(p->region.clipXMax, 0.9f, 1e-6f, "clipXMax updated");
    std::printf("  Test 2 (full update) PASS\n");
  }

  // --- Test 3: Partial field update (only clipYMin) ---
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":1,"clipYMin":-0.8})");
    requireOk(r, "setPaneRegion partial");

    const dc::Pane* p = scene.getPane(1);
    requireClose(p->region.clipYMin, -0.8f, 1e-6f, "clipYMin partial");
    requireClose(p->region.clipYMax, 0.5f, 1e-6f, "clipYMax unchanged");
    requireClose(p->region.clipXMin, -0.9f, 1e-6f, "clipXMin unchanged");
    requireClose(p->region.clipXMax, 0.9f, 1e-6f, "clipXMax unchanged");
    std::printf("  Test 3 (partial update) PASS\n");
  }

  // --- Test 4: setPaneRegion on non-existent pane ---
  {
    auto r = cp.applyJsonText(
      R"({"cmd":"setPaneRegion","id":999,"clipYMin":0.0})");
    requireTrue(!r.ok, "setPaneRegion non-existent should fail");
    requireTrue(r.err.code == "NOT_FOUND", "error code NOT_FOUND");
    std::printf("  Test 4 (non-existent pane) PASS\n");
  }

  std::printf("D9.1 pane region: ALL PASS\n");
  return 0;
}
