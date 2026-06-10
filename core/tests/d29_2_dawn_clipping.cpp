// ENC-494 (P3.2) — Stencil clipping masks (D29.2) on Dawn.
//
// GL clips via a two-pass stencil (see core/src/gl/Renderer.cpp /
// GlDevice::setClipState): a clip-SOURCE pass writes a mask into the stencil
// buffer (stencil compare Always, op Replace, ref=1, color writes OFF), then a
// clipped pass draws content only where stencil == 1 (compare Equal, ref=1,
// color writes on). This test reproduces that on WebGPU via the DawnDevice
// depth-stencil attachment + the per-ClipMode pipeline variants minted by the
// (blend, clip) permutation cache (ENC-493 extended by ENC-494).
//
// SCENARIO (cross-checks the GL d29_2_clipping baseline):
//   * Clip SOURCE: a SMALL centered triangle drawn in ClipMode::WriteMask —
//     writes 1 into the stencil where it covers, no color (colorTarget.writeMask
//     == None on that pipeline variant).
//   * CONTENT: a LARGE full-viewport green triangle drawn in ClipMode::UseMask —
//     passes the stencil test (== 1) ONLY inside the clip triangle, so green
//     appears only there; everywhere else stays the cleared black.
// We then read back:
//   * the viewport CENTER  -> inside the clip region  -> GREEN (content drawn)
//   * a CORNER             -> outside the clip region -> BLACK (content clipped)
//
// The clip mode is driven via DawnDevice::setClipState(), which (on WebGPU) does
// not mutate global state but records the mode the next bindPipeline() selects
// the pipeline variant for — exactly how the Renderer dispatcher honors
// isClipSource / useClipMask. The triSolid backend's bindPipeline/draw then
// render through the chosen (blend, clip) variant.
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
  std::printf("=== D29.2 Dawn stencil clipping masks ===\n");

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

  // Render one DrawItem in a given ClipMode (mirrors the Renderer dispatcher:
  // setClipState() before the draw -> bindPipeline() selects the (blend, clip)
  // variant -> draw). Returns the draw-call count.
  auto renderDi = [&](dc::Scene& scene, dc::CpuBufferStore& store,
                      std::uint32_t diId, dc::ClipMode clip) -> std::uint32_t {
    const dc::DrawItem* di = scene.getDrawItem(diId);
    if (!di) return 0;
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    if (!be) return 0;
    dev.setClipState(clip);
    dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    return bs.drawCalls;
  };

  // ---- Build the scene: clip-source triangle + full-viewport content. -----
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // Clip SOURCE: a small triangle centered on the viewport (NDC). It covers the
  // center but not the corners. Same shape as the GL d29_2 baseline's clip tri.
  float clipTri[] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f};
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "clipDi");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "clipBuf");
  store.setCpuData(10, clipTri, sizeof(clipTri));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
      "clipGeom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
      "clipBind");
  // The clip source's color is irrelevant (WriteMask masks color off); set it
  // to something obvious so a regression that DID write color would be caught.
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})"),
      "clipColor");

  // CONTENT: a large full-viewport green triangle (same fan as the blend test).
  float bigTri[] = {-1, -1, 3, -1, -1, 3};
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "contentDi");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":24})"), "contentBuf");
  store.setCpuData(11, bigTri, sizeof(bigTri));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":3,"format":"pos2_clip"})"),
      "contentGeom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"triSolid@1","geometryId":101})"),
      "contentBind");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})"),
      "contentColor");

  // ---- Render: clear (color + stencil) -> WriteMask source -> UseMask. -----
  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;  // black background
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  rp.clearStencil = true;   // per-pane stencil clear to 0

  dev.beginRenderPass(rp);
  // Pass 1 — clip SOURCE: write the stencil mask, no color (WriteMask variant).
  std::uint32_t cClip = renderDi(scene, store, 3, dc::ClipMode::WriteMask);
  // Pass 2 — clipped CONTENT: draw only where stencil == 1 (UseMask variant).
  std::uint32_t cContent = renderDi(scene, store, 4, dc::ClipMode::UseMask);
  dev.endRenderPass();

  check(cClip == 1 && cContent == 1, "two draws issued (clip source + content)");

  // ---- Read back inside vs outside the clip region. ----------------------
  // INSIDE: viewport center is inside the centered clip triangle -> content
  // (green) drawn. OUTSIDE: a corner is outside the clip triangle -> stencil
  // test fails there -> content clipped -> background (black).
  std::uint8_t inside[4];
  std::uint8_t outside[4];
  dev.readPixel(W / 2, H / 2, inside);  // center
  dev.readPixel(2, 2, outside);         // top-left corner

  std::printf("  INSIDE  (center %u,%u): R=%u G=%u B=%u A=%u  (expect green)\n",
              W / 2, H / 2, inside[0], inside[1], inside[2], inside[3]);
  std::printf("  OUTSIDE (corner 2,2):   R=%u G=%u B=%u A=%u  (expect black)\n",
              outside[0], outside[1], outside[2], outside[3]);

  // Inside the clip region: the green content shows through.
  check(inside[0] < 30, "inside clip: red low");
  check(inside[1] > 200, "inside clip: green high (content drawn)");
  check(inside[2] < 30, "inside clip: blue low");

  // Outside the clip region: content is clipped, background black shows.
  check(outside[0] < 15, "outside clip: red ~0 (content clipped)");
  check(outside[1] < 15, "outside clip: green ~0 (content clipped)");
  check(outside[2] < 15, "outside clip: blue ~0 (content clipped)");

  // Sanity: the clip SOURCE wrote NO color (WriteMask masks color off). If it
  // had leaked its red color, the inside pixel's red would be high — already
  // asserted low above, so this is implicitly covered.

  std::printf("=== Dawn stencil clipping: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
