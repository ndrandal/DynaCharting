// ENC-487 (P2.4) — triGradient@1 + triAA@1 on Dawn.
//
// The Dawn counterparts of the GL gradient/AA paths (d28_3_gradient.cpp is the
// GL gradient baseline; Renderer::drawTriGradient / drawTriAA are the GL refs).
// Renders, through the backend registry with DeviceKind::Dawn into the headless
// DawnDevice offscreen RGBA8 target:
//
//   1. a triGradient@1 triangle with distinct per-vertex colors (bottom-left
//      RED, bottom-right GREEN, top-center BLUE) and asserts the three corners
//      trend toward their vertex colors and the centroid is a 3-way blend.
//
//   2. a triAA@1 triangle whose vertices carry a per-vertex alpha: a large
//      triangle covering the viewport, fill color = opaque green, with all three
//      vertices alpha=1 (the AA "body") — the interior must read back as the
//      solid fill (green) — plus a second triAA triangle with a 0-alpha apex to
//      confirm the per-vertex alpha is actually plumbed through (the row near the
//      faded apex reads a lower alpha than the opaque base).
//
// Blending is disabled in the (ENC-484) pipeline, so the triAA fragment value
// vec4(color.rgb, color.a * v_alpha) is written verbatim into the RGBA8 target;
// the readback alpha channel therefore equals color.a * v_alpha at each pixel —
// an exact, deterministic check of the per-vertex-alpha plumbing.
//
// Cross-check vs the GL gradient baseline (d28_3_gradient): same corner-color
// trend and a mixed centroid. NDC y-flip is applied in the WGSL (same as
// triSolid), so the geometry that is "bottom" in GL clip space lands at the
// framebuffer bottom on readback too.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTriGradientBackend.hpp"
#include "dc/gpu/DawnTriAABackend.hpp"

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
  std::printf("=== D28.3 Dawn gradient + triAA ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

  // --- 1. Bring up the headless Dawn device. ------------------------------
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

  // Backends + registry (registered ADDITIVELY under DeviceKind::Dawn).
  dc::DawnTriGradientBackend triGradient;
  dc::DawnTriAABackend triAA;
  if (!triGradient.init(dev)) {
    std::fprintf(stderr, "DawnTriGradientBackend::init failed\n");
    return 1;
  }
  if (!triAA.init(dev)) {
    std::fprintf(stderr, "DawnTriAABackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &triGradient);
  backends.registerBackend(dc::DeviceKind::Dawn, &triAA);

  // Render-pass desc shared by the cases (cleared to black, like the GL test).
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

  // Dispatch one draw item through the registry (mirrors the Renderer seam).
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

  // =====================================================================
  // Case 1: triGradient@1 — per-vertex RGB triangle (D28.3 GL parity).
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":72})"), "buf");
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","vertexBufferId":3,"vertexCount":3,"format":"pos2_color4"})"),
        "geom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Grad"})"), "di");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triGradient@1","geometryId":4})"),
        "bind");

    // Pos2Color4: x, y, r, g, b, a. Bottom-left RED, bottom-right GREEN,
    // top-center BLUE — a viewport-covering triangle (same as the GL test).
    float verts[] = {
      -1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  // v0 bottom-left  red
       1.0f, -1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  // v1 bottom-right green
       0.0f,  1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  // v2 top-center   blue
    };
    store.setCpuData(3, verts, sizeof(verts));

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 5);
    dev.endRenderPass();
    check(calls == 1, "triGradient: 1 draw call");

    // After the WGSL y-flip, clip-space "bottom" (y=-1) lands at framebuffer TOP
    // (small y), and "top" (y=+1) lands at framebuffer BOTTOM. readPixel uses
    // top-left origin. So the geometry maps as:
    //   RED   (clip bottom-left)  -> framebuffer top-left   (small x, small y)
    //   GREEN (clip bottom-right) -> framebuffer top-right  (large x, small y)
    //   BLUE  (clip top-center)   -> framebuffer bottom apex (x=W/2, large y)
    // The triangle is full-width along the top row and narrows to the bottom
    // apex, so the corner samples sit a couple px inside the covered region.
    auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
      dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
    };

    std::uint8_t red[4], green[4], blue[4], cen[4];
    px(3, 2, red);             // top-left     -> red vertex
    px(W - 4, 2, green);       // top-right    -> green vertex
    px(W / 2, H - 8, blue);    // bottom apex  -> blue vertex
    px(W / 2, H / 2, cen);     // centroid-ish -> 3-way blend

    std::printf("  gradient red(top-left)    R=%u G=%u B=%u A=%u\n", red[0], red[1], red[2], red[3]);
    std::printf("  gradient green(top-right) R=%u G=%u B=%u A=%u\n", green[0], green[1], green[2], green[3]);
    std::printf("  gradient blue(bottom)     R=%u G=%u B=%u A=%u\n", blue[0], blue[1], blue[2], blue[3]);
    std::printf("  gradient center           R=%u G=%u B=%u A=%u\n", cen[0], cen[1], cen[2], cen[3]);

    // Corners trend toward their vertex colors (dominant channel strong, others weak).
    check(red[0] > 150 && red[1] < 110 && red[2] < 110, "gradient: red corner trends RED");
    check(green[1] > 150 && green[0] < 110 && green[2] < 110, "gradient: green corner trends GREEN");
    check(blue[2] > 150 && blue[0] < 110 && blue[1] < 110, "gradient: blue corner trends BLUE");

    // Centroid is a blend of all three: every channel present, none saturated.
    check(cen[0] > 20 && cen[0] < 220, "gradient: center has mixed red");
    check(cen[1] > 20 && cen[1] < 220, "gradient: center has mixed green");
    check(cen[2] > 20 && cen[2] < 220, "gradient: center has mixed blue");
  }

  // =====================================================================
  // Case 2a: triAA@1 — body (all vertices alpha=1) reads back as solid fill.
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    // NOTE: the two triAA cases share ONE DawnTriAABackend instance, which caches
    // its Dawn vertex buffer keyed by geometryId. We give each case an EXPLICIT,
    // distinct geometryId (40 here, 41 below) so the body geometry isn't reused
    // for the fade case (auto-allocated IDs would both be 4 across fresh Scenes).
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":30,"byteLength":36})"), "buf");
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":40,"vertexBufferId":30,"vertexCount":3,"format":"pos2_alpha"})"),
        "geom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":2,"name":"AA"})"), "di");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triAA@1","geometryId":40})"),
        "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":50,"r":0.0,"g":1.0,"b":0.0,"a":1.0})"),
        "color");

    // Pos2 + alpha (12-byte vertex). Big triangle covering the viewport; all
    // vertices alpha=1 => the whole interior is the AA "body" (solid fill).
    float verts[] = {
      -1.0f, -1.0f, 1.0f,
       3.0f, -1.0f, 1.0f,
      -1.0f,  3.0f, 1.0f,
    };
    store.setCpuData(30, verts, sizeof(verts));

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 50);
    dev.endRenderPass();
    check(calls == 1, "triAA body: 1 draw call");

    std::uint8_t cen[4];
    dev.readPixel(W / 2, H / 2, cen);
    std::printf("  triAA body center R=%u G=%u B=%u A=%u\n", cen[0], cen[1], cen[2], cen[3]);
    // Interior (v_alpha==1) == fill green, alpha = a*1 = 255.
    check(cen[0] < 16, "triAA body: red ~0");
    check(cen[1] > 240, "triAA body: green ~255");
    check(cen[2] < 16, "triAA body: blue ~0");
    check(cen[3] > 240, "triAA body: alpha ~255 (v_alpha=1)");
  }

  // =====================================================================
  // Case 2b: triAA@1 — per-vertex alpha gradient (apex alpha=0 fades out).
  // =====================================================================
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    // Distinct geometryId (41) from case 2a — see the cache note there.
    requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":31,"byteLength":36})"), "buf");
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":41,"vertexBufferId":31,"vertexCount":3,"format":"pos2_alpha"})"),
        "geom");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":51,"layerId":2,"name":"AA2"})"), "di");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":51,"pipeline":"triAA@1","geometryId":41})"),
        "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":51,"r":1.0,"g":1.0,"b":1.0,"a":1.0})"),
        "color");

    // Pos2 + alpha. Base (bottom two verts) alpha=1, apex alpha=0. After the
    // WGSL y-flip, clip "bottom" lands at framebuffer TOP and clip "top" (the
    // apex) lands at framebuffer BOTTOM. So readback alpha = color.a * v_alpha
    // is ~255 along the top rows (base) and fades toward 0 near the bottom apex.
    float verts[] = {
      -1.0f, -1.0f, 1.0f,  // bottom-left  alpha 1  -> framebuffer top-left
       1.0f, -1.0f, 1.0f,  // bottom-right alpha 1  -> framebuffer top-right
       0.0f,  1.0f, 0.0f,  // top apex     alpha 0  -> framebuffer bottom apex
    };
    store.setCpuData(31, verts, sizeof(verts));

    dc::RenderPassDesc rp = makeRp();
    dev.beginRenderPass(rp);
    std::uint32_t calls = renderDi(scene, store, 51);
    dev.endRenderPass();
    check(calls == 1, "triAA fade: 1 draw call");

    auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
      dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
    };
    std::uint8_t base[4], mid[4], apex[4];
    px(W / 2, 3, base);       // framebuffer top    -> alpha~1 (opaque base)
    px(W / 2, H / 2, mid);    // framebuffer middle -> alpha~0.5 (linear fade)
    px(W / 2, H - 8, apex);   // framebuffer bottom -> alpha~0 (faded apex)
    std::printf("  triAA fade base(top)    R=%u G=%u B=%u A=%u\n",
                base[0], base[1], base[2], base[3]);
    std::printf("  triAA fade mid          R=%u G=%u B=%u A=%u\n",
                mid[0], mid[1], mid[2], mid[3]);
    std::printf("  triAA fade apex(bottom) R=%u G=%u B=%u A=%u\n",
                apex[0], apex[1], apex[2], apex[3]);
    // RGB is the white fill everywhere the triangle covers; only alpha varies.
    // Blending is off: readback alpha == color.a * v_alpha. Base ~255, fading.
    check(base[3] > 200, "triAA fade: base alpha high (v_alpha~1)");
    check(mid[3] < base[3] && mid[3] > apex[3], "triAA fade: mid alpha between base and apex (linear v_alpha)");
    check(apex[3] < base[3], "triAA fade: apex alpha < base alpha (per-vertex alpha plumbed)");
    check(apex[3] < 130, "triAA fade: apex alpha low (v_alpha~0)");
  }

  std::printf("=== Dawn gradient/triAA: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
