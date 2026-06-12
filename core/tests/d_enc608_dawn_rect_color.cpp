// ENC-608 (P2.1) — instancedRectColor@1 on Dawn: THE KEYSTONE per-instance-color
// rect. The Dawn render proof that ONE instanced draw can paint a grid of rects
// each with a DIFFERENT color (read straight from the per-instance packed RGBA8
// in the Rect4Color record) — NOT a single uniform color. This is what collapses
// the weather-radar / correlation / footprint / pie views to native with ZERO
// compute (RESEARCH §4.3).
//
// The Dawn counterpart of d28_2_dawn_rounded_rect.cpp, but the four quadrant rects
// carry FOUR DISTINCT colors in one draw. Renders through the backend registry
// with DeviceKind::Dawn into the headless DawnDevice offscreen RGBA8 target, reads
// back, and asserts each quadrant shows its OWN color (and that the four colors
// are mutually distinct — proving per-instance, not uniform, color).
//
// Rect4Color instance record (24B): rect4 f32 (x0,y0,x1,y1) @0 + packed RGBA8 @16
// + reserved scalar/row-id lane @20 (0). Color byte order: R,G,B,A low..high.
//
// NDC Y-FLIP: clip-space y is negated in the WGSL (same as instancedRect), so a
// clip-bottom quadrant lands at framebuffer-top (noted at each read).
//
// On this headless box the only Vulkan backend may be lavapipe (software). Force:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedRectColorBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

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

// Pack one Rect4Color instance (24B) into `out`.
static void pushInstance(std::vector<std::uint8_t>& out, float x0, float y0,
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
  pu(rgba8);   // packed RGBA8 color (per instance)
  pu(0u);      // reserved scalar/row-id lane
}

int main() {
  std::printf("=== ENC-608 Dawn instancedRectColor (keystone per-instance color) ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

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

  dc::DawnInstancedRectColorBackend instRectColor;
  if (!instRectColor.init(dev)) {
    std::fprintf(stderr, "DawnInstancedRectColorBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &instRectColor);

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
  // A grid of FOUR quadrant rects, each a DIFFERENT color, in ONE instanced
  // draw. Colors (packed RGBA8, byte0=R..byte3=A, all opaque):
  //   rect0: red    0xFF0000FF   rect1: green 0xFF00FF00
  //   rect2: blue   0xFFFF0000   rect3: yellow 0xFF00FFFF
  // =====================================================================
  const std::uint32_t kRed    = 0xFF0000FFu;
  const std::uint32_t kGreen  = 0xFF00FF00u;
  const std::uint32_t kBlue   = 0xFFFF0000u;
  const std::uint32_t kYellow = 0xFF00FFFFu;

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  // Four quadrant rects (clip space), each with its own packed color.
  std::vector<std::uint8_t> verts;
  pushInstance(verts, -1.0f, -1.0f, 0.0f, 0.0f, kRed);    // rect0 clip BL
  pushInstance(verts,  0.0f, -1.0f, 1.0f, 0.0f, kGreen);  // rect1 clip BR
  pushInstance(verts, -1.0f,  0.0f, 0.0f, 1.0f, kBlue);   // rect2 clip TL
  pushInstance(verts,  0.0f,  0.0f, 1.0f, 1.0f, kYellow); // rect3 clip TR

  requireOk(cp.applyJsonText(
      R"({"cmd":"createBuffer","id":10,"byteLength":96})"), "buf");
  store.setCpuData(10, verts.data(),
                   static_cast<std::uint32_t>(verts.size()));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"rect4_color"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRectColor@1","geometryId":100})"),
      "bind");
  // Sharp corners so quadrant coverage is exact.
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})"), "style");

  dc::RenderPassDesc rp = makeRp();
  dev.beginRenderPass(rp);
  std::uint32_t calls = renderDi(scene, store, 3);
  dev.endRenderPass();
  check(calls == 1, "rectColor: 1 instanced draw call (4 instances)");

  // After the WGSL y-flip, clip-bottom lands at framebuffer-top:
  //   rect0 (clip BL) -> framebuffer TOP-left      -> red
  //   rect1 (clip BR) -> framebuffer TOP-right     -> green
  //   rect2 (clip TL) -> framebuffer BOTTOM-left   -> blue
  //   rect3 (clip TR) -> framebuffer BOTTOM-right  -> yellow
  std::uint8_t tl[4], tr[4], bl[4], br[4];
  px(W / 4,     H / 4,     tl);  // top-left      -> rect0 red
  px(3 * W / 4, H / 4,     tr);  // top-right     -> rect1 green
  px(W / 4,     3 * H / 4, bl);  // bottom-left   -> rect2 blue
  px(3 * W / 4, 3 * H / 4, br);  // bottom-right  -> rect3 yellow
  std::printf("  TL(rect0) R=%u G=%u B=%u A=%u\n", tl[0], tl[1], tl[2], tl[3]);
  std::printf("  TR(rect1) R=%u G=%u B=%u A=%u\n", tr[0], tr[1], tr[2], tr[3]);
  std::printf("  BL(rect2) R=%u G=%u B=%u A=%u\n", bl[0], bl[1], bl[2], bl[3]);
  std::printf("  BR(rect3) R=%u G=%u B=%u A=%u\n", br[0], br[1], br[2], br[3]);

  // Each quadrant reads back ITS OWN per-instance color.
  check(tl[0] > 240 && tl[1] < 16  && tl[2] < 16,  "rectColor: rect0 -> RED");
  check(tr[0] < 16  && tr[1] > 240 && tr[2] < 16,  "rectColor: rect1 -> GREEN");
  check(bl[0] < 16  && bl[1] < 16  && bl[2] > 240, "rectColor: rect2 -> BLUE");
  check(br[0] > 240 && br[1] > 240 && br[2] < 16,  "rectColor: rect3 -> YELLOW");

  // The four colors are MUTUALLY DISTINCT — proving per-instance, not a single
  // uniform color (the whole point of the keystone).
  auto rgbDiffer = [](const std::uint8_t* a, const std::uint8_t* b) {
    return a[0] != b[0] || a[1] != b[1] || a[2] != b[2];
  };
  bool allDistinct = rgbDiffer(tl, tr) && rgbDiffer(tl, bl) &&
                     rgbDiffer(tl, br) && rgbDiffer(tr, bl) &&
                     rgbDiffer(tr, br) && rgbDiffer(bl, br);
  check(allDistinct,
        "rectColor: 4 quadrants show 4 DISTINCT colors (per-instance, not uniform)");

  std::printf("=== Dawn instancedRectColor: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
