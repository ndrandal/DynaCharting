// ENC-500 (P5.1) — JsonHost-via-Dawn conformance + streaming perf sweep.
//
// Proves the cutover: drives the EXACT JsonHost embedding pipeline
// (parseSceneDocument -> SceneReconciler -> IngestProcessor -> buffer store ->
// render -> readback) end-to-end on BOTH backends and asserts the Dawn readback
// matches the GL readback within the parity tolerance (reusing the parity diff /
// row-flip convention from parity_harness.hpp). This is the same render path
// core/src/host/JsonHost.cpp takes — the only difference between the two passes
// here is the backend (GL Renderer + GpuBufferManager + OSMesa readback vs
// DawnSceneRenderer + CpuBufferStore + DawnDevice readback), exactly as the host
// selects at build time via DC_HAS_DAWN.
//
// Then a STREAMING PERF SWEEP: times N writeRange()+uploadDirty() cycles on the
// GL GpuBufferManager and on the Dawn CpuBufferStore+DeviceBufferResolver hot
// path, and reports both numbers so the Dawn streaming-ingest path can be sanity-
// checked against GL (the d2_4 / d81_3 tests already cover correctness).
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
//
// Graceful skip: if OSMesa or Dawn is unavailable the test prints SKIP and
// exits 0, matching every other GL/Dawn test on a box without the backend.

#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/render/CpuBufferStore.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int g_failed = 0;
static int g_passed = 0;

static void check(bool cond, const char* name) {
  if (cond) { std::printf("  PASS: %s\n", name); ++g_passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++g_failed; }
}

// A representative multi-pipeline JsonHost scene document: a filled triangle
// (triSolid@1), a line (line2d@1), and an instanced rect (instancedRect@1) on a
// single pane — exactly the SceneDocument shape JsonHost parses + reconciles.
static const char* kSceneJson = R"({
  "version": 1,
  "viewport": { "width": 96, "height": 64 },
  "buffers": {
    "100": { "data": [-0.9, -0.7, -0.3, -0.7, -0.6, 0.7] },
    "101": { "data": [-0.2, -0.6, 0.2, 0.6] },
    "102": { "data": [0.35, 0.0, 0.9, 0.0] }
  },
  "transforms": {
    "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 }
  },
  "panes": { "1": { "name": "main" } },
  "layers": { "10": { "paneId": 1, "name": "data" } },
  "geometries": {
    "200": { "vertexBufferId": 100, "format": "pos2_clip", "vertexCount": 3 },
    "201": { "vertexBufferId": 101, "format": "rect4", "vertexCount": 1 },
    "202": { "vertexBufferId": 102, "format": "pos2_clip", "vertexCount": 2 }
  },
  "drawItems": {
    "300": { "layerId": 10, "pipeline": "triSolid@1",
             "geometryId": 200, "transformId": 50, "color": [1, 0, 0, 1] },
    "301": { "layerId": 10, "pipeline": "instancedRect@1",
             "geometryId": 201, "transformId": 50, "color": [0, 1, 0, 1] },
    "302": { "layerId": 10, "pipeline": "line2d@1",
             "geometryId": 202, "transformId": 50, "color": [0, 0, 1, 1] }
  }
})";

// ---- JsonHost render flow, parameterized by backend store population. --------
// Build the same scene the host builds (parse + reconcile + ingest) and return
// the document so each backend can populate its store from the ingested bytes.
static bool buildHostScene(dc::SceneDocument& doc, dc::Scene& scene,
                           dc::ResourceRegistry& reg, dc::CommandProcessor& cp,
                           dc::IngestProcessor& ingest) {
  if (!dc::parseSceneDocument(kSceneJson, doc)) return false;
  cp.setIngestProcessor(&ingest);
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  if (!result.ok) return false;
  // Upload inline buffer data into the ingest processor (host step 6).
  for (const auto& [id, buf] : doc.buffers) {
    if (!buf.data.empty()) {
      ingest.ensureBuffer(id);
      ingest.setBufferData(id,
        reinterpret_cast<const std::uint8_t*>(buf.data.data()),
        static_cast<std::uint32_t>(buf.data.size() * sizeof(float)));
    }
  }
  return true;
}

template <typename Store>
static void syncStore(const dc::SceneDocument& doc, dc::IngestProcessor& ingest,
                      Store& store) {
  for (const auto& [id, buf] : doc.buffers) {
    const auto* data = ingest.getBufferData(id);
    auto size = ingest.getBufferSize(id);
    if (data && size > 0) store.setCpuData(id, data, size);
  }
}

int main() {
  std::printf("=== ENC-500 JsonHost-via-Dawn conformance + perf ===\n");
  constexpr int W = 96, H = 64;

  // --- GL JsonHost baseline (bottom-up RGBA readback). ---------------------
  std::vector<std::uint8_t> glFb;
  {
    dc::OsMesaContext ctx;
    if (!ctx.init(W, H)) {
      std::printf("SKIP: OSMesa/GL unavailable\n");
      return 0;
    }
    dc::SceneDocument doc; dc::Scene scene; dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg); dc::IngestProcessor ingest;
    if (!buildHostScene(doc, scene, reg, cp, ingest)) {
      std::fprintf(stderr, "GL: buildHostScene failed\n"); return 1;
    }
    dc::GpuBufferManager gpuBufs;
    syncStore(doc, ingest, gpuBufs);
    gpuBufs.uploadDirty();
    dc::Renderer renderer;
    if (!renderer.init()) { std::printf("SKIP: GL Renderer init failed\n"); return 0; }
    renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();
    glFb = ctx.readPixels();
    if (glFb.size() != static_cast<std::size_t>(W) * H * 4) {
      std::fprintf(stderr, "GL: readPixels size mismatch\n"); return 1;
    }
  }

  // --- Dawn JsonHost (the cutover path; top-down RGBA readback). -----------
  std::vector<std::uint8_t> dawnFb(static_cast<std::size_t>(W) * H * 4, 0);
  std::string dawnBackend;
  {
    dc::DawnSceneRenderer renderer;  // no atlas/textures: this scene uses neither
    if (!renderer.init()) {
      std::printf("SKIP: Dawn adapter unavailable: %s\n",
                  renderer.errorMessage().c_str());
      return 0;
    }
    dawnBackend = renderer.device().backendName();
    dc::SceneDocument doc; dc::Scene scene; dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg); dc::IngestProcessor ingest;
    if (!buildHostScene(doc, scene, reg, cp, ingest)) {
      std::fprintf(stderr, "Dawn: buildHostScene failed\n"); return 1;
    }
    dc::CpuBufferStore store;
    syncStore(doc, ingest, store);
    renderer.render(scene, store, W, H);
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        std::uint8_t px[4] = {0, 0, 0, 0};
        renderer.device().readPixel(x, y, px);
        std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
        dawnFb[idx + 0] = px[0]; dawnFb[idx + 1] = px[1];
        dawnFb[idx + 2] = px[2]; dawnFb[idx + 3] = px[3];
      }
  }

  // --- Compare GL (bottom-up) vs Dawn (top-down) with row flip. ------------
  // Solid-fill scene: small per-channel tolerance, allow a tiny fraction of
  // edge pixels (the two rasterizers' triangle/line edges differ by a pixel).
  const int channelTol = 16;
  const double maxMismatchPct = 2.0;
  long mismatch = 0, compared = static_cast<long>(W) * H;
  int maxDelta = 0;
  for (int y = 0; y < H; ++y) {
    const int dy = y;  // DawnSceneRenderer readback matches GL orientation (no flip; see ENC-510 parity harness, which empirically detected direct/no-flip)
    for (int x = 0; x < W; ++x) {
      const std::size_t gi = (static_cast<std::size_t>(y) * W + x) * 4;
      const std::size_t di = (static_cast<std::size_t>(dy) * W + x) * 4;
      int worst = 0;
      for (int c = 0; c < 3; ++c) {
        int d = std::abs(static_cast<int>(glFb[gi + c]) -
                         static_cast<int>(dawnFb[di + c]));
        if (d > worst) worst = d;
      }
      if (worst > maxDelta) maxDelta = worst;
      if (worst > channelTol) ++mismatch;
    }
  }
  double mismatchPct = 100.0 * static_cast<double>(mismatch) / compared;
  std::printf(
      "[JsonHost conformance] dawn=%s  W=%d H=%d  maxDelta=%d  "
      "mismatch=%ld/%ld (%.3f%%)  tol(chan=%d, pct<=%.3f)\n",
      dawnBackend.c_str(), W, H, maxDelta, mismatch, compared, mismatchPct,
      channelTol, maxMismatchPct);
  check(mismatchPct <= maxMismatchPct,
        "JsonHost Dawn readback matches GL baseline within tolerance");

  // --- Streaming perf sweep: N writeRange()+uploadDirty() cycles. ----------
  // The live-ingest hot path appends to a buffer tail each tick and re-uploads
  // the dirty range. We time that on GL (GpuBufferManager VBO upload) and on
  // Dawn (CpuBufferStore + DeviceBufferResolver -> queue.writeBuffer).
  {
    const int kCycles = 2000;
    const std::uint32_t kChunk = 256;  // bytes appended per cycle
    std::vector<std::uint8_t> payload(kChunk);
    for (std::uint32_t i = 0; i < kChunk; ++i)
      payload[i] = static_cast<std::uint8_t>(i & 0xFF);

    // GL path.
    double glMs = -1.0;
    {
      dc::OsMesaContext ctx;
      if (ctx.init(8, 8)) {
        dc::GpuBufferManager bufs;
        bufs.reserve(7, kChunk * kCycles);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kCycles; ++i) {
          bufs.writeRange(7, static_cast<std::uint32_t>(i) * kChunk,
                          payload.data(), kChunk);
          bufs.uploadDirty();
        }
        auto t1 = std::chrono::steady_clock::now();
        glMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
      }
    }

    // Dawn path.
    double dawnMs = -1.0;
    {
      dc::DawnDevice device;
      if (device.init()) {
        dc::CpuBufferStore store;
        dc::DeviceBufferResolver resolver(device);
        store.reserve(7, kChunk * kCycles);
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < kCycles; ++i) {
          store.writeRange(7, static_cast<std::uint32_t>(i) * kChunk,
                           payload.data(), kChunk);
          store.uploadDirty(device, resolver);
        }
        auto t1 = std::chrono::steady_clock::now();
        dawnMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
      }
    }

    std::printf(
        "[streaming perf] %d cycles x %u bytes (writeRange+uploadDirty):\n"
        "    GL   GpuBufferManager : %s\n"
        "    Dawn CpuBufferStore   : %s\n",
        kCycles, kChunk,
        glMs >= 0 ? (std::to_string(glMs) + " ms").c_str() : "(GL unavailable)",
        dawnMs >= 0 ? (std::to_string(dawnMs) + " ms").c_str()
                    : "(Dawn unavailable)");
    if (glMs >= 0 && dawnMs >= 0) {
      double ratio = dawnMs / glMs;
      std::printf("    Dawn/GL ratio        : %.2fx  (per-cycle GL=%.4f ms, "
                  "Dawn=%.4f ms)\n",
                  ratio, glMs / 2000.0, dawnMs / 2000.0);
      // Perf SANITY only (not a hard gate): flag if Dawn is wildly slower.
      check(ratio < 10.0,
            "Dawn streaming hot path within 10x of GL (perf sanity)");
    }
  }

  std::printf("=== JsonHost-Dawn: %d passed, %d failed (dawn=%s) ===\n",
              g_passed, g_failed, dawnBackend.c_str());
  return g_failed > 0 ? 1 : 0;
}
