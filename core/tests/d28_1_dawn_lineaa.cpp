// ENC-490 (P2.7) — lineAA@1 (anti-aliased thick lines) + dash patterns (D28.1)
// on Dawn.
//
// The Dawn counterpart of the GL lineAA paths:
//   * d10_6_aa_lines.cpp     — the anti-aliased thick-line baseline
//     (Renderer::drawLineAA, kLineAAVert/kLineAAFrag): AA coverage falloff.
//   * d28_1_dashed_lines.cpp — the dash-pattern baseline (same shader, dash
//     on/off via the dashLen/gapLen uniforms).
//
// WebGPU has NO native line width, so each segment is QUAD-EXPANDED in the vertex
// shader into a thick screen-aligned quad; the fragment shader computes the AA
// coverage (distance-to-line falloff over the AA fringe) and applies the dash
// pattern. This test renders, through the backend registry with DeviceKind::Dawn
// into the headless DawnDevice offscreen RGBA8 target:
//
//   1. SOLID THICK AA LINE: a thick green horizontal line on black. The line
//      CENTER row reads back the green fill; a row just OUTSIDE the nominal width
//      reads back PARTIAL green (the AA fringe fades, not a hard cut); a row well
//      outside reads back CLEAR. Proves the quad expansion + AA coverage.
//
//   2. DASHED LINE: a thick green horizontal dashed line (16px dash, 16px gap). A
//      pixel inside a DASH reads back green; a pixel inside a GAP reads back
//      CLEAR. Proves the dash on/off pattern. We also count colored vs gap
//      pixels along the center row (cross-check vs the GL d28_1_dashed_lines
//      coloredCount/blackCount asserts).
//
// NDC Y-FLIP: clip-space y is negated in the WGSL (same as triSolid/instancedRect).
// The lines are horizontal and centered (y=0), so the flip is symmetric about the
// center row — the AA fringe appears symmetrically above and below.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnLineAABackend.hpp"

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
  std::printf("=== D28.1 Dawn lineAA (thick AA lines + dash) ===\n");

  // Square viewport so the GL-parity lineWidth->clip conversion (which divides by
  // viewW only) maps to the same pixel scale vertically — making the AA fringe a
  // clean, samplable band of partial-coverage pixels across the line edge.
  constexpr std::uint32_t W = 128;
  constexpr std::uint32_t H = 128;

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
  dc::DawnLineAABackend lineAA;
  if (!lineAA.init(dev)) {
    std::fprintf(stderr, "DawnLineAABackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &lineAA);

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
  // Case 1: solid thick AA line — center solid, AA edge fades, clear outside.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // Horizontal segment from (-0.9, 0) to (0.9, 0): one rect4 instance whose
    // xy/zw are the two segment endpoints. Center row = H/2.
    float line[] = {-0.9f, 0.0f, 0.9f, 0.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    store.setCpuData(10, line, sizeof(line));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})"),
        "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"lineAA@1","geometryId":100})"),
        "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":0,"a":1})"), "color");
    // Thick solid line: 12px wide, no dash. AA fringe is 1.5px beyond each edge,
    // so the edge transition spans whole pixels (clean partial-coverage band).
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,"lineWidth":12,"dashLength":0,"gapLength":0})"),
        "style");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 3);
    dev.endRenderPass();
    check(calls == 1, "solid line: 1 draw call");

    // Scan a column at mid-x (x = W/2) downward from the center row to find the
    // green profile: solid at center, fading in the AA fringe, clear far away.
    const std::uint32_t cx = W / 2;
    const std::uint32_t cy = H / 2;
    std::uint8_t center[4];
    px(cx, cy, center);
    std::printf("  solid center (%u,%u)  R=%u G=%u B=%u A=%u\n",
                cx, cy, center[0], center[1], center[2], center[3]);

    // Walk outward (both above and below the center row) recording the green
    // profile. With a 12px line (half-width ~6px) the core is solid out to ~6px
    // and the AA fringe fades over the next ~1.5px — yielding at least one pixel
    // with intermediate (partial-coverage) green between solid and clear.
    int partialGreen = -1;
    std::printf("  solid column profile (dy: green):");
    for (int dy = -12; dy <= 12; ++dy) {
      std::uint8_t p[4];
      px(cx, static_cast<std::uint32_t>(static_cast<int>(cy) + dy), p);
      std::printf(" %d:%u", dy, p[1]);
      if (p[1] > 16 && p[1] < 240) {
        if (static_cast<int>(p[1]) > partialGreen) partialGreen = p[1];
      }
    }
    std::printf("\n");
    std::uint8_t farClear[4];
    px(cx, cy + 24, farClear);  // ~24px below center: well outside the line
    std::printf("  solid AA partial green max = %d (intermediate => AA fade)\n", partialGreen);
    std::printf("  solid far (%u,%u)     R=%u G=%u B=%u A=%u\n",
                cx, cy + 24, farClear[0], farClear[1], farClear[2], farClear[3]);

    // Center pixel is the solid green line color.
    check(center[1] > 200 && center[0] < 40 && center[2] < 40,
          "solid line: center is green");
    // The AA edge produces at least one partial-coverage green pixel
    // (intermediate 16..240) — proves the falloff, not a hard cut.
    check(partialGreen >= 16 && partialGreen <= 240,
          "solid line: AA edge fades (partial coverage)");
    // Far outside the line is clear (the black clear color).
    check(farClear[1] < 16, "solid line: far from line is clear");
  }

  // =====================================================================
  // Case 2: dashed thick line — dash pixels green, gap pixels clear.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":20,"name":"P2"})"), "pane2");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":21,"paneId":20})"), "layer2");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":22,"layerId":21})"), "di2");

    float line[] = {-0.9f, 0.0f, 0.9f, 0.0f};
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":16})"), "buf2");
    store.setCpuData(30, line, sizeof(line));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":200,"vertexBufferId":30,"vertexCount":1,"format":"rect4"})"),
        "geom2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":22,"pipeline":"lineAA@1","geometryId":200})"),
        "bind2");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":22,"r":0,"g":1,"b":0,"a":1})"), "color2");
    // 16px dash, 16px gap, 4px wide (matches the GL d28_1_dashed_lines case).
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":22,"lineWidth":4,"dashLength":16,"gapLength":16})"),
        "style2");

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 22);
    dev.endRenderPass();
    check(calls == 1, "dashed line: 1 draw call");

    // Count colored vs gap pixels along the center row (cross-check vs the GL
    // d28_1_dashed_lines coloredCount/blackCount asserts).
    const std::uint32_t y = H / 2;
    int coloredCount = 0;
    int gapCount = 0;
    int firstDashX = -1, firstGapX = -1;
    for (std::uint32_t x = 10; x < W - 10; ++x) {
      std::uint8_t p[4];
      px(x, y, p);
      if (p[1] > 100) {
        ++coloredCount;
        if (firstDashX < 0) firstDashX = static_cast<int>(x);
      } else {
        ++gapCount;
        // record the first gap that comes after we have already seen a dash, so
        // it is an interior gap (not the lead-in margin).
        if (firstGapX < 0 && firstDashX >= 0) firstGapX = static_cast<int>(x);
      }
    }
    std::printf("  dashed center row: coloredCount=%d gapCount=%d\n",
                coloredCount, gapCount);

    std::uint8_t dashPx[4] = {0, 0, 0, 0};
    std::uint8_t gapPx[4] = {0, 0, 0, 0};
    if (firstDashX >= 0) px(static_cast<std::uint32_t>(firstDashX), y, dashPx);
    if (firstGapX >= 0) px(static_cast<std::uint32_t>(firstGapX), y, gapPx);
    std::printf("  dashed dash px x=%d   R=%u G=%u B=%u A=%u\n",
                firstDashX, dashPx[0], dashPx[1], dashPx[2], dashPx[3]);
    std::printf("  dashed gap  px x=%d   R=%u G=%u B=%u A=%u\n",
                firstGapX, gapPx[0], gapPx[1], gapPx[2], gapPx[3]);

    // Has both dash (colored) and gap (clear) pixels — and is not fully solid.
    check(coloredCount > 10, "dashed line: has colored (dash) pixels");
    check(gapCount > 10, "dashed line: has gap pixels");
    check(coloredCount < static_cast<int>(W - 20),
          "dashed line: not fully solid");
    // A pixel sampled in a dash is green; a pixel sampled in a gap is clear.
    check(firstDashX >= 0 && dashPx[1] > 100, "dashed line: dash pixel is green");
    check(firstGapX >= 0 && gapPx[1] < 16, "dashed line: gap pixel is clear");
  }

  std::printf("=== Dawn lineAA: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
