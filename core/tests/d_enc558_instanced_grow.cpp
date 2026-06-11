// ENC-558 — instanced backends re-upload when the streaming CPU buffer GROWS.
//
// REGRESSION for the bug the showcase vertical slice hit: the instanced Dawn
// backends (DawnInstancedCandleBackend / DawnInstancedRectBackend) cached their
// GPU instance buffer per geometryId on the FIRST render and never re-read the
// CpuBufferStore on subsequent renders. So when live/replayed instanced data
// grew via streaming ingest (writeRange/setCpuData), they kept drawing the first
// frame's instance count and pixels — and stopped updating.
//
// The fix: CpuBufferStore now exposes getCpuDataVersion() (bumped on every CPU
// mutation); the instanced backends stamp the source buffer version they built
// from and re-gather/re-upload (growing the device buffer + updating
// instanceCount) whenever it changes. Static (unchanged) geometry stays a cache
// hit.
//
// This test renders one candle (and one rect), then GROWS the geometry to three
// instances and renders again WITH THE SAME geometryId, asserting:
//   * the second render's instanced draw covers the new instances (the gather
//     count grew), and
//   * pixels that were CLEAR in render 1 (where the 2nd/3rd instance now sit)
//     are LIT in render 2.
// Before the fix, render 2 == render 1 (frozen first frame) and these fail.
//
// Headless: force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedCandleBackend.hpp"
#include "dc/gpu/DawnInstancedRectBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdint>
#include <cstdio>

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
  if (cond) { std::printf("  PASS: %s\n", name); ++passed; }
  else      { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

int main() {
  std::printf("=== ENC-558 instanced buffer GROWTH re-upload ===\n");

  constexpr std::uint32_t W = 128;
  constexpr std::uint32_t H = 128;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json to force lavapipe.\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // x positions for the 3 candle/rect slots: cx=-0.5 -> px32, 0.0 -> px64,
  // 0.5 -> px96. We grow from 1 instance (slot 0) to 3 (slots 0,1,2).
  constexpr std::uint32_t cxPix[3] = {32, 64, 96};
  constexpr std::uint32_t bodyY = H / 2;

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };
  auto lit = [](const std::uint8_t* p) {
    return p[0] > 80 || p[1] > 80 || p[2] > 80;
  };

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = rp.clearColor[1] = rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  // ---------------------------------------------------------------- CANDLE ---
  {
    dc::DawnInstancedCandleBackend candle;
    if (!candle.init(dev)) { std::fprintf(stderr, "candle init failed\n"); return 1; }
    dc::BackendRegistry backends;
    backends.registerBackend(dc::DeviceKind::Dawn, &candle);

    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // candle6 = (cx, open, high, low, close, hw). One UP candle in slot 0.
    float c0[] = { -0.5f, -0.3f, 0.85f, -0.85f, 0.3f, 0.15f };
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf");
    store.setCpuData(10, c0, sizeof(c0));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"candle6"})"), "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedCandle@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
        R"("colorUpR":0,"colorUpG":1,"colorUpB":0,"colorUpA":1,)"
        R"("colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})"), "style");

    const dc::DrawItem* di = scene.getDrawItem(3);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);

    // --- Render 1: one candle. ---
    dev.beginRenderPass(rp);
    dc::BackendStats bs1 = be->renderDrawItem(dev, scene, store, *di, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r1s1[4], r1s2[4];
    px(cxPix[1], bodyY, r1s1);  // slot 1 (empty in render 1)
    px(cxPix[2], bodyY, r1s2);  // slot 2 (empty in render 1)
    check(bs1.drawCalls == 1, "candle render1: 1 draw call");
    check(!lit(r1s1) && !lit(r1s2), "candle render1: slots 1 & 2 are clear");

    // --- GROW to three candles (streaming ingest: writeRange tail-append + the
    // geometry vertexCount bump the app issues alongside). SAME geometryId. ---
    float c12[] = {
       0.0f,  0.3f, 0.85f, -0.85f, -0.3f, 0.15f,  // slot 1 DOWN
       0.5f, -0.3f, 0.85f, -0.85f,  0.3f, 0.15f,  // slot 2 UP
    };
    store.writeRange(10, sizeof(c0), c12, sizeof(c12));
    requireOk(cp.applyJsonText(
        R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":3})"), "grow");

    // --- Render 2: must now draw all three. ---
    const dc::DrawItem* di2 = scene.getDrawItem(3);
    dev.beginRenderPass(rp);
    dc::BackendStats bs2 = be->renderDrawItem(dev, scene, store, *di2, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r2s1[4], r2s2[4];
    px(cxPix[1], bodyY, r2s1);
    px(cxPix[2], bodyY, r2s2);
    std::printf("  candle r2 slot1 R=%u G=%u B=%u  slot2 R=%u G=%u B=%u\n",
                r2s1[0], r2s1[1], r2s1[2], r2s2[0], r2s2[1], r2s2[2]);
    check(bs2.drawCalls == 1, "candle render2: 1 draw call (now 3 instances)");
    check(lit(r2s1), "candle render2: slot 1 now lit (buffer grew -> re-uploaded)");
    check(lit(r2s2), "candle render2: slot 2 now lit (buffer grew -> re-uploaded)");
    check(r2s1[0] > 150 && r2s1[1] < 60, "candle render2: slot 1 is DOWN color (red)");
  }

  // ------------------------------------------------------------------ RECT ---
  {
    dc::DawnInstancedRectBackend rect;
    if (!rect.init(dev)) { std::fprintf(stderr, "rect init failed\n"); return 1; }
    dc::BackendRegistry backends;
    backends.registerBackend(dc::DeviceKind::Dawn, &rect);

    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // rect4 = (x0,y0,x1,y1). One bar centered at cx=-0.5 in slot 0.
    float r0[] = { -0.6f, -0.3f, -0.4f, 0.3f };  // slot 0
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    store.setCpuData(10, r0, sizeof(r0));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})"), "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,"r":0,"g":0.6,"b":1,"a":1})"), "style");

    const dc::DrawItem* di = scene.getDrawItem(3);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);

    dev.beginRenderPass(rp);
    dc::BackendStats bs1 = be->renderDrawItem(dev, scene, store, *di, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r1s1[4], r1s2[4];
    px(cxPix[1], bodyY, r1s1);
    px(cxPix[2], bodyY, r1s2);
    check(bs1.drawCalls == 1, "rect render1: 1 draw call");
    check(!lit(r1s1) && !lit(r1s2), "rect render1: slots 1 & 2 are clear");

    // GROW to three bars (slots 1 and 2). SAME geometryId.
    float r12[] = {
      -0.1f, -0.3f, 0.1f, 0.3f,   // slot 1 (cx=0.0)
       0.4f, -0.3f, 0.6f, 0.3f,   // slot 2 (cx=0.5)
    };
    store.writeRange(10, sizeof(r0), r12, sizeof(r12));
    requireOk(cp.applyJsonText(
        R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":3})"), "grow");

    const dc::DrawItem* di2 = scene.getDrawItem(3);
    dev.beginRenderPass(rp);
    dc::BackendStats bs2 = be->renderDrawItem(dev, scene, store, *di2, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r2s1[4], r2s2[4];
    px(cxPix[1], bodyY, r2s1);
    px(cxPix[2], bodyY, r2s2);
    std::printf("  rect r2 slot1 R=%u G=%u B=%u  slot2 R=%u G=%u B=%u\n",
                r2s1[0], r2s1[1], r2s1[2], r2s2[0], r2s2[1], r2s2[2]);
    check(bs2.drawCalls == 1, "rect render2: 1 draw call (now 3 instances)");
    check(lit(r2s1), "rect render2: slot 1 now lit (buffer grew -> re-uploaded)");
    check(lit(r2s2), "rect render2: slot 2 now lit (buffer grew -> re-uploaded)");
  }

  std::printf("=== ENC-558 instanced growth: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
