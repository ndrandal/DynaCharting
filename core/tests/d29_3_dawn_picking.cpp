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

  std::printf("=== D29.3 Dawn picking: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
