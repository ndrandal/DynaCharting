// ENC-489 (P2.6) — instancedCandle@1 on Dawn.
//
// The Dawn counterpart of the GL instancedCandle path (Renderer::
// drawInstancedCandle, kInstCandleVert/kInstCandleFrag) and the D14.6 candle
// semantics (d14_6_volume_gl.cpp). Renders a row of OHLC candles through the
// backend registry with DeviceKind::Dawn into the headless DawnDevice offscreen
// RGBA8 target, reads back, and asserts:
//
//   * UP candles (close >= open) read back the up color at their body center.
//   * DOWN candles (close < open) read back the down color at their body center.
//   * Each candle's wick is present ABOVE and BELOW the body (thin vertical line
//     at the high / low, on the candle's x).
//   * The gaps BETWEEN candles read back CLEAR.
//
// GEOMETRY / Y-FLIP: a candle6 record is (cx, open, high, low, close, hw). The
// vertex shader expands 12 verts/instance (6 body + 6 wick) and negates clip.y
// (same convention as triSolid / instancedRect) so the WebGPU top-left
// framebuffer matches the GL bottom-left readback. With the identity transform
// used here, data y maps directly to clip y, so after the flip the HIGH (large y)
// lands near framebuffer TOP and the LOW near framebuffer BOTTOM.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedCandleBackend.hpp"

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
  std::printf("=== D14.6 Dawn instancedCandle ===\n");

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
  dc::DawnInstancedCandleBackend candle;
  if (!candle.init(dev)) {
    std::fprintf(stderr, "DawnInstancedCandleBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &candle);

  // ----- Scene: three candles (UP, DOWN, UP), each with a wick. -----------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  // candle6 = (cx, open, high, low, close, hw). Identity transform -> data is
  // clip space. Body spans min(open,close)..max(open,close); wick spans low..high
  // at cx. hw=0.15 in clip x.
  //   candle 0: cx=-0.5  UP   (close 0.3 >= open -0.3)  body -0.3..0.3
  //   candle 1: cx= 0.0  DOWN (close -0.3 < open 0.3)   body -0.3..0.3
  //   candle 2: cx= 0.5  UP                              body -0.3..0.3
  // high=0.85 / low=-0.85 for all -> wicks extend well past the body, reaching
  // near the framebuffer top/bottom (rows ~10 / ~118) so the high/low probes
  // below land squarely on lit wick pixels.
  float candles[] = {
    -0.5f, -0.3f, 0.85f, -0.85f,  0.3f, 0.15f,  // UP
     0.0f,  0.3f, 0.85f, -0.85f, -0.3f, 0.15f,  // DOWN
     0.5f, -0.3f, 0.85f, -0.85f,  0.3f, 0.15f,  // UP
  };
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})"), "buf");
  store.setCpuData(10, candles, sizeof(candles));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"candle6"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedCandle@1","geometryId":100})"),
      "bind");
  // Up = green, Down = red (the GL defaults; set explicitly via setDrawItemStyle).
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
      R"("colorUpR":0,"colorUpG":1,"colorUpB":0,"colorUpA":1,)"
      R"("colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})"), "style");

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
  const dc::DrawItem* di = scene.getDrawItem(3);
  dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
  dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                           static_cast<int>(W),
                                           static_cast<int>(H));
  dev.endRenderPass();
  check(bs.drawCalls == 1, "candles: 1 instanced draw call (3 candles)");

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };

  // clip x in [-1,1] -> framebuffer x in [0,W]:  px = (clip.x*0.5 + 0.5)*W.
  //   cx=-0.5 -> 32,  cx=0.0 -> 64,  cx=0.5 -> 96.
  // clip y is negated then mapped: data y -0.85 (low) -> fb top (~row 10), +0.85
  // (high) -> fb bottom (~row 118). Body center (y=0) -> fb y = H/2 = 64. Both
  // probe rows below sit inside the wick's vertical span (~10..118), above and
  // below the body (rows ~45..83).
  constexpr std::uint32_t cxPix[3] = {32, 64, 96};
  constexpr std::uint32_t bodyY    = H / 2;  // body center row (data y ~ 0)
  constexpr std::uint32_t highY    = 12;     // near top  (inside wick span)
  constexpr std::uint32_t lowY     = 116;    // near bot  (inside wick span)

  // --- Body centers: candle 0/2 UP (green), candle 1 DOWN (red). -----------
  std::uint8_t b0[4], b1[4], b2[4];
  px(cxPix[0], bodyY, b0);
  px(cxPix[1], bodyY, b1);
  px(cxPix[2], bodyY, b2);
  std::printf("  body0 (UP)   x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[0], bodyY, b0[0], b0[1], b0[2], b0[3]);
  std::printf("  body1 (DOWN) x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[1], bodyY, b1[0], b1[1], b1[2], b1[3]);
  std::printf("  body2 (UP)   x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[2], bodyY, b2[0], b2[1], b2[2], b2[3]);
  check(b0[1] > 200 && b0[0] < 40 && b0[2] < 40, "candle0 body center is UP color (green)");
  check(b1[0] > 200 && b1[1] < 40 && b1[2] < 40, "candle1 body center is DOWN color (red)");
  check(b2[1] > 200 && b2[0] < 40 && b2[2] < 40, "candle2 body center is UP color (green)");

  // --- Wicks: present above (high) and below (low) the body, on candle x. ---
  // The wick is the SAME up/down color as its candle; here we just assert it is
  // lit (non-clear) at the high/low rows for candle 0 (green) and candle 1 (red).
  std::uint8_t w0hi[4], w0lo[4], w1hi[4], w1lo[4];
  px(cxPix[0], highY, w0hi);
  px(cxPix[0], lowY,  w0lo);
  px(cxPix[1], highY, w1hi);
  px(cxPix[1], lowY,  w1lo);
  std::printf("  wick0 high x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[0], highY, w0hi[0], w0hi[1], w0hi[2], w0hi[3]);
  std::printf("  wick0 low  x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[0], lowY,  w0lo[0], w0lo[1], w0lo[2], w0lo[3]);
  std::printf("  wick1 high x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[1], highY, w1hi[0], w1hi[1], w1hi[2], w1hi[3]);
  std::printf("  wick1 low  x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[1], lowY,  w1lo[0], w1lo[1], w1lo[2], w1lo[3]);
  check(w0hi[1] > 150 && w0hi[0] < 60, "candle0 wick present above body (green @ high)");
  check(w0lo[1] > 150 && w0lo[0] < 60, "candle0 wick present below body (green @ low)");
  check(w1hi[0] > 150 && w1hi[1] < 60, "candle1 wick present above body (red @ high)");
  check(w1lo[0] > 150 && w1lo[1] < 60, "candle1 wick present below body (red @ low)");

  // --- Wick is THIN: a few px to the side of cx (but off the body) is clear. --
  // Body half-width hw=0.15 clip = 0.15*W/2 ~= 9.6 px. At +20px from cx and at the
  // high row (above the body), we are outside both the body and the 1px wick.
  std::uint8_t wThin[4];
  px(cxPix[0] + 20, highY, wThin);
  std::printf("  wick0 thin-check x=%u y=%u  R=%u G=%u B=%u A=%u\n", cxPix[0] + 20, highY, wThin[0], wThin[1], wThin[2], wThin[3]);
  check(wThin[0] < 40 && wThin[1] < 40 && wThin[2] < 40, "wick is thin (clear 20px off cx at high)");

  // --- Gaps between candles read back CLEAR. --------------------------------
  // Midpoint between candle 0 (x=32) and candle 1 (x=64) is x=48; at the body row
  // there is no geometry there (bodies are hw~9.6px wide, wicks are 1px at cx).
  std::uint8_t gap01[4], gap12[4];
  px(48, bodyY, gap01);
  px(80, bodyY, gap12);
  std::printf("  gap 0-1 x=48 y=%u  R=%u G=%u B=%u A=%u\n", bodyY, gap01[0], gap01[1], gap01[2], gap01[3]);
  std::printf("  gap 1-2 x=80 y=%u  R=%u G=%u B=%u A=%u\n", bodyY, gap12[0], gap12[1], gap12[2], gap12[3]);
  check(gap01[0] < 40 && gap01[1] < 40 && gap01[2] < 40, "gap between candle 0 and 1 is clear");
  check(gap12[0] < 40 && gap12[1] < 40 && gap12[2] < 40, "gap between candle 1 and 2 is clear");

  std::printf("=== Dawn instancedCandle: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
