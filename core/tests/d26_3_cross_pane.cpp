// D26.3 — Selection-change trigger (pure C++, no GL)
// Tests: notifySelectionChanged() fires dependent compute callbacks,
//        flag resets after update(), only flagged recipes fire.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/data/FakeDataSource.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
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
  std::printf("=== D26.3 Selection-Change Trigger ===\n");

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Create prerequisites
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P1"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L1"})"), "layer1");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"L2"})"), "layer2");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

  dc::FakeDataSourceConfig fakeCfg;
  fakeCfg.candleBufferId = 0;
  fakeCfg.lineBufferId = 0;
  fakeCfg.tickIntervalMs = 100;
  dc::FakeDataSource source(fakeCfg);

  dc::ChartSession session(cp, ingest);

  // Mount two line recipes
  auto line1 = std::make_unique<dc::LineRecipe>(100,
    dc::LineRecipeConfig{0, 10, "Line1", false});
  auto h1 = session.mount(std::move(line1), 50);

  auto line2 = std::make_unique<dc::LineRecipe>(200,
    dc::LineRecipeConfig{0, 11, "Line2", false});
  auto h2 = session.mount(std::move(line2), 50);

  // Track callback invocations
  int cb1Count = 0;
  int cb2Count = 0;

  session.setComputeCallback(h1, [&]() -> std::vector<dc::Id> {
    cb1Count++;
    return {999}; // fake touched buffer
  });

  session.setComputeCallback(h2, [&]() -> std::vector<dc::Id> {
    cb2Count++;
    return {998};
  });

  // Test 1: Without notifySelectionChanged, callbacks do NOT fire on selection path
  {
    cb1Count = 0;
    cb2Count = 0;
    session.setRecomputeOnSelectionChange(h1, true);
    session.setRecomputeOnSelectionChange(h2, true);

    auto result = session.update(source);
    check(!result.selectionChanged, "no selectionChanged without notify");
    check(cb1Count == 0, "cb1 not called without notify");
    check(cb2Count == 0, "cb2 not called without notify");
  }

  // Test 2: notifySelectionChanged fires callbacks with the flag set
  {
    cb1Count = 0;
    cb2Count = 0;

    session.notifySelectionChanged();
    auto result = session.update(source);

    check(result.selectionChanged, "selectionChanged is true after notify");
    check(cb1Count == 1, "cb1 fired once");
    check(cb2Count == 1, "cb2 fired once");
    // Touched buffer IDs should include the callback return values
    bool has999 = false, has998 = false;
    for (dc::Id id : result.touchedBufferIds) {
      if (id == 999) has999 = true;
      if (id == 998) has998 = true;
    }
    check(has999, "touchedBufferIds contains 999 from cb1");
    check(has998, "touchedBufferIds contains 998 from cb2");
  }

  // Test 3: Flag resets after update (not sticky)
  {
    cb1Count = 0;
    cb2Count = 0;

    auto result = session.update(source);
    check(!result.selectionChanged, "selectionChanged resets after update");
    check(cb1Count == 0, "cb1 not called after flag reset");
    check(cb2Count == 0, "cb2 not called after flag reset");
  }

  // Test 4: Only flagged recipes fire on selection change
  {
    cb1Count = 0;
    cb2Count = 0;

    session.setRecomputeOnSelectionChange(h1, true);
    session.setRecomputeOnSelectionChange(h2, false); // disable for h2

    session.notifySelectionChanged();
    auto result = session.update(source);

    check(result.selectionChanged, "selectionChanged with partial flag");
    check(cb1Count == 1, "cb1 fired (flagged)");
    check(cb2Count == 0, "cb2 NOT fired (not flagged)");
  }

  // Test 5: Multiple notifySelectionChanged before update = single fire
  {
    cb1Count = 0;
    session.setRecomputeOnSelectionChange(h1, true);

    session.notifySelectionChanged();
    session.notifySelectionChanged();
    session.notifySelectionChanged();
    auto result = session.update(source);

    check(cb1Count == 1, "multiple notifies = single callback fire");
  }

  std::printf("=== D26.3 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
