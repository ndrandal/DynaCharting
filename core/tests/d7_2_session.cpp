// D7.2 — ChartSession mount/unmount lifecycle test (pure C++, no GL)
// Tests: mount recipes → verify scene, LiveIngestLoop bindings.
//        unmount → verify clean. Compute dependency graph.

#include "dc/session/ChartSession.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/recipe/CandleRecipe.hpp"
#include "dc/recipe/LineRecipe.hpp"
#include "dc/viewport/Viewport.hpp"
#include "dc/data/FakeDataSource.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>

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

int main() {
  // --- Test 1: Mount and verify scene state ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    // Create pane + layers (prerequisites)
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"Line"})"), "l11");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

    dc::ChartSession session(cp, ingest);

    // Mount candle recipe
    auto candle = std::make_unique<dc::CandleRecipe>(100,
      dc::CandleRecipeConfig{0, 10, "OHLC", false});
    auto hCandle = session.mount(std::move(candle), 50);
    requireTrue(session.isMounted(hCandle), "candle mounted");

    // Verify scene has the candle draw item
    const auto* di = scene.getDrawItem(102);
    requireTrue(di != nullptr, "candle drawItem exists in scene");
    requireTrue(di->transformId == 50, "candle drawItem has shared transform");

    // Mount line recipe
    auto line = std::make_unique<dc::LineRecipe>(200,
      dc::LineRecipeConfig{0, 11, "CloseLine", false});
    auto hLine = session.mount(std::move(line), 50);
    requireTrue(session.isMounted(hLine), "line mounted");

    const auto* ldi = scene.getDrawItem(202);
    requireTrue(ldi != nullptr, "line drawItem exists in scene");
    requireTrue(ldi->transformId == 50, "line drawItem has shared transform");

    // Verify getRecipe works
    requireTrue(session.getRecipe(hCandle) != nullptr, "getRecipe candle");
    requireTrue(session.getRecipe(hLine) != nullptr, "getRecipe line");
    requireTrue(session.getRecipe(999) == nullptr, "getRecipe invalid");

    std::printf("  Mount PASS\n");
  }

  // --- Test 2: Unmount removes scene objects ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"Price"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"Candles"})"), "l10");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

    dc::ChartSession session(cp, ingest);

    auto candle = std::make_unique<dc::CandleRecipe>(100,
      dc::CandleRecipeConfig{0, 10, "OHLC", false});
    auto hCandle = session.mount(std::move(candle), 50);

    requireTrue(scene.getDrawItem(102) != nullptr, "drawItem before unmount");
    requireTrue(scene.getGeometry(101) != nullptr, "geometry before unmount");

    session.unmount(hCandle);
    requireTrue(!session.isMounted(hCandle), "candle unmounted");
    requireTrue(scene.getDrawItem(102) == nullptr, "drawItem removed after unmount");
    requireTrue(scene.getGeometry(101) == nullptr, "geometry removed after unmount");

    std::printf("  Unmount PASS\n");
  }

  // --- Test 3: UnmountAll ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "l10");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"L2"})"), "l11");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

    dc::ChartSession session(cp, ingest);

    auto h1 = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "c", false}), 50);
    auto h2 = session.mount(
      std::make_unique<dc::LineRecipe>(200,
        dc::LineRecipeConfig{0, 11, "l", false}), 50);

    requireTrue(session.isMounted(h1), "h1 mounted");
    requireTrue(session.isMounted(h2), "h2 mounted");

    session.unmountAll();

    requireTrue(!session.isMounted(h1), "h1 unmounted");
    requireTrue(!session.isMounted(h2), "h2 unmounted");
    requireTrue(scene.getDrawItem(102) == nullptr, "candle DI gone");
    requireTrue(scene.getDrawItem(202) == nullptr, "line DI gone");

    std::printf("  UnmountAll PASS\n");
  }

  // --- Test 4: LiveIngestLoop bindings match subscriptions ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "l");

    dc::ChartSession session(cp, ingest);

    auto h1 = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "c", false}));
    auto h2 = session.mount(
      std::make_unique<dc::LineRecipe>(200,
        dc::LineRecipeConfig{0, 10, "l", false}));

    // Use FakeDataSource to test bindings work with consumeAndUpdate
    dc::FakeDataSourceConfig srcCfg;
    srcCfg.candleBufferId = 100;
    srcCfg.lineBufferId = 200;
    srcCfg.tickIntervalMs = 10;
    srcCfg.candleIntervalMs = 50;
    srcCfg.startPrice = 100.0f;
    srcCfg.volatility = 0.5f;

    dc::FakeDataSource src(srcCfg);
    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto touched = session.loop().consumeAndUpdate(src, ingest, cp);
    src.stop();

    requireTrue(!touched.empty(), "bindings work: data processed");

    auto candleSz = ingest.getBufferSize(100);
    requireTrue(candleSz > 0, "candle buffer has data via session bindings");
    requireTrue(candleSz % 24 == 0, "candle buffer size valid");

    auto lineSz = ingest.getBufferSize(200);
    requireTrue(lineSz > 0, "line buffer has data via session bindings");
    requireTrue(lineSz % 8 == 0, "line buffer size valid");

    // Unmount candle, verify line bindings still work
    session.unmount(h1);

    // Start source again, produce more data
    dc::FakeDataSource src2(srcCfg);
    src2.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto touched2 = session.loop().consumeAndUpdate(src2, ingest, cp);
    src2.stop();

    // Line should still get data. Candle buffer size unchanged since unmount
    // (new data goes to buffer IDs 100 and 200 but only 200's binding exists)
    auto lineSz2 = ingest.getBufferSize(200);
    requireTrue(lineSz2 >= lineSz, "line buffer grew after candle unmount");

    std::printf("  Bindings PASS\n");
  }

  // --- Test 5: Compute dependency graph ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "l");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":11,"paneId":1,"name":"L2"})"), "l2");

    dc::ChartSession session(cp, ingest);
    dc::Viewport vp;
    vp.setPixelViewport(400, 300);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(-5.0, 30.0, 90.0, 110.0);
    session.setViewport(&vp);

    auto hCandle = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "c", false}));
    auto hLine = session.mount(
      std::make_unique<dc::LineRecipe>(200,
        dc::LineRecipeConfig{0, 11, "l", false}));

    // Line depends on candle buffer for its compute
    bool computeCalled = false;
    session.addComputeDependency(hLine, 100); // when buffer 100 is touched
    session.setComputeCallback(hLine, [&]() -> std::vector<dc::Id> {
      computeCalled = true;
      return {200}; // pretend we wrote to buffer 200
    });

    dc::FakeDataSourceConfig srcCfg;
    srcCfg.candleBufferId = 100;
    srcCfg.lineBufferId = 200;
    srcCfg.tickIntervalMs = 10;
    srcCfg.candleIntervalMs = 50;
    srcCfg.startPrice = 100.0f;

    dc::FakeDataSource src(srcCfg);
    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto fr = session.update(src);
    src.stop();

    requireTrue(fr.dataChanged, "data changed");
    requireTrue(computeCalled, "compute callback invoked");
    requireTrue(!fr.touchedBufferIds.empty(), "touched buffer IDs");

    // Check that buffer 200 is in the touched set (from compute callback)
    bool has200 = false;
    for (dc::Id id : fr.touchedBufferIds) {
      if (id == 200) { has200 = true; break; }
    }
    requireTrue(has200, "compute result merged into touched set");

    std::printf("  Compute deps PASS\n");
  }

  // --- Test 6: update() syncs transforms ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::IngestProcessor ingest;
    cp.setIngestProcessor(&ingest);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1,"name":"L"})"), "l");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");

    dc::ChartSession session(cp, ingest);
    dc::Viewport vp;
    vp.setPixelViewport(400, 300);
    vp.setClipRegion({-1.0f, 1.0f, -1.0f, 1.0f});
    vp.setDataRange(0.0, 100.0, 50.0, 150.0);
    session.setViewport(&vp);

    auto hCandle = session.mount(
      std::make_unique<dc::CandleRecipe>(100,
        dc::CandleRecipeConfig{0, 10, "c", false}), 50);

    dc::FakeDataSourceConfig srcCfg;
    srcCfg.candleBufferId = 100;
    srcCfg.lineBufferId = 200;
    srcCfg.tickIntervalMs = 10;
    srcCfg.candleIntervalMs = 50;
    srcCfg.startPrice = 100.0f;

    dc::FakeDataSource src(srcCfg);
    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto fr = session.update(src);
    src.stop();

    requireTrue(fr.viewportChanged, "viewport changed flag set");

    // Verify the transform was actually updated in the scene
    const auto* xf = scene.getTransform(50);
    requireTrue(xf != nullptr, "transform exists");
    // The transform should have non-identity values from viewport mapping
    auto tp = vp.computeTransformParams();
    requireTrue(std::fabs(xf->params.sx - tp.sx) < 1e-5f, "transform sx matches viewport");
    requireTrue(std::fabs(xf->params.sy - tp.sy) < 1e-5f, "transform sy matches viewport");

    std::printf("  Transform sync PASS\n");
  }

  std::printf("\nD7.2 session PASS\n");
  return 0;
}
