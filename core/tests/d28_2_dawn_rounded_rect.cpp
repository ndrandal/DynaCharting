// ENC-488 (P2.5) — instancedRect@1 + rounded corners (D28.2) + indexed gather
// (D26) on Dawn.
//
// The Dawn counterpart of the GL instancedRect paths:
//   * d28_2_rounded_rect.cpp — the rounded-corner SDF baseline
//     (Renderer::drawInstancedRect, kInstRectVert/kInstRectFrag).
//   * d26_2_indexed_gl.cpp    — the D26 indexed-gather instanced baseline.
//
// Renders, through the backend registry with DeviceKind::Dawn into the headless
// DawnDevice offscreen RGBA8 target:
//
//   1. ROUNDED RECT: a single large rect with a big cornerRadius. The interior
//      and mid-edges read back the per-instance fill color; the extreme corners
//      read back CLEAR (rounded off by the SDF) — proving the rounded-corner
//      math is plumbed.
//
//   2. SHARP RECT: the same rect with cornerRadius = 0. Corners AND center all
//      read back the fill color (no rounding) — the cornerRadius==0 fast path.
//
//   3. INDEXED GATHER (D26): four quadrant rects with an index buffer selecting a
//      diagonal pair (rect0 + rect3). Only the two selected quadrants are filled;
//      the other two stay clear — proving the CPU-side gather draws exactly the
//      selected subset.
//
// This also exercises the shared drawInstanced foundation (per-instance
// step-mode buffer + instanced draw) that ENC-489/490/491 reuse.
//
// NDC Y-FLIP: clip-space y is negated in the WGSL (same as triSolid). The
// rounded rect is centered+symmetric so its corner checks are flip-invariant;
// the indexed quadrants map clip-bottom -> framebuffer-top (noted at each read).
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedRectBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

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
  std::printf("=== D28.2 Dawn rounded rect + indexed gather ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

  // --- Bring up the headless Dawn device. ---------------------------------
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

  // Backend + registry (registered ADDITIVELY under DeviceKind::Dawn).
  dc::DawnInstancedRectBackend instRect;
  if (!instRect.init(dev)) {
    std::fprintf(stderr, "DawnInstancedRectBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &instRect);

  auto makeRp = [&]() {
    dc::RenderPassDesc rp;
    rp.target = {};
    rp.viewportWidth = W;
    rp.viewportHeight = H;
    rp.clear = true;
    rp.clearColor[0] = 0.0f;
    rp.clearColor[1] = 0.0f;
    rp.clearColor[2] = 0.0f;
    rp.clearColor[3] = 1.0f;
    return rp;
  };

  auto renderDi = [&](dc::Scene& scene, dc::CpuBufferStore& store,
                      std::uint32_t diId) -> std::uint32_t {
    const dc::DrawItem* di = scene.getDrawItem(diId);
    if (!di) return 0;
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    if (!be) return 0;
    dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    return bs.drawCalls;
  };

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };

  // =====================================================================
  // Case 1: rounded rect — interior filled, corners rounded off (clear).
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // One rect covering most of the viewport (same as the GL d28_2 case).
    float rect[] = {-0.8f, -0.8f, 0.8f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    store.setCpuData(10, rect, sizeof(rect));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})"),
        "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})"),
        "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})"), "color");
    // Large corner radius (15px) — same as the GL baseline.
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":15})"), "style");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 3);
    dev.endRenderPass();
    check(calls == 1, "rounded rect: 1 draw call");

    std::uint8_t cen[4], bl[4], tr[4], midB[4];
    px(W / 2, H / 2, cen);   // center -> red fill
    px(7, 7, bl);            // extreme bottom-left corner -> rounded clear
    px(56, 56, tr);          // extreme top-right corner   -> rounded clear
    px(W / 2, 8, midB);      // mid bottom edge            -> red fill
    std::printf("  rounded center      R=%u G=%u B=%u A=%u\n", cen[0], cen[1], cen[2], cen[3]);
    std::printf("  rounded corner BL   R=%u G=%u B=%u A=%u\n", bl[0], bl[1], bl[2], bl[3]);
    std::printf("  rounded corner TR   R=%u G=%u B=%u A=%u\n", tr[0], tr[1], tr[2], tr[3]);
    std::printf("  rounded mid-edge    R=%u G=%u B=%u A=%u\n", midB[0], midB[1], midB[2], midB[3]);

    // Center is the red fill.
    check(cen[0] > 240 && cen[1] < 16 && cen[2] < 16, "rounded rect: center is red");
    // Corners rounded off -> cleared (black, the SDF coverage ~0 there).
    check(bl[0] < 24 && bl[1] < 24 && bl[2] < 24, "rounded rect: BL corner rounded (clear)");
    check(tr[0] < 24 && tr[1] < 24 && tr[2] < 24, "rounded rect: TR corner rounded (clear)");
    // Mid-edge stays inside the rounding -> still red.
    check(midB[0] > 200 && midB[1] < 40 && midB[2] < 40, "rounded rect: mid-edge is red");
  }

  // =====================================================================
  // Case 2: sharp rect (cornerRadius == 0) — corners AND center filled.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    float rect[] = {-0.8f, -0.8f, 0.8f, 0.8f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":16})"), "buf2");
    store.setCpuData(30, rect, sizeof(rect));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":1,"format":"rect4"})"),
        "geom2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"instancedRect@1","geometryId":200})"),
        "bind2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":22,"r":0,"g":0,"b":1,"a":1})"), "color2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":22,"cornerRadius":0})"), "style2");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 22);
    dev.endRenderPass();
    check(calls == 1, "sharp rect: 1 draw call");

    std::uint8_t cen[4], bl[4], tr[4];
    px(W / 2, H / 2, cen);
    px(8, 8, bl);
    px(55, 55, tr);
    std::printf("  sharp center     R=%u G=%u B=%u A=%u\n", cen[0], cen[1], cen[2], cen[3]);
    std::printf("  sharp corner BL  R=%u G=%u B=%u A=%u\n", bl[0], bl[1], bl[2], bl[3]);
    std::printf("  sharp corner TR  R=%u G=%u B=%u A=%u\n", tr[0], tr[1], tr[2], tr[3]);
    check(cen[2] > 240 && cen[0] < 16 && cen[1] < 16, "sharp rect: center is blue");
    check(bl[2] > 240 && bl[0] < 16, "sharp rect: BL corner is blue (not rounded)");
    check(tr[2] > 240 && tr[0] < 16, "sharp rect: TR corner is blue (not rounded)");
  }

  // =====================================================================
  // Case 3: D26 indexed gather — draw a selected diagonal pair of quadrants.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":40,"name":"P3"})"), "pane3");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":41,"paneId":40})"), "layer3");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":41})"), "di3");

    // Four quadrant rects (clip space), same as the GL d26_2 instanced case.
    float rects[] = {
      -1.0f, -1.0f, 0.0f, 0.0f,  // rect0: clip bottom-left
       0.0f, -1.0f, 1.0f, 0.0f,  // rect1: clip bottom-right
      -1.0f,  0.0f, 0.0f, 1.0f,  // rect2: clip top-left
       0.0f,  0.0f, 1.0f, 1.0f,  // rect3: clip top-right
    };
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":50,"byteLength":64})"), "vbuf3");
    store.setCpuData(50, rects, sizeof(rects));

    // Index buffer selects rect0 + rect3 (the diagonal pair).
    std::uint32_t indices[] = {0, 3};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":51,"byteLength":8})"), "ibuf3");
    store.setCpuData(51, indices, sizeof(indices));

    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":300,"vertexBufferId":50,"vertexCount":4,"format":"rect4","indexBufferId":51,"indexCount":2})"),
        "geom3");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"instancedRect@1","geometryId":300})"),
        "bind3");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":42,"r":1,"g":0,"b":0,"a":1})"), "color3");
    // Sharp corners so quadrant coverage is exact.
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":42,"cornerRadius":0})"), "style3");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 42);
    dev.endRenderPass();
    check(calls == 1, "indexed gather: 1 draw call (2 instances)");

    // After the WGSL y-flip, clip-bottom lands at framebuffer-top:
    //   rect0 (clip bottom-left)  -> framebuffer TOP-left      (small x, small y)
    //   rect3 (clip top-right)    -> framebuffer BOTTOM-right   (large x, large y)
    //   rect1 (clip bottom-right) -> framebuffer TOP-right      (NOT selected)
    //   rect2 (clip top-left)     -> framebuffer BOTTOM-left    (NOT selected)
    std::uint8_t sel0[4], sel3[4], skip1[4], skip2[4];
    px(W / 4, H / 4, sel0);        // top-left      -> rect0 (selected)  red
    px(3 * W / 4, 3 * H / 4, sel3);// bottom-right  -> rect3 (selected)  red
    px(3 * W / 4, H / 4, skip1);   // top-right     -> rect1 (filtered)  clear
    px(W / 4, 3 * H / 4, skip2);   // bottom-left   -> rect2 (filtered)  clear
    std::printf("  indexed sel rect0(TL)  R=%u G=%u B=%u A=%u\n", sel0[0], sel0[1], sel0[2], sel0[3]);
    std::printf("  indexed sel rect3(BR)  R=%u G=%u B=%u A=%u\n", sel3[0], sel3[1], sel3[2], sel3[3]);
    std::printf("  indexed skip rect1(TR) R=%u G=%u B=%u A=%u\n", skip1[0], skip1[1], skip1[2], skip1[3]);
    std::printf("  indexed skip rect2(BL) R=%u G=%u B=%u A=%u\n", skip2[0], skip2[1], skip2[2], skip2[3]);

    check(sel0[0] > 240 && sel0[1] < 16 && sel0[2] < 16, "indexed: rect0 selected -> red");
    check(sel3[0] > 240 && sel3[1] < 16 && sel3[2] < 16, "indexed: rect3 selected -> red");
    check(skip1[0] < 16 && skip1[1] < 16 && skip1[2] < 16, "indexed: rect1 filtered -> clear");
    check(skip2[0] < 16 && skip2[1] < 16 && skip2[2] < 16, "indexed: rect2 filtered -> clear");
  }

  std::printf("=== Dawn instancedRect: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
