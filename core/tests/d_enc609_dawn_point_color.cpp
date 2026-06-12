// ENC-609 (P2.2) — instancedPointColor@1 on Dawn: the per-POINT color+size
// scatter. The Dawn render proof that ONE instanced draw can paint a set of dots
// each with a DIFFERENT color (read from the per-instance packed RGBA8) and a
// DIFFERENT size (read from the per-instance size lane) — NOT a uniform color /
// 1px point. The per-point sibling of d_enc608_dawn_rect_color.
//
// Point4Color instance record (16B): pos2 f32 (x,y) @0 + packed RGBA8 @8 + size
// f32 @12. Color byte order: R,G,B,A low..high. Size is the dot diameter in
// PIXELS (a quad expanded ±size/2 about the transformed center via the viewport).
//
// NDC Y-FLIP: clip-space y is negated in the WGSL (same as every backend), so a
// clip-bottom point lands at framebuffer-top (noted at each read).
//
// On this headless box the only Vulkan backend may be lavapipe (software). Force:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedPointColorBackend.hpp"

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

// Pack one Point4Color instance (16B) into `out`.
static void pushPoint(std::vector<std::uint8_t>& out, float x, float y,
                      std::uint32_t rgba8, float size) {
  auto pf = [&](float v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  auto pu = [&](std::uint32_t v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  pf(x); pf(y);
  pu(rgba8);  // packed RGBA8 color (per point)
  pf(size);   // diameter in pixels
}

int main() {
  std::printf("=== ENC-609 Dawn instancedPointColor (per-point color + size) ===\n");

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

  dc::DawnInstancedPointColorBackend instPointColor;
  if (!instPointColor.init(dev)) {
    std::fprintf(stderr, "DawnInstancedPointColorBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &instPointColor);

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
  // Four LARGE dots, one per quadrant, each a DIFFERENT color + size, in ONE
  // instanced draw. Big sizes so each dot covers its quadrant center pixel.
  //   pt0 (clip TL): red    size 28   pt1 (clip TR): green  size 28
  //   pt2 (clip BL): blue   size 20   pt3 (clip BR): yellow size 20
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

  // Centers at the four quadrant centers in clip space (±0.5, ±0.5).
  std::vector<std::uint8_t> verts;
  pushPoint(verts, -0.5f,  0.5f, kRed,    28.0f);  // clip TL
  pushPoint(verts,  0.5f,  0.5f, kGreen,  28.0f);  // clip TR
  pushPoint(verts, -0.5f, -0.5f, kBlue,   20.0f);  // clip BL
  pushPoint(verts,  0.5f, -0.5f, kYellow, 20.0f);  // clip BR

  requireOk(cp.applyJsonText(
      R"({"cmd":"createBuffer","id":10,"byteLength":64})"), "buf");
  store.setCpuData(10, verts.data(),
                   static_cast<std::uint32_t>(verts.size()));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":4,"format":"point4_color"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedPointColor@1","geometryId":100})"),
      "bind");

  dc::RenderPassDesc rp = makeRp();
  dev.beginRenderPass(rp);
  std::uint32_t calls = renderDi(scene, store, 3);
  dev.endRenderPass();
  check(calls == 1, "pointColor: 1 instanced draw call (4 instances)");

  // The WGSL negates clip.y (same convention as every backend, incl. the proven
  // instancedRectColor@1 keystone), so a clip-TOP point lands at framebuffer-
  // BOTTOM. Hence the per-point centers map:
  //   pt0 (clip TL, y=+0.5) -> framebuffer BOTTOM-left  -> red
  //   pt1 (clip TR, y=+0.5) -> framebuffer BOTTOM-right -> green
  //   pt2 (clip BL, y=-0.5) -> framebuffer TOP-left     -> blue
  //   pt3 (clip BR, y=-0.5) -> framebuffer TOP-right    -> yellow
  // Sample names below are FRAMEBUFFER positions; the variable suffix names the pt.
  std::uint8_t tl[4], tr[4], bl[4], br[4];
  px(W / 4,     3 * H / 4, tl);  // fb bottom-left  -> pt0 red
  px(3 * W / 4, 3 * H / 4, tr);  // fb bottom-right -> pt1 green
  px(W / 4,     H / 4,     bl);  // fb top-left     -> pt2 blue
  px(3 * W / 4, H / 4,     br);  // fb top-right    -> pt3 yellow
  std::printf("  pt0(fb BL) R=%u G=%u B=%u A=%u\n", tl[0], tl[1], tl[2], tl[3]);
  std::printf("  pt1(fb BR) R=%u G=%u B=%u A=%u\n", tr[0], tr[1], tr[2], tr[3]);
  std::printf("  pt2(fb TL) R=%u G=%u B=%u A=%u\n", bl[0], bl[1], bl[2], bl[3]);
  std::printf("  pt3(fb TR) R=%u G=%u B=%u A=%u\n", br[0], br[1], br[2], br[3]);

  // Each dot reads back ITS OWN per-instance color.
  check(tl[0] > 240 && tl[1] < 16  && tl[2] < 16,  "pointColor: pt0 -> RED");
  check(tr[0] < 16  && tr[1] > 240 && tr[2] < 16,  "pointColor: pt1 -> GREEN");
  check(bl[0] < 16  && bl[1] < 16  && bl[2] > 240, "pointColor: pt2 -> BLUE");
  check(br[0] > 240 && br[1] > 240 && br[2] < 16,  "pointColor: pt3 -> YELLOW");

  // The four colors are MUTUALLY DISTINCT — proving per-point, not uniform color.
  auto rgbDiffer = [](const std::uint8_t* a, const std::uint8_t* b) {
    return a[0] != b[0] || a[1] != b[1] || a[2] != b[2];
  };
  bool allDistinct = rgbDiffer(tl, tr) && rgbDiffer(tl, bl) &&
                     rgbDiffer(tl, br) && rgbDiffer(tr, bl) &&
                     rgbDiffer(tr, br) && rgbDiffer(bl, br);
  check(allDistinct,
        "pointColor: 4 dots show 4 DISTINCT colors (per-point, not uniform)");

  // PER-POINT SIZE proof: a big dot (size 28) covers a pixel further from its
  // center than a small dot (size 20) would. Sample a pixel offset ~12px from
  // the red (size-28) center: it must STILL be red (the 28px dot reaches it).
  // The same offset from the blue (size-20) center must be the BLACK clear
  // (the 20px dot does NOT reach 12px out, i.e. > size/2 = 10). This only holds
  // if size is read PER POINT, not a shared uniform.
  std::uint8_t bigEdge[4], smallEdge[4];
  // Red (size 28) center is framebuffer (W/4, 3H/4) = (16,48). 12px right -> x=28.
  px(W / 4 + 12, 3 * H / 4, bigEdge);
  // Blue (size 20) center is framebuffer (W/4, H/4) = (16,16). 12px right -> x=28.
  px(W / 4 + 12, H / 4, smallEdge);
  std::printf("  bigEdge(+12 from red,  size28) R=%u G=%u B=%u\n",
              bigEdge[0], bigEdge[1], bigEdge[2]);
  std::printf("  smallEdge(+12 from blue, size20) R=%u G=%u B=%u\n",
              smallEdge[0], smallEdge[1], smallEdge[2]);
  bool bigStillCovered = bigEdge[0] > 240 && bigEdge[1] < 16 && bigEdge[2] < 16;
  bool smallNotCovered =
      smallEdge[0] < 16 && smallEdge[1] < 16 && smallEdge[2] < 16;
  check(bigStillCovered,
        "pointColor: 12px from the size-28 dot is STILL covered (large size)");
  check(smallNotCovered,
        "pointColor: 12px from the size-20 dot is NOT covered (small size)");
  check(bigStillCovered && smallNotCovered,
        "pointColor: per-point SIZE distinguishes the dots (not a shared uniform)");

  std::printf("=== Dawn instancedPointColor: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
