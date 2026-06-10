// ENC-510 (P5.0b) — GL <-> Dawn parity harness.
//
// GOAL
// ----
// Prove the Dawn backend (DawnSceneRenderer, ENC-509) matches the GL backend
// (Renderer + OsMesaContext) comprehensively before cutover, by rendering the
// SAME Scene through both backends and comparing the RGBA readbacks pixel by
// pixel. This header is the reusable comparison core; each parity_*.cpp test
// supplies a scene-builder closure + viewport and asserts parity within a
// tolerance.
//
// HOW A SCENE IS DESCRIBED ONCE, RENDERED TWICE
// ---------------------------------------------
// A parity test builds a Scene via CommandProcessor JSON commands EXACTLY ONCE
// (the same command list both backends see) and supplies the per-buffer CPU
// bytes through a small BufferData list. The harness then:
//   (a) GL path:   OsMesaContext.init(W,H) -> populate a GpuBufferManager from
//                  the BufferData list -> Renderer.render(scene,bufs,W,H) ->
//                  ctx.readPixels() (bottom-left origin RGBA).
//   (b) Dawn path: DawnSceneRenderer(atlas,textures).init() -> populate a
//                  CpuBufferStore from the SAME BufferData list ->
//                  renderer.render(scene,store,W,H) -> device.readPixel() per
//                  pixel (top-left origin).
//   (c) Compare the two RGBA buffers with a tolerance model (below) and report
//       max channel delta + mismatch %.
//
// ORIGIN / Y-FLIP CONVENTION  (the load-bearing subtlety)
// -------------------------------------------------------
// The GL readback (OsMesaContext::readPixels) is BOTTOM-LEFT origin: row 0 is
// the bottom of the frame. The Dawn readback (DawnDevice::readPixel) is
// TOP-LEFT origin, and every Dawn backend negates clip-space y so the SAME
// scene lands in the SAME on-screen position as GL (see d2_1_dawn_first_render).
// Net effect: a scene element that GL writes at readback row `y` (from bottom)
// is written by Dawn at readback row `H-1-y` (from top). So the harness compares
//      GL[x][y]   against   Dawn.readPixel(x, H-1-y).
// This row flip is determined EMPIRICALLY once per process (renderProbeFlip())
// by rendering a deliberately top/bottom-asymmetric scene through both backends
// and checking which row mapping makes them agree — so the harness can't be
// silently wrong about the convention, and reports which mapping it used.
//
// TOLERANCE MODEL
// ---------------
// Rasterizers (OSMesa/llvmpipe GL vs Dawn-on-Vulkan/lavapipe) differ at edges
// and in fractional coverage, so byte-exact full-frame equality is unrealistic
// for AA/text. The model has two knobs:
//   * channelTol   — a pixel is a MISMATCH only if any channel differs by more
//                    than this (|Δ| > channelTol). Solid-fill interiors use
//                    channelTol == 0 (exact); AA/text use a small N (e.g. 8..40).
//   * maxMismatchPct — the test passes if the fraction of mismatching pixels is
//                    <= this. Solid scenes use 0; AA-edge/text scenes allow a
//                    few percent of fringe pixels to differ.
// The harness ALWAYS reports the realized maxChannelDelta and mismatchPct so a
// divergence is visible even when the test still passes within tolerance. A
// scenario that genuinely can't reach parity is documented in its test (WHY)
// rather than hidden by inflating the tolerance.
//
// SKIP-GRACEFULLY
// ---------------
// If OSMesa can't init (no GL) or Dawn can't find an adapter, the harness
// returns a SKIPPED result and the test exits 0 — matching every other
// GL/Dawn test's graceful-skip behavior on a box without the backend.
#pragma once

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/debug/Stats.hpp"

#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/text/GlyphAtlas.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace dc {
namespace parity {

// One logical buffer's CPU bytes (a vertex/index/instance buffer the scene's
// geometry references by id). The SAME list is fed to both backends.
struct BufferData {
  Id id{0};
  std::vector<std::uint8_t> bytes;

  BufferData() = default;
  BufferData(Id i, const void* data, std::size_t n)
      : id(i),
        bytes(reinterpret_cast<const std::uint8_t*>(data),
              reinterpret_cast<const std::uint8_t*>(data) + n) {}
};

// A scenario: build the Scene (identical commands for both backends) and return
// the per-buffer CPU bytes. The builder gets a fresh CommandProcessor each call
// (the GL and Dawn passes each build their own Scene from the same closure so
// neither backend can perturb the other's scene state).
using SceneBuilder =
    std::function<std::vector<BufferData>(CommandProcessor& cp, Scene& scene)>;

struct Tolerance {
  int channelTol{0};         // per-channel |Δ| allowed before a pixel counts as mismatch
  double maxMismatchPct{0.0};// max fraction (%) of mismatching pixels to still PASS
  bool compareAlpha{false};  // also diff the A channel (RGB-only by default; the
                             // offscreen targets are opaque so A is uniformly 255)
};

// ENC-511 (P5.0c) — optional D78 pane border/separator style applied to BOTH
// backends before rendering (GL Renderer::setRenderStyle and
// DawnSceneRenderer::setRenderStyle). Defaults (zero widths) mean "no style",
// so existing scenarios that don't pass one are byte-identical to ENC-510.
struct ParityStyle {
  bool enabled{false};
  float paneBorderColor[4] = {0, 0, 0, 0};
  float paneBorderWidth{0.0f};
  float separatorColor[4] = {0, 0, 0, 0};
  float separatorWidth{0.0f};
};

struct ParityResult {
  bool skipped{false};
  std::string skipReason;
  bool passed{false};

  int width{0};
  int height{0};
  int maxChannelDelta{0};     // worst single-channel |Δ| over the whole frame
  long mismatchPixels{0};     // pixels exceeding channelTol
  long comparedPixels{0};
  double mismatchPct{0.0};

  std::string glBackend{"OSMesa"};
  std::string dawnBackend;    // e.g. "Vulkan" / adapter name
  bool rowFlipped{true};      // mapping used: Dawn row = H-1-glRow (true) or = glRow
};

// ---------------------------------------------------------------------------
// Internal helpers.
// ---------------------------------------------------------------------------

// Build a Scene from `builder` and render it via GL into `outRgba` (bottom-left
// origin, row-major RGBA, size W*H*4). Returns false if OSMesa is unavailable.
inline bool renderGl(const SceneBuilder& builder, int W, int H,
                     GlyphAtlas* atlas, std::vector<std::uint8_t>& outRgba,
                     const ParityStyle& style = {}) {
  OsMesaContext ctx;
  if (!ctx.init(W, H)) return false;

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  if (atlas) cp.setGlyphAtlas(atlas);

  std::vector<BufferData> buffers = builder(cp, scene);

  GpuBufferManager bufs;
  for (const auto& b : buffers) {
    bufs.setCpuData(b.id, b.bytes.data(),
                    static_cast<std::uint32_t>(b.bytes.size()));
  }

  Renderer renderer;
  if (!renderer.init()) return false;
  if (atlas) renderer.setGlyphAtlas(atlas);
  if (style.enabled) {
    RenderStyle rs;
    rs.paneBorderColor[0] = style.paneBorderColor[0];
    rs.paneBorderColor[1] = style.paneBorderColor[1];
    rs.paneBorderColor[2] = style.paneBorderColor[2];
    rs.paneBorderColor[3] = style.paneBorderColor[3];
    rs.paneBorderWidth = style.paneBorderWidth;
    rs.separatorColor[0] = style.separatorColor[0];
    rs.separatorColor[1] = style.separatorColor[1];
    rs.separatorColor[2] = style.separatorColor[2];
    rs.separatorColor[3] = style.separatorColor[3];
    rs.separatorWidth = style.separatorWidth;
    renderer.setRenderStyle(rs);
  }
  bufs.uploadDirty();
  renderer.render(scene, bufs, W, H);
  ctx.swapBuffers();

  outRgba = ctx.readPixels();
  return outRgba.size() == static_cast<std::size_t>(W) * H * 4;
}

// Build a Scene from `builder` and render it via Dawn, reading it back into
// `outRgba` (TOP-LEFT origin, row-major RGBA, size W*H*4 — readPixel per pixel).
// Returns false (with `reason`) if Dawn can't bring up an adapter.
inline bool renderDawn(const SceneBuilder& builder, int W, int H,
                       GlyphAtlas* atlas, std::vector<std::uint8_t>& outRgba,
                       std::string& backendName, std::string& reason,
                       const ParityStyle& style = {}) {
  DawnSceneRenderer renderer(atlas, nullptr);
  if (!renderer.init()) {
    reason = renderer.errorMessage();
    return false;
  }
  backendName = renderer.device().backendName();
  if (style.enabled) {
    DawnRenderStyle rs;
    rs.paneBorderColor[0] = style.paneBorderColor[0];
    rs.paneBorderColor[1] = style.paneBorderColor[1];
    rs.paneBorderColor[2] = style.paneBorderColor[2];
    rs.paneBorderColor[3] = style.paneBorderColor[3];
    rs.paneBorderWidth = style.paneBorderWidth;
    rs.separatorColor[0] = style.separatorColor[0];
    rs.separatorColor[1] = style.separatorColor[1];
    rs.separatorColor[2] = style.separatorColor[2];
    rs.separatorColor[3] = style.separatorColor[3];
    rs.separatorWidth = style.separatorWidth;
    renderer.setRenderStyle(rs);
  }

  Scene scene;
  ResourceRegistry reg;
  CommandProcessor cp(scene, reg);
  if (atlas) cp.setGlyphAtlas(atlas);

  std::vector<BufferData> buffers = builder(cp, scene);

  CpuBufferStore store;
  for (const auto& b : buffers) {
    store.setCpuData(b.id, b.bytes.data(),
                     static_cast<std::uint32_t>(b.bytes.size()));
  }

  renderer.render(scene, store, W, H);

  outRgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      std::uint8_t px[4] = {0, 0, 0, 0};
      renderer.device().readPixel(x, y, px);
      std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
      outRgba[idx + 0] = px[0];
      outRgba[idx + 1] = px[1];
      outRgba[idx + 2] = px[2];
      outRgba[idx + 3] = px[3];
    }
  }
  return true;
}

// Count mismatching pixels between a GL frame (bottom-left) and a Dawn frame
// (top-left) under a row mapping (rowFlipped: dawnRow = H-1-glRow). Returns the
// mismatch count and the worst channel delta via out-params.
inline long diffFrames(const std::vector<std::uint8_t>& gl,
                       const std::vector<std::uint8_t>& dawn, int W, int H,
                       const Tolerance& tol, bool rowFlipped, int& maxDelta) {
  long mismatch = 0;
  maxDelta = 0;
  const int channels = tol.compareAlpha ? 4 : 3;
  for (int y = 0; y < H; ++y) {
    const int dy = rowFlipped ? (H - 1 - y) : y;
    for (int x = 0; x < W; ++x) {
      const std::size_t gi = (static_cast<std::size_t>(y) * W + x) * 4;
      const std::size_t di = (static_cast<std::size_t>(dy) * W + x) * 4;
      int worst = 0;
      for (int c = 0; c < channels; ++c) {
        int d = std::abs(static_cast<int>(gl[gi + c]) -
                         static_cast<int>(dawn[di + c]));
        if (d > worst) worst = d;
      }
      if (worst > maxDelta) maxDelta = worst;
      if (worst > tol.channelTol) ++mismatch;
    }
  }
  return mismatch;
}

// Decide the row mapping empirically: a tiny top-biased scene (a solid quad in
// the TOP half of the frame in screen space) is rendered through both backends;
// whichever row mapping (flipped vs direct) yields fewer mismatches is the
// convention. This guards against silently comparing mirror images.
inline bool detectRowFlip(GlyphAtlas* /*atlas*/, bool& haveResult) {
  haveResult = false;
  // A red rect occupying clip y in [0.3, 0.9] (TOP of the frame in clip space).
  // GL: top of clip -> top rows (bottom-left origin => high y indices).
  // Dawn: backends negate clip y => lands at bottom of WebGPU framebuffer; with
  // top-left readback that's high y indices too — but the harness must MEASURE,
  // not assume. We render and pick the better mapping.
  const int W = 32, H = 32;
  SceneBuilder probe = [](CommandProcessor& cp, Scene&) {
    cp.applyJsonText(R"({"cmd":"createPane","id":1})");
    cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})");
    cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})");
    float rect[] = {-0.9f, 0.3f, 0.9f, 0.9f};  // rect4: x0 y0 x1 y1 (top band)
    cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})");
    cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":1,"format":"rect4"})");
    cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRect@1","geometryId":100})");
    cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})");
    return std::vector<BufferData>{BufferData(10, rect, sizeof(rect))};
  };

  std::vector<std::uint8_t> glFb, dawnFb;
  if (!renderGl(probe, W, H, nullptr, glFb)) return true;  // no GL: assume default
  std::string be, reason;
  if (!renderDawn(probe, W, H, nullptr, dawnFb, be, reason)) return true;

  Tolerance tol; tol.channelTol = 60;
  int md;
  long flipped = diffFrames(glFb, dawnFb, W, H, tol, /*rowFlipped=*/true, md);
  long direct = diffFrames(glFb, dawnFb, W, H, tol, /*rowFlipped=*/false, md);
  haveResult = true;
  return flipped <= direct;  // true => flipped mapping is the right one
}

// ---------------------------------------------------------------------------
// Public entry: render `builder` through both backends and compare.
// ---------------------------------------------------------------------------
inline ParityResult compareScene(const char* name, const SceneBuilder& builder,
                                  int W, int H, const Tolerance& tol,
                                  GlyphAtlas* atlas = nullptr,
                                  const ParityStyle& style = {}) {
  ParityResult r;
  r.width = W;
  r.height = H;

  std::vector<std::uint8_t> glFb;
  if (!renderGl(builder, W, H, atlas, glFb, style)) {
    r.skipped = true;
    r.skipReason = "OSMesa/GL unavailable";
    std::printf("[parity %s] SKIP: %s\n", name, r.skipReason.c_str());
    return r;
  }

  std::vector<std::uint8_t> dawnFb;
  std::string reason;
  if (!renderDawn(builder, W, H, atlas, dawnFb, r.dawnBackend, reason, style)) {
    r.skipped = true;
    r.skipReason = "Dawn adapter unavailable: " + reason;
    std::printf("[parity %s] SKIP: %s\n", name, r.skipReason.c_str());
    return r;
  }

  // Determine the row mapping once (cached per process).
  static bool s_haveFlip = false;
  static bool s_rowFlip = true;
  if (!s_haveFlip) {
    bool got = false;
    bool f = detectRowFlip(atlas, got);
    if (got) { s_rowFlip = f; s_haveFlip = true; }
  }
  r.rowFlipped = s_rowFlip;

  int maxDelta = 0;
  long mismatch = diffFrames(glFb, dawnFb, W, H, tol, r.rowFlipped, maxDelta);

  r.comparedPixels = static_cast<long>(W) * H;
  r.mismatchPixels = mismatch;
  r.maxChannelDelta = maxDelta;
  r.mismatchPct = r.comparedPixels > 0
                      ? (100.0 * static_cast<double>(mismatch) / r.comparedPixels)
                      : 0.0;
  r.passed = (r.mismatchPct <= tol.maxMismatchPct);

  std::printf(
      "[parity %s] %s  W=%d H=%d  maxDelta=%d  mismatch=%ld/%ld (%.3f%%)  "
      "tol(chan=%d, pct<=%.3f)  rowFlip=%d  dawn=%s\n",
      name, r.passed ? "PASS" : "FAIL", W, H, r.maxChannelDelta, r.mismatchPixels,
      r.comparedPixels, r.mismatchPct, tol.channelTol, tol.maxMismatchPct,
      r.rowFlipped ? 1 : 0, r.dawnBackend.c_str());
  return r;
}

}  // namespace parity
}  // namespace dc
