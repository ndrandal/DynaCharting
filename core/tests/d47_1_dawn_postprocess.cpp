// ENC-496 (P3.4) — Post-process passes (D47 / D78.2) on Dawn.
//
// The LAST Phase-3 ticket: render a scene into an offscreen texture, then chain
// fullscreen effect passes that each SAMPLE the previous pass's output. The Dawn
// mirror of the GL PostProcessPass / PostProcessStack (core/src/gl/
// PostProcessPass.cpp) + BuiltinEffects (core/src/gl/BuiltinEffects.cpp), and a
// cross-check of the GL d47_1_post_process / d47_2_blur_pass / d78_2_builtin_
// effects baselines.
//
// CHAIN DESIGN (render-to-texture, built on the ENC-495 multi-target support):
//   scene target (id 2)  --[pass 0 samples it]-->  output target (id 3)  --> ...
// The scene/intermediate targets carry TextureBinding usage (ENC-496) so a pass
// can sample them; ids 0/1 stay reserved for the main + pick targets.
//
// SCENARIOS:
//   * Test 1 (vignette): render a flat bright-grey full-screen scene, apply the
//     vignette pass (port of kVignetteFragSrc). Assert the CENTER stays bright
//     and the CORNERS are darkened (center > corner), matching the GL vignette
//     (d78_2) semantics: vignette darkens toward the corners.
//   * Test 2 (blur): render a split scene (LEFT bright / RIGHT dark, a sharp
//     vertical edge), apply the horizontal blur pass (port of the Gaussian
//     kBloomBlurFragSrc / d47_2 blur). Assert the edge pixel becomes an
//     INTERMEDIATE value (a gradient between the two colors) while far-left stays
//     bright and far-right stays dark — exactly the GL d47_2 blur assertions.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnPostProcess.hpp"
#include "dc/gpu/DawnTriSolidBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
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

namespace {

constexpr std::uint32_t W = 64;
constexpr std::uint32_t H = 64;
constexpr std::uint32_t kSceneTarget = 2;  // ids 0/1 reserved (main/pick)

// Render a full-screen colored triangle (covering the whole viewport) of the
// given color into render target `targetId`, via the DawnTriSolidBackend. A
// single triangle with clip verts (-1,-1),(3,-1),(-1,3) covers [-1,1]^2.
void renderFullScreen(dc::DawnDevice& dev, dc::DawnTriSolidBackend& triSolid,
                      std::uint32_t targetId, float r, float g, float b) {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di");
  float verts[] = {-1, -1, 3, -1, -1, 3};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf");
  store.setCpuData(10, verts, sizeof(verts));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})"),
      "bind");
  char colorCmd[160];
  std::snprintf(colorCmd, sizeof(colorCmd),
                R"({"cmd":"setDrawItemColor","drawItemId":5,"r":%f,"g":%f,"b":%f,"a":1.0})",
                r, g, b);
  requireOk(cp.applyJsonText(colorCmd), "color");

  dc::RenderPassDesc rp;
  rp.target.id = targetId;
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  dev.beginRenderPass(rp);
  const dc::DrawItem* di = scene.getDrawItem(5);
  triSolid.renderDrawItem(dev, scene, store, *di, static_cast<int>(W),
                          static_cast<int>(H));
  dev.endRenderPass();
}

// Render a split scene into `targetId`: LEFT half color (rl,gl,bl), RIGHT half
// color (rr,gr,br), as two triSolid triangles (quad halves). x is NOT y-flipped,
// so "left" stays on the framebuffer's left.
void renderSplit(dc::DawnDevice& dev, dc::DawnTriSolidBackend& triSolid,
                 std::uint32_t targetId, float lum_l, float lum_r) {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // LEFT half: two triangles covering clip x in [-1,0], y in [-1,1].
  float left[] = {-1, -1, 0, -1, 0, 1, -1, -1, 0, 1, -1, 1};
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "diL");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":48})"), "bufL");
  store.setCpuData(10, left, sizeof(left));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":6,"format":"pos2_clip"})"),
      "geomL");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":100})"),
      "bindL");
  char cl[160];
  std::snprintf(cl, sizeof(cl),
                R"({"cmd":"setDrawItemColor","drawItemId":5,"r":%f,"g":%f,"b":%f,"a":1.0})",
                lum_l, lum_l, lum_l);
  requireOk(cp.applyJsonText(cl), "colorL");

  // RIGHT half: clip x in [0,1].
  float right[] = {0, -1, 1, -1, 1, 1, 0, -1, 1, 1, 0, 1};
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":6,"layerId":2})"), "diR");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":48})"), "bufR");
  store.setCpuData(11, right, sizeof(right));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":6,"format":"pos2_clip"})"),
      "geomR");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":6,"pipeline":"triSolid@1","geometryId":101})"),
      "bindR");
  char cr[160];
  std::snprintf(cr, sizeof(cr),
                R"({"cmd":"setDrawItemColor","drawItemId":6,"r":%f,"g":%f,"b":%f,"a":1.0})",
                lum_r, lum_r, lum_r);
  requireOk(cp.applyJsonText(cr), "colorR");

  dc::RenderPassDesc rp;
  rp.target.id = targetId;
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  dev.beginRenderPass(rp);
  triSolid.renderDrawItem(dev, scene, store, *scene.getDrawItem(5),
                          static_cast<int>(W), static_cast<int>(H));
  triSolid.renderDrawItem(dev, scene, store, *scene.getDrawItem(6),
                          static_cast<int>(W), static_cast<int>(H));
  dev.endRenderPass();
}

}  // namespace

int main() {
  std::printf("=== D47/D78.2 Dawn post-process passes ===\n");

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

  dc::DawnTriSolidBackend triSolid;
  if (!triSolid.init(dev)) {
    std::fprintf(stderr, "DawnTriSolidBackend::init failed\n");
    return 1;
  }

  // -- Test 1: vignette darkens the corners ---------------------------------
  {
    std::printf("\n-- Test 1: vignette (center brighter than corners) --\n");
    // Render a flat bright-grey scene (0.8) into the scene target.
    renderFullScreen(dev, triSolid, kSceneTarget, 0.8f, 0.8f, 0.8f);

    // Pre-effect: read the scene target back (it is the active target after the
    // scene render) to record the un-vignetted corner/center.
    std::uint8_t preCenter[4] = {0, 0, 0, 0};
    std::uint8_t preCorner[4] = {0, 0, 0, 0};
    dev.readPixel(W / 2, H / 2, preCenter);
    dev.readPixel(1, 1, preCorner);
    std::printf("  scene (pre-effect)  center R=%u  corner R=%u\n",
                preCenter[0], preCorner[0]);

    // Build the chain: one vignette pass (strength 0.9, radius 0.2 — strong, so
    // the corners are clearly darkened).
    dc::DawnPostProcessStack stack;
    stack.init(/*sceneTargetId=*/kSceneTarget, /*firstPassTargetId=*/3);
    check(stack.addPass(dev, "vignette", dc::vignetteEffectWgsl(),
                        /*strength=*/0.9f, /*radius=*/0.2f),
          "vignette pass built");
    check(stack.passCount() == 1, "stack has 1 pass");

    const std::uint32_t outId = stack.apply(dev, W, H);

    std::uint8_t postCenter[4] = {0, 0, 0, 0};
    std::uint8_t postCorner[4] = {0, 0, 0, 0};
    dev.readPixel(W / 2, H / 2, postCenter);
    dev.readPixel(1, 1, postCorner);
    std::printf("  vignette (post)     center R=%u  corner R=%u  (out target=%u)\n",
                postCenter[0], postCorner[0], outId);

    // The scene was flat (center == corner pre-effect). After vignette the
    // center stays bright (radius 0.2 leaves the center essentially untouched)
    // and the corners are darkened: center > corner.
    check(preCenter[0] > 180 && preCorner[0] > 180,
          "pre-effect scene is flat & bright (center ~= corner)");
    check(postCenter[0] > postCorner[0] + 30,
          "post vignette: center clearly brighter than corner");
    check(postCenter[0] > 180, "post vignette: center stays bright");
    check(postCorner[0] < preCorner[0] - 20,
          "post vignette: corner darkened vs the flat scene");
  }

  // -- Test 2: blur turns a sharp edge into a gradient ----------------------
  {
    std::printf("\n-- Test 2: blur (sharp edge -> gradient) --\n");
    // Render a split scene: LEFT bright (0.9), RIGHT dark (0.05).
    renderSplit(dev, triSolid, kSceneTarget, /*lum_l=*/0.9f, /*lum_r=*/0.05f);

    // Pre-effect: the scene target holds a sharp edge.
    std::uint8_t preFarL[4] = {0, 0, 0, 0};
    std::uint8_t preFarR[4] = {0, 0, 0, 0};
    dev.readPixel(5, H / 2, preFarL);
    dev.readPixel(W - 6, H / 2, preFarR);
    std::printf("  scene (pre-effect)  farLeft R=%u  farRight R=%u\n",
                preFarL[0], preFarR[0]);
    check(preFarL[0] > 200, "pre-effect: left side bright");
    check(preFarR[0] < 40, "pre-effect: right side dark");

    // One horizontal blur pass (param0 = 0 == horizontal). Use a fresh stack /
    // intermediate target.
    dc::DawnPostProcessStack stack;
    stack.init(/*sceneTargetId=*/kSceneTarget, /*firstPassTargetId=*/4);
    check(stack.addPass(dev, "blur_h", dc::blurEffectWgsl(),
                        /*direction=*/0.0f),
          "blur pass built");

    stack.apply(dev, W, H);

    // Sample across the edge: far-left, the edge column (x = W/2), far-right.
    std::uint8_t postFarL[4] = {0, 0, 0, 0};
    std::uint8_t postEdge[4] = {0, 0, 0, 0};
    std::uint8_t postFarR[4] = {0, 0, 0, 0};
    dev.readPixel(5, H / 2, postFarL);
    dev.readPixel(W / 2, H / 2, postEdge);
    dev.readPixel(W - 6, H / 2, postFarR);
    std::printf("  blur (post)         farLeft R=%u  edge R=%u  farRight R=%u\n",
                postFarL[0], postEdge[0], postFarR[0]);

    // The edge becomes an intermediate value (a gradient between the two colors);
    // far-left stays bright and far-right stays dark — the GL d47_2 assertions.
    check(postEdge[0] > 30 && postEdge[0] < 225,
          "blur: edge pixel is intermediate (gradient between the colors)");
    check(postFarL[0] > 200, "blur: far-left still bright");
    check(postFarR[0] < 50, "blur: far-right still dark");
  }

  std::printf("\n=== D47/D78.2 Dawn post-process: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
