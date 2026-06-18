// ENC-495 (P3.3) — GPU color-ID picking (D29.3) on Dawn.
//
// Pixel-perfect hit testing on WebGPU: each pickable DrawItem's id is rendered
// as a unique 24-bit RGB color into a SEPARATE offscreen pick target (the
// DawnDevice multi-target support added here — RenderTargetHandle id 1), then the
// pixel under the cursor is read back and decoded to recover the id. id 0 means
// "no hit". The Dawn mirror of GL Renderer::renderPick (core/src/gl/Renderer.cpp)
// and a cross-check of the GL d29_3_gpu_picking / d41_2_texquad_pick baselines.
//
// SCENARIO:
//   * Test 1 (flat triSolid): full-screen triangle id 5. Pick center -> 5;
//     a small centered triangle id 7 -> center is 7, a corner is 0 (background).
//   * Test 2 (instancedRect, distinct ids): a LEFT rect id 11 and a RIGHT rect
//     id 22. Pick inside the left half -> 11; inside the right half -> 22; a gap
//     between them -> 0. Confirms distinct items decode to distinct ids. The
//     pick uses x-separated probes (x is NOT y-flipped) so it's orientation-safe.
//   * Test 3 (EventBus): the renderPick overload emits a GeometryClicked with
//     the hit id.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnPickBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Types.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/event/EventBus.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
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

// Pack one Rect4Color instance (24B): rect4 f32 @0 + packed RGBA8 @16 + reserved
// scalar/row-id lane @20. The color is irrelevant to pick (the pick pass writes a
// flat id color) — it's set only to prove the color lane is correctly skipped.
static void pushRectColor(std::vector<std::uint8_t>& out, float x0, float y0,
                          float x1, float y1, std::uint32_t rgba8) {
  auto pf = [&](float v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  auto pu = [&](std::uint32_t v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  pf(x0); pf(y0); pf(x1); pf(y1);
  pu(rgba8);  // packed RGBA8 (skipped by the pick vertex layout)
  pu(0u);     // reserved scalar/row-id lane
}

// Pack one Point4Color instance (16B): pos2 f32 @0 + packed RGBA8 @8 + f32 pixel
// size @12. As with rects, the color lane is skipped by the pick layout.
static void pushPointColor(std::vector<std::uint8_t>& out, float x, float y,
                           std::uint32_t rgba8, float sizePx) {
  auto pf = [&](float v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  auto pu = [&](std::uint32_t v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  pf(x); pf(y);
  pu(rgba8);    // packed RGBA8 (skipped by the pick vertex layout)
  pf(sizePx);   // pixel diameter
}

int main() {
  std::printf("=== D29.3 Dawn GPU color-ID picking ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  dc::DawnPickBackend pick;
  if (!pick.init(dev)) {
    std::fprintf(stderr, "DawnPickBackend::init failed\n");
    return 1;
  }

  // -- Test 1: flat triSolid pick (full-screen + small triangle). ----------
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // Full-screen triangle id 5 (drawn first / underneath).
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di5");
    float big[] = {-1, -1, 3, -1, -1, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf5");
    store.setCpuData(10, big, sizeof(big));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
        "geom5");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})"),
        "bind5");

    // Small centered triangle id 7 (drawn second / on top).
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":7,"layerId":2})"), "di7");
    float small[] = {-0.3f, -0.3f, 0.3f, -0.3f, 0.0f, 0.3f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "buf7");
    store.setCpuData(11, small, sizeof(small));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"),
        "geom7");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":7,"pipeline":"triSolid@1","geometryId":101})"),
        "bind7");

    auto r1 = pick.renderPick(dev, scene, store, W, H, W / 2, H / 2);
    std::printf("  pick center: decoded id = %u (expect 7, the topmost)\n",
                r1.drawItemId);
    check(r1.drawItemId == 7, "flat pick center -> 7 (topmost small tri)");

    auto r2 = pick.renderPick(dev, scene, store, W, H, 2, 2);
    std::printf("  pick corner: decoded id = %u (expect 5, full-screen tri)\n",
                r2.drawItemId);
    check(r2.drawItemId == 5, "flat pick corner -> 5 (underlying full-screen tri)");
  }

  // -- Test 2: instancedRect, two distinct ids in left/right halves. -------
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // LEFT rect id 11: clip-space x in [-0.8, -0.2], full height.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":11,"layerId":2})"), "diL");
    float left[] = {-0.8f, -0.8f, -0.2f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":16})"), "bufL");
    store.setCpuData(20, left, sizeof(left));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":200,"vertexBufferId":20,"vertexCount":1,"format":"rect4"})"),
        "geomL");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":11,"pipeline":"instancedRect@1","geometryId":200})"),
        "bindL");

    // RIGHT rect id 22: clip-space x in [0.2, 0.8], full height.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":2})"), "diR");
    float right[] = {0.2f, -0.8f, 0.8f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":21,"byteLength":16})"), "bufR");
    store.setCpuData(21, right, sizeof(right));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":201,"vertexBufferId":21,"vertexCount":1,"format":"rect4"})"),
        "geomR");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"instancedRect@1","geometryId":201})"),
        "bindR");

    // Probe x positions (x not y-flipped): left ~ x=0.5*W*... compute pixels.
    // left rect spans clip [-0.8,-0.2] -> px [6.4,25.6]; probe x=16 (center).
    // right rect spans clip [0.2,0.8] -> px [38.4,57.6]; probe x=48 (center).
    // gap clip [-0.2,0.2] -> px [25.6,38.4]; probe x=32 (center, background).
    const int yMid = H / 2;
    auto rl = pick.renderPick(dev, scene, store, W, H, 16, yMid);
    auto rr = pick.renderPick(dev, scene, store, W, H, 48, yMid);
    auto rg = pick.renderPick(dev, scene, store, W, H, 32, yMid);
    std::printf("  pick left (x=16):  decoded id = %u (expect 11)\n", rl.drawItemId);
    std::printf("  pick right (x=48): decoded id = %u (expect 22)\n", rr.drawItemId);
    std::printf("  pick gap (x=32):   decoded id = %u (expect 0)\n", rg.drawItemId);
    check(rl.drawItemId == 11, "instRect pick left -> 11");
    check(rr.drawItemId == 22, "instRect pick right -> 22");
    check(rg.drawItemId == 0, "instRect pick gap -> 0 (background)");
    check(rl.drawItemId != rr.drawItemId, "distinct items decode to distinct ids");
  }

  // -- Test 3: EventBus GeometryClicked on hit. ----------------------------
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;
    dc::EventBus bus;

    dc::Id clickedId = 0;
    int clickCount = 0;
    bus.subscribe(dc::EventType::GeometryClicked,
                  [&](const dc::EventData& ev) {
                    clickedId = ev.targetId;
                    ++clickCount;
                  });

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":2})"), "di42");
    float big[] = {-1, -1, 3, -1, -1, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":24})"), "buf42");
    store.setCpuData(30, big, sizeof(big));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":300,"vertexBufferId":30,"vertexCount":3,"format":"pos2_clip"})"),
        "geom42");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"triSolid@1","geometryId":300})"),
        "bind42");

    auto rh = pick.renderPick(dev, scene, store, W, H, W / 2, H / 2, &bus);
    check(rh.drawItemId == 42, "event pick center -> 42");
    check(clickCount == 1 && clickedId == 42,
          "GeometryClicked emitted with id 42");

    // A clear pick emits no event.
    clickCount = 0;
    clickedId = 0;
    // Replace geometry with a tiny centered triangle and probe a corner.
    requireOk(cp.applyJsonText(R"({"cmd":"delete","id":300})"), "delGeom");
    float small[] = {-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":31,"byteLength":24})"), "buf42b");
    store.setCpuData(31, small, sizeof(small));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":301,"vertexBufferId":31,"vertexCount":3,"format":"pos2_clip"})"),
        "geom42b");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"triSolid@1","geometryId":301})"),
        "bind42b");
    auto rc = pick.renderPick(dev, scene, store, W, H, 2, 2, &bus);
    check(rc.drawItemId == 0, "event pick corner -> 0 (no hit)");
    check(clickCount == 0, "no GeometryClicked on a clear pick");
  }

  // -- Test 4 (ENC-652): instancedRectColor@1 is pickable. -----------------
  // Two per-instance-color rects (LEFT id 11, RIGHT id 22), each a single-instance
  // DrawItem, mirror Test 2 but on the 24B Rect4Color path. Proves the color +
  // reserved lanes are stepped over and the id still decodes per DrawItem.
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // LEFT rect id 11: clip x in [-0.8,-0.2], full height; arbitrary color.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":11,"layerId":2})"), "diL");
    std::vector<std::uint8_t> left;
    pushRectColor(left, -0.8f, -0.8f, -0.2f, 0.8f, 0xFF0000FFu);  // red
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "bufL");
    store.setCpuData(20, left.data(), static_cast<std::uint32_t>(left.size()));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":200,"vertexBufferId":20,"vertexCount":1,"format":"rect4_color"})"),
        "geomL");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":11,"pipeline":"instancedRectColor@1","geometryId":200})"),
        "bindL");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":11,"cornerRadius":0})"), "styleL");

    // RIGHT rect id 22: clip x in [0.2,0.8], full height; different color.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":2})"), "diR");
    std::vector<std::uint8_t> right;
    pushRectColor(right, 0.2f, -0.8f, 0.8f, 0.8f, 0xFF00FF00u);  // green
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":21,"byteLength":24})"), "bufR");
    store.setCpuData(21, right.data(), static_cast<std::uint32_t>(right.size()));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":201,"vertexBufferId":21,"vertexCount":1,"format":"rect4_color"})"),
        "geomR");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"instancedRectColor@1","geometryId":201})"),
        "bindR");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":22,"cornerRadius":0})"), "styleR");

    const int yMid = H / 2;
    auto rl = pick.renderPick(dev, scene, store, W, H, 16, yMid);
    auto rr = pick.renderPick(dev, scene, store, W, H, 48, yMid);
    auto rg = pick.renderPick(dev, scene, store, W, H, 32, yMid);
    std::printf("  rectColor pick left (x=16):  id=%u (expect 11)\n", rl.drawItemId);
    std::printf("  rectColor pick right (x=48): id=%u (expect 22)\n", rr.drawItemId);
    std::printf("  rectColor pick gap (x=32):   id=%u (expect 0)\n", rg.drawItemId);
    check(rl.drawItemId == 11, "instRectColor pick left -> 11");
    check(rr.drawItemId == 22, "instRectColor pick right -> 22");
    check(rg.drawItemId == 0, "instRectColor pick gap -> 0 (background)");
  }

  // -- Test 5 (ENC-652): instancedPointColor@1 is pickable. ----------------
  // Two per-point-color dots (LEFT id 33, RIGHT id 44), each a fixed-pixel disc.
  // A probe on a dot's center decodes its id; the gap between them is background.
  // Dots are large (32px) so the centers land squarely inside the footprint.
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // LEFT dot id 33 at clip (-0.5, 0) -> px x≈16. RIGHT dot id 44 at clip (0.5,0)
    // -> px x≈48. y=0 -> framebuffer mid-row. 16px discs: left covers px[8,24],
    // right px[40,56], so the x=32 gap probe is clearly background.
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":33,"layerId":2})"), "diL");
    std::vector<std::uint8_t> ld;
    pushPointColor(ld, -0.5f, 0.0f, 0xFF0000FFu, 16.0f);
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":50,"byteLength":16})"), "bufL");
    store.setCpuData(50, ld.data(), static_cast<std::uint32_t>(ld.size()));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":500,"vertexBufferId":50,"vertexCount":1,"format":"point4_color"})"),
        "geomL");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":33,"pipeline":"instancedPointColor@1","geometryId":500})"),
        "bindL");

    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":44,"layerId":2})"), "diR");
    std::vector<std::uint8_t> rd;
    pushPointColor(rd, 0.5f, 0.0f, 0xFF00FF00u, 16.0f);
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":51,"byteLength":16})"), "bufR");
    store.setCpuData(51, rd.data(), static_cast<std::uint32_t>(rd.size()));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":501,"vertexBufferId":51,"vertexCount":1,"format":"point4_color"})"),
        "geomR");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":44,"pipeline":"instancedPointColor@1","geometryId":501})"),
        "bindR");

    const int yMid = H / 2;
    auto rl = pick.renderPick(dev, scene, store, W, H, 16, yMid);
    auto rr = pick.renderPick(dev, scene, store, W, H, 48, yMid);
    auto rg = pick.renderPick(dev, scene, store, W, H, 32, yMid);
    std::printf("  pointColor pick left (x=16):  id=%u (expect 33)\n", rl.drawItemId);
    std::printf("  pointColor pick right (x=48): id=%u (expect 44)\n", rr.drawItemId);
    std::printf("  pointColor pick gap (x=32):   id=%u (expect 0)\n", rg.drawItemId);
    check(rl.drawItemId == 33, "instPointColor pick left -> 33");
    check(rr.drawItemId == 44, "instPointColor pick right -> 44");
    check(rg.drawItemId == 0, "instPointColor pick gap -> 0 (background)");
  }

  std::printf("=== D29.3 Dawn picking: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
