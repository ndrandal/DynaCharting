// ENC-1257 — THE RASTER PROOF: candles really do keep a clear pixel column.
//
// `dc_enc1257_bar_sizing` proves the arithmetic of the rule in the default
// build. This proves the pixels: it renders candle rows through the real
// `instancedCandle@1` Dawn backend, reads the framebuffer back, and counts the
// lit/clear runs along the body row. The chart is legible iff the number of lit
// runs equals the number of bars — a fused slab is exactly the case where two
// bars share one run.
//
// Three scenes, each taken from SPEC `specs/2026-09-19-chart-quality-bar/SPEC.md`:
//
//   [A] 10 bars across the live capture's 1300px plot (§1.0).
//   [B] 500 bars across the same plot — D1 tier 2's other acceptance point, and
//       the density at which the FIXED-PIXEL WICK, not the body, is what eats
//       the gap.
//   [C] 156 bars at `apps/showcase/views/candles-aapl/`'s exact committed
//       transform and halfWidth on the 800px canvas snap-stills uses — i.e. the
//       fused-slab still in §1.1, rendered. Before ENC-1257 this scene produced
//       runs of fused bodies; the test asserts 156 separate runs.
//
// WHICH RASTER. ENC-1249 established that a tier-0 pixel assertion must name the
// raster it reads, because the raw Dawn readback is vertically mirrored against
// what the user sees (LIMITATIONS.md DC-L05). This test reads the RAW readback
// and says so — and it is sound here precisely because the property measured is
// horizontal: a row flip permutes which row you land on, never the left-to-right
// run structure within a row. Every candle below spans a symmetric y band, so
// the scanned row is inside the body either way. Nothing here would change on
// the presented raster.
//
// Dawn-gated, therefore EXCLUDED from the default ctest (LIMITATIONS.md DC-L01).
// Run it with:
//   cmake -B build-dawn -G Ninja -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
//   cmake --build build-dawn -j$(nproc)
//   ctest --test-dir build-dawn -R dc_enc1257_dawn_candle_gap --output-on-failure
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedCandleBackend.hpp"

#include "dc/commands/CommandProcessor.hpp"
#include "dc/render/BackendRegistry.hpp"
#include "dc/render/BarSizing.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool cond, const std::string& name) {
  if (cond) { std::printf("  PASS: %s\n", name.c_str()); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name.c_str()); ++failed; }
}
static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: %s %s\n", ctx, r.err.code.c_str(),
                 r.err.message.c_str());
    std::exit(1);
  }
}

// One scan of a framebuffer row: the lengths of the lit runs and of the clear
// runs strictly BETWEEN them (leading/trailing clear margins are not gaps).
struct RowRuns {
  std::vector<int> lit;
  std::vector<int> gaps;
};

static RowRuns scanRow(const std::vector<std::uint8_t>& rgba, int W, int row) {
  RowRuns r;
  int litRun = 0, clearRun = 0;
  bool seenLit = false;
  for (int x = 0; x < W; ++x) {
    const std::uint8_t* p = rgba.data() + (static_cast<std::size_t>(row) * W + x) * 4;
    const bool lit = p[0] > 24 || p[1] > 24 || p[2] > 24;
    if (lit) {
      if (clearRun > 0 && seenLit) r.gaps.push_back(clearRun);
      clearRun = 0;
      ++litRun;
    } else {
      if (litRun > 0) { r.lit.push_back(litRun); seenLit = true; }
      litRun = 0;
      ++clearRun;
    }
  }
  if (litRun > 0) r.lit.push_back(litRun);
  return r;
}

// Render `count` all-UP candles at data x = x0, x0+1, … through the real
// backend with the given transform, and scan the body row.
// `idBase` must be UNIQUE per call. The backend caches its GPU instance buffer
// per geometryId and invalidates on the CpuBufferStore's data VERSION — and a
// fresh store restarts that counter at 1, so a second scene reusing geometry 100
// is a cache HIT and silently re-renders the first scene's candles under the
// second scene's transform. That is not a product defect (one process has one
// store) but it is an excellent way to write a test that measures nothing, and
// it is exactly what the first run of this file did.
static RowRuns renderRow(dc::DawnDevice& dev, dc::BackendRegistry& backends,
                         int count, float x0, float halfWidth, float sx,
                         float tx, std::uint32_t W, std::uint32_t H,
                         unsigned idBase) {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  // candle6: (cx, open, high, low, close, hw). All UP so every mark is green and
  // "lit" is unambiguous. Body -0.5..0.5, wick -0.8..0.8 in DATA y; the
  // transform below is x-only, so data y is clip y.
  std::vector<float> recs(static_cast<std::size_t>(count) * 6);
  for (int i = 0; i < count; ++i) {
    float* r = recs.data() + static_cast<std::size_t>(i) * 6;
    r[0] = x0 + static_cast<float>(i);
    r[1] = -0.5f;  // open
    r[2] = 0.8f;   // high
    r[3] = -0.8f;  // low
    r[4] = 0.5f;   // close (>= open -> UP)
    r[5] = halfWidth;
  }
  const unsigned bufId = idBase + 10;
  const unsigned geoId = idBase + 100;
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                R"({"cmd":"createBuffer","id":%u,"byteLength":%zu})", bufId,
                recs.size() * sizeof(float));
  requireOk(cp.applyJsonText(buf), "buf");
  store.setCpuData(bufId, recs.data(), recs.size() * sizeof(float));
  std::snprintf(buf, sizeof(buf),
                R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,)"
                R"("vertexCount":%d,"format":"candle6"})", geoId, bufId, count);
  requireOk(cp.applyJsonText(buf), "geom");
  std::snprintf(buf, sizeof(buf),
                R"({"cmd":"bindDrawItem","drawItemId":3,)"
                R"("pipeline":"instancedCandle@1","geometryId":%u})", geoId);
  requireOk(cp.applyJsonText(buf), "bind");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,)"
      R"("colorUpR":0,"colorUpG":1,"colorUpB":0,"colorUpA":1,)"
      R"("colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})"), "style");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");
  std::snprintf(buf, sizeof(buf),
                R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":1,"tx":%.9g,"ty":0})",
                static_cast<double>(sx), static_cast<double>(tx));
  requireOk(cp.applyJsonText(buf), "setxform");
  requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":3,"transformId":50})"), "attach");

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = rp.clearColor[1] = rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  dev.beginRenderPass(rp);
  const dc::DrawItem* di = scene.getDrawItem(3);
  dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
  be->renderDrawItem(dev, scene, store, *di, static_cast<int>(W),
                     static_cast<int>(H));
  dev.endRenderPass();

  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(W) * H * 4, 0);
  std::uint32_t gotW = 0, gotH = 0;
  if (!dev.readFramebufferRGBA(rgba.data(), rgba.size(), &gotW, &gotH) ||
      gotW != W || gotH != H) {
    std::fprintf(stderr, "readFramebufferRGBA failed\n");
    std::exit(1);
  }
  // Body row: data y = 0 is inside the body (-0.5..0.5) for every candle.
  return scanRow(rgba, static_cast<int>(W), static_cast<int>(H / 2));
}

// The SAME candles at their AUTHORED width, rendered so the rule cannot engage:
// one instance per DrawItem. With a single instance there is no inter-bar
// relation, `barPitchFromRecords` returns 0, `resolveCandleBodyClip` reports
// `apply == false`, and the shader falls back to the per-instance halfWidth —
// i.e. exactly the pre-ENC-1257 geometry, drawn by the post-ENC-1257 binary.
// This is how the "before" raster in the claims below is a MEASUREMENT and not
// an extrapolation.
static RowRuns renderRowLegacy(dc::DawnDevice& dev, dc::BackendRegistry& backends,
                               int count, float x0, float halfWidth, float sx,
                               float tx, std::uint32_t W, std::uint32_t H,
                               unsigned idBase) {
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "xform");
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                R"({"cmd":"setTransform","id":50,"sx":%.9g,"sy":1,"tx":%.9g,"ty":0})",
                static_cast<double>(sx), static_cast<double>(tx));
  requireOk(cp.applyJsonText(buf), "setxform");

  for (int i = 0; i < count; ++i) {
    const unsigned di = idBase + 1u + static_cast<unsigned>(i) * 3u;
    const unsigned bufId = di + 1u;
    const unsigned geoId = di + 2u;
    const float rec[6] = {x0 + static_cast<float>(i), -0.5f, 0.8f,
                          -0.8f, 0.5f, halfWidth};
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"createBuffer","id":%u,"byteLength":24})", bufId);
    requireOk(cp.applyJsonText(buf), "lbuf");
    store.setCpuData(bufId, rec, sizeof(rec));
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"createGeometry","id":%u,"vertexBufferId":%u,)"
                  R"("vertexCount":1,"format":"candle6"})", geoId, bufId);
    requireOk(cp.applyJsonText(buf), "lgeom");
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"createDrawItem","id":%u,"layerId":2})", di);
    requireOk(cp.applyJsonText(buf), "ldi");
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"bindDrawItem","drawItemId":%u,)"
                  R"("pipeline":"instancedCandle@1","geometryId":%u})", di, geoId);
    requireOk(cp.applyJsonText(buf), "lbind");
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"setDrawItemStyle","drawItemId":%u,)"
                  R"("colorUpR":0,"colorUpG":1,"colorUpB":0,"colorUpA":1,)"
                  R"("colorDownR":1,"colorDownG":0,"colorDownB":0,"colorDownA":1})", di);
    requireOk(cp.applyJsonText(buf), "lstyle");
    std::snprintf(buf, sizeof(buf),
                  R"({"cmd":"attachTransform","drawItemId":%u,"transformId":50})", di);
    requireOk(cp.applyJsonText(buf), "lattach");
  }

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = rp.clearColor[1] = rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  dev.beginRenderPass(rp);
  for (int i = 0; i < count; ++i) {
    const dc::DrawItem* di =
        scene.getDrawItem(idBase + 1u + static_cast<unsigned>(i) * 3u);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
    be->renderDrawItem(dev, scene, store, *di, static_cast<int>(W),
                       static_cast<int>(H));
  }
  dev.endRenderPass();

  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(W) * H * 4, 0);
  std::uint32_t gotW = 0, gotH = 0;
  if (!dev.readFramebufferRGBA(rgba.data(), rgba.size(), &gotW, &gotH) ||
      gotW != W || gotH != H) {
    std::fprintf(stderr, "readFramebufferRGBA failed\n");
    std::exit(1);
  }
  return scanRow(rgba, static_cast<int>(W), static_cast<int>(H / 2));
}

static int minOf(const std::vector<int>& v) {
  int m = 1 << 30;
  for (int x : v) if (x < m) m = x;
  return v.empty() ? 0 : m;
}

static int maxOf(const std::vector<int>& v) {
  int m = 0;
  for (int x : v) if (x > m) m = x;
  return m;
}

int main() {
  std::printf("=== ENC-1257 Dawn raster: inter-bar gap ===\n");

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr, "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                         "lvp_icd.x86_64.json forces lavapipe.\n");
    return 1;
  }
  // SPEC D8 — a render observation that does not name its adapter is not
  // admissible.
  std::printf("adapter: backend=%s name=\"%s\"\n", dev.backendName().c_str(),
              dev.adapterName().c_str());

  dc::DawnInstancedCandleBackend candle;
  if (!candle.init(dev)) {
    std::fprintf(stderr, "DawnInstancedCandleBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &candle);

  constexpr std::uint32_t H = 160;

  // --- [A] and [B]: N bars filling the live capture's 1300px plot ------------
  for (const int N : {10, 500}) {
    constexpr std::uint32_t W = 1300;
    // cx = 0..N-1 at a 1-unit pitch; sx = 2/N puts one pitch on W/N px, and tx
    // centres the first bar half a pitch in from the left edge.
    const float sx = 2.0f / static_cast<float>(N);
    const float tx = -1.0f + 1.0f / static_cast<float>(N);
    const RowRuns r = renderRow(dev, backends, N, 0.0f, 0.4f, sx, tx, W, H,
                                1000u * static_cast<unsigned>(N));
    const dc::BarMetrics m = dc::barMetricsForCount(N, static_cast<float>(W));
    std::printf("  [%d bars / %upx] pitch %.3fpx -> body %.3f gap %.3f | "
                "raster: %zu lit runs, min lit %d px, min gap %d px\n",
                N, W, m.pitchPx, m.bodyPx, m.gapPx, r.lit.size(),
                minOf(r.lit), minOf(r.gaps));
    check(static_cast<int>(r.lit.size()) == N,
          std::to_string(N) + " bars: every bar is a SEPARATE lit run (no fusion)");
    check(minOf(r.gaps) >= 1,
          std::to_string(N) + " bars: min inter-bar gap >= 1px in the raster");
    check(minOf(r.lit) >= 1,
          std::to_string(N) + " bars: every bar still has at least 1px of ink");
  }

  // --- [C] the fused-slab still, rendered ------------------------------------
  // apps/showcase/views/candles-aapl: view.json transform.sx = 0.011333333,
  // instruction.json halfWidth const = 0.4, x = record index starting at 4, and
  // the replay's xAnchor puts x=4 at clip -0.85 (tx = -0.8953333). snap-stills
  // runs an 800px canvas. ~156 bars are inside the frame.
  {
    constexpr std::uint32_t W = 800;
    const RowRuns r = renderRow(dev, backends, 156, 4.0f, 0.4f, 0.011333333f,
                                -0.8953333f, W, H, 900000u);
    std::printf("  [candles-aapl, 156 bars / %upx] raster: %zu lit runs, "
                "min lit %d px, min gap %d px\n",
                W, r.lit.size(), minOf(r.lit), minOf(r.gaps));
    check(static_cast<int>(r.lit.size()) == 156,
          "candles-aapl: 156 bars are 156 separate runs (the slab is gone)");
    check(minOf(r.gaps) >= 1,
          "candles-aapl: min inter-bar gap >= 1px in the raster");

    // …and the SAME scene at its authored 0.4 halfWidth, with the rule unable to
    // engage. This is the shipped still's geometry, measured rather than
    // inferred: 156 bodies collapse into a handful of slabs.
    const RowRuns before = renderRowLegacy(dev, backends, 156, 4.0f, 0.4f,
                                           0.011333333f, -0.8953333f, W, H,
                                           910000u);
    std::printf("  [candles-aapl, AUTHORED width (pre-ENC-1257 geometry)] "
                "raster: %zu lit runs, longest slab %d px\n",
                before.lit.size(), maxOf(before.lit));
    check(static_cast<int>(before.lit.size()) < 156,
          "pre-fix: the authored width does NOT give 156 separate runs");
    check(maxOf(before.lit) > 3 * maxOf(r.lit),
          "pre-fix: bodies fuse into slabs far longer than one bar");
    check(static_cast<int>(r.lit.size()) >
              static_cast<int>(before.lit.size()) * 2,
          "ENC-1257 more than doubles the number of resolvable bars");
  }

  // --- [D] ENC-1249's tier-0 candle scene, at its exact numbers ---------------
  // core/tests/dc_enc1249_tier0_truthful.cpp `caseCandle`: two candles at
  // cx -0.45 / +0.45, halfWidth 0.20, identity transform, 256px wide. Its B2/B4
  // probes read the body at cx+10px and its B5 probe requires x=W/2 clear. The
  // bar-sizing rule clamps that 51.2px body to the 24px ceiling — a 12px
  // half-width — so the cx+10 probe still lands on body, with 2px to spare. This
  // case exists so that margin is measured on THIS branch rather than assumed,
  // since the two tickets are in flight at the same time.
  {
    constexpr std::uint32_t W = 256;
    // cx = 0 and 1 in data space; sx 0.9 and tx -0.45 put them at clip -0.45/+0.45.
    const RowRuns r = renderRow(dev, backends, 2, 0.0f, 0.20f, 0.9f, -0.45f, W, H,
                                950000u);
    const dc::CandleBodyResolution res =
        dc::resolveCandleBodyClip(0.9f, 0.20f, 0.9f, static_cast<int>(W));
    const int cxPx = static_cast<int>((-0.45f * 0.5f + 0.5f) * W);  // ~70
    std::printf("  [ENC-1249 scene] authored body %.1fpx -> drawn %.1fpx "
                "(half %.1fpx); raster: %zu lit runs, min gap %d px\n",
                2.0f * 0.20f * 0.9f * static_cast<float>(W) * 0.5f,
                res.metrics.bodyPx, res.metrics.bodyPx * 0.5f, r.lit.size(),
                minOf(r.gaps));
    check(res.metrics.bodyPx * 0.5f > 10.0f,
          "ENC-1249 compat: the drawn body half-width still exceeds its 10px probe");
    check(static_cast<int>(r.lit.size()) == 2,
          "ENC-1249 compat: two candles, two runs (B5's gap stays clear)");
    (void)cxPx;
  }

  std::printf("=== ENC-1257 Dawn raster: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
