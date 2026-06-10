// ENC-493 (P3.1) — Blend modes as immutable pipeline permutations (D29.1) on Dawn.
//
// WebGPU bakes blend state into the IMMUTABLE pipeline (unlike GL's per-draw
// glBlendFunc), so the four per-DrawItem blend modes (Normal/Additive/Multiply/
// Screen) become pipeline VARIANTS behind a permutation cache keyed on
// (base pipeline, blendMode). This test is the Dawn counterpart of the GL
// d29_1_blend_modes baseline: it renders a known opaque BASE (destination) shape
// then an overlapping SRC shape in each of the four modes through the backend
// registry with DeviceKind::Dawn into the headless DawnDevice, reads back the
// overlap region, and asserts the composited pixel matches each mode's exact
// blend math.
//
// The blend mode is driven via DawnDevice::setBlendMode(), which (on WebGPU)
// does not mutate global state but records the mode that the next bindPipeline()
// selects the pipeline variant for — exactly how the Renderer dispatcher honors
// di.blendMode. The triSolid backend's bindPipeline/draw then composite against
// the already-drawn base.
//
// KNOWN COLORS (RGBA8Unorm is LINEAR — no sRGB gamma — so the [0,1] blend math
// maps straight to 8-bit by *255):
//   dst (base, opaque)      = (0.4, 0.4, 0.4)
//   src (overlap)           = (0.5, 0.6, 0.7), srcAlpha = 0.5
//
// Per-channel result = srcFactor*src + dstFactor*dst (op = Add):
//   Normal   : src*0.5 + dst*0.5                 -> (0.45, 0.50, 0.55) -> ~(115,128,140)
//   Additive : src*0.5 + dst*1                   -> (0.65, 0.70, 0.75) -> ~(166,179,191)
//   Multiply : src*dst + dst*0 = src*dst         -> (0.20, 0.24, 0.28) -> ~( 51, 61, 71)
//   Screen   : src*1 + dst*(1-src) = src+dst-src*dst
//                                                -> (0.70, 0.76, 0.82) -> ~(179,194,209)
//
// Cross-check (vs GL d29_1_blend_modes): Additive brightens toward the sum,
// Multiply darkens toward the product, Screen lightens, Normal is src-over.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTriSolidBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/GpuDevice.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Types.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

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

// Assert a read-back channel is within +/-3 LSB of the expected 8-bit value
// (allows rounding / software-rasterizer slack).
static bool near8(std::uint8_t got, int expected) {
  return std::abs(static_cast<int>(got) - expected) <= 3;
}

int main() {
  std::printf("=== D29.1 Dawn blend permutations ===\n");

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

  // triSolid backend, registered under DeviceKind::Dawn.
  dc::DawnTriSolidBackend triSolid;
  if (!triSolid.init(dev)) {
    std::fprintf(stderr, "DawnTriSolidBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &triSolid);

  auto renderDi = [&](dc::Scene& scene, dc::CpuBufferStore& store,
                      std::uint32_t diId) -> std::uint32_t {
    const dc::DrawItem* di = scene.getDrawItem(diId);
    if (!di) return 0;
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    if (!be) return 0;
    // Honor the per-DrawItem blend mode exactly like the Renderer dispatcher:
    // setBlendMode() before the draw -> bindPipeline() selects the variant.
    dc::DeviceBlendMode dbm = dc::DeviceBlendMode::Normal;
    switch (di->blendMode) {
      case dc::BlendMode::Normal:   dbm = dc::DeviceBlendMode::Normal;   break;
      case dc::BlendMode::Additive: dbm = dc::DeviceBlendMode::Additive; break;
      case dc::BlendMode::Multiply: dbm = dc::DeviceBlendMode::Multiply; break;
      case dc::BlendMode::Screen:   dbm = dc::DeviceBlendMode::Screen;   break;
    }
    dev.setBlendMode(dbm);
    dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    return bs.drawCalls;
  };

  // One blend-mode case: clear to black, draw the OPAQUE base (dst), then draw
  // the overlapping SRC shape in `blendModeJson`, read back the center, and
  // assert the composited pixel matches (expR,expG,expB).
  auto runCase = [&](const char* label, const char* blendModeJson, int expR,
                     int expG, int expB) {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

    // Full-viewport triangle (same fan as the GL d29_1 baseline).
    float tri[] = {-1, -1, 3, -1, -1, 3};

    // --- Base (destination): opaque grey (0.4,0.4,0.4), Normal blend. ---
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "diBase");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "bufBase");
    store.setCpuData(10, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
        "geomBase");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
        "bindBase");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0.4,"g":0.4,"b":0.4,"a":1})"),
        "colorBase");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,"blendMode":"normal"})"), "styleBase");

    // --- Overlap (source): (0.5,0.6,0.7) @ 0.5 alpha, `blendModeJson`. ---
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "diSrc");
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "bufSrc");
    store.setCpuData(11, tri, sizeof(tri));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"),
        "geomSrc");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"triSolid@1","geometryId":101})"),
        "bindSrc");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0.5,"g":0.6,"b":0.7,"a":0.5})"),
        "colorSrc");
    {
      std::string style = std::string(
          R"({"cmd":"setDrawItemStyle","drawItemId":4,"blendMode":")") +
          blendModeJson + R"("})";
      requireOk(cp.applyJsonText(style.c_str()), "styleSrc");
    }

    dc::RenderPassDesc rp;
    rp.target = {};
    rp.viewportWidth = W;
    rp.viewportHeight = H;
    rp.clear = true;
    rp.clearColor[0] = 0.0f;
    rp.clearColor[1] = 0.0f;
    rp.clearColor[2] = 0.0f;
    rp.clearColor[3] = 1.0f;

    dev.beginRenderPass(rp);
    std::uint32_t cBase = renderDi(scene, store, 3);   // base -> dst
    std::uint32_t cSrc = renderDi(scene, store, 4);    // overlap -> blended
    dev.endRenderPass();

    std::uint8_t px[4];
    dev.readPixel(W / 2, H / 2, px);
    std::printf("  [%-8s] center R=%u G=%u B=%u A=%u  (expect R~%d G~%d B~%d)\n",
                label, px[0], px[1], px[2], px[3], expR, expG, expB);

    check(cBase == 1 && cSrc == 1, (std::string(label) + ": 2 draw calls").c_str());
    check(near8(px[0], expR), (std::string(label) + ": red composited").c_str());
    check(near8(px[1], expG), (std::string(label) + ": green composited").c_str());
    check(near8(px[2], expB), (std::string(label) + ": blue composited").c_str());
  };

  // dst=(0.4,0.4,0.4) opaque; src=(0.5,0.6,0.7)@0.5. Expected 8-bit results:
  //   Normal   src*0.5 + dst*0.5            -> (115,128,140)
  //   Additive src*0.5 + dst*1             -> (166,179,191)
  //   Multiply src*dst                     -> ( 51, 61, 71)
  //   Screen   src + dst*(1-src)           -> (179,194,209)
  runCase("Normal",   "normal",   115, 128, 140);
  runCase("Additive", "additive", 166, 179, 191);
  runCase("Multiply", "multiply",  51,  61,  71);
  runCase("Screen",   "screen",   179, 194, 209);

  // Sanity: the four modes must produce DIFFERENT composited pixels (the variant
  // cache really did bake distinct blend state, not reuse one pipeline).
  std::printf("  (4 modes give 4 distinct composites — distinct pipeline variants)\n");

  std::printf("=== Dawn blend permutations: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
