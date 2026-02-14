// D9.5 — Multi-viewport session test (pure C++, no GL)
// Tests: per-pane viewports sync transforms, X-axis linking, backward compat.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/data/FakeDataSource.hpp"
#include "dc/recipe/CandleRecipe.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

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

static void requireClose(double a, double b, double eps, const char* msg) {
  if (std::fabs(a - b) > eps) {
    std::fprintf(stderr, "ASSERT FAIL: %s (got %.6f, expected %.6f)\n", msg, a, b);
    std::exit(1);
  }
}

int main() {
  // --- Test 1: Two panes with different transforms ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    dc::ChartSession session(cp, ingest);
    cp.setIngestProcessor(&ingest);

    // Create panes and transforms
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane 1");
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2})"), "createPane 2");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":100})"), "createTransform 100");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":200})"), "createTransform 200");

    dc::Viewport vp1, vp2;
    vp1.setPixelViewport(400, 150);
    vp1.setDataRange(0, 100, 0, 50);
    vp2.setPixelViewport(400, 150);
    vp2.setDataRange(0, 100, 0, 200);

    session.addPaneViewport(1, &vp1, 100);
    session.addPaneViewport(2, &vp2, 200);

    // Need a data source for update — use FakeDataSource
    dc::FakeDataSource ds({});

    auto result = session.update(ds);
    requireTrue(result.viewportChanged, "viewportChanged");

    // Both transforms should be set
    const dc::Transform* t100 = scene.getTransform(100);
    const dc::Transform* t200 = scene.getTransform(200);
    requireTrue(t100 != nullptr, "transform 100 exists");
    requireTrue(t200 != nullptr, "transform 200 exists");

    // Verify they have different sy values (different Y ranges)
    auto tp1 = vp1.computeTransformParams();
    auto tp2 = vp2.computeTransformParams();
    requireClose(t100->params.sy, tp1.sy, 1e-5, "t100 sy matches vp1");
    requireClose(t200->params.sy, tp2.sy, 1e-5, "t200 sy matches vp2");

    std::printf("  Test 1 (two panes different transforms) PASS\n");
  }

  // --- Test 2: linkXAxis: pan primary → all viewports get X-range updated ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    dc::ChartSession session(cp, ingest);
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane 1");
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2})"), "createPane 2");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":100})"), "createTransform 100");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":200})"), "createTransform 200");

    dc::Viewport vp1, vp2;
    vp1.setPixelViewport(400, 150);
    vp1.setDataRange(0, 100, 0, 50);
    vp2.setPixelViewport(400, 150);
    vp2.setDataRange(0, 100, 0, 200);

    session.addPaneViewport(1, &vp1, 100);
    session.addPaneViewport(2, &vp2, 200);
    session.setLinkXAxis(true);

    // Pan primary viewport X range
    vp1.setDataRange(10, 110, 0, 50);

    dc::FakeDataSource ds({});
    session.update(ds);

    // vp2 should have X range updated to match vp1
    requireClose(vp2.dataRange().xMin, 10.0, 1e-5, "vp2 xMin linked");
    requireClose(vp2.dataRange().xMax, 110.0, 1e-5, "vp2 xMax linked");
    // Y should be unchanged
    requireClose(vp2.dataRange().yMin, 0.0, 1e-5, "vp2 yMin unchanged");
    requireClose(vp2.dataRange().yMax, 200.0, 1e-5, "vp2 yMax unchanged");

    std::printf("  Test 2 (linkXAxis) PASS\n");
  }

  // --- Test 3: Single setViewport() still works (backward compat) ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    dc::ChartSession session(cp, ingest);
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane 1");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":100})"), "createTransform 100");

    dc::Viewport vp;
    vp.setPixelViewport(400, 300);
    vp.setDataRange(0, 100, 0, 50);
    session.setViewport(&vp);

    // Mount a recipe so there's a managed transform
    dc::CandleRecipeConfig rcfg;
    rcfg.paneId = 1;
    auto recipe = std::make_unique<dc::CandleRecipe>(1000, rcfg);
    session.mount(std::move(recipe), 100);

    dc::FakeDataSource ds({});
    session.update(ds);

    const dc::Transform* t = scene.getTransform(100);
    requireTrue(t != nullptr, "transform 100 exists");
    auto tp = vp.computeTransformParams();
    requireClose(t->params.sx, tp.sx, 1e-5, "t100 sx matches single vp");

    std::printf("  Test 3 (backward compat single viewport) PASS\n");
  }

  // --- Test 4: removePaneViewport + re-add lifecycle ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    dc::ChartSession session(cp, ingest);
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane 1");
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":2})"), "createPane 2");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":100})"), "createTransform 100");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":200})"), "createTransform 200");

    dc::Viewport vp1, vp2;
    vp1.setPixelViewport(400, 150);
    vp1.setDataRange(0, 100, 0, 50);
    vp2.setPixelViewport(400, 150);
    vp2.setDataRange(0, 100, 0, 200);

    session.addPaneViewport(1, &vp1, 100);
    session.addPaneViewport(2, &vp2, 200);
    requireTrue(session.paneViewports().size() == 2, "2 pane viewports");

    session.removePaneViewport(1);
    requireTrue(session.paneViewports().size() == 1, "1 pane viewport after remove");

    // Re-add
    session.addPaneViewport(1, &vp1, 100);
    requireTrue(session.paneViewports().size() == 2, "2 pane viewports after re-add");

    std::printf("  Test 4 (remove + re-add) PASS\n");
  }

  std::printf("D9.5 multi-viewport: ALL PASS\n");
  return 0;
}
