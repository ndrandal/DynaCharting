// ENC-501 (P5 cutover) — Dawn-only JsonHost golden conformance + streaming sweep.
//
// Originally (ENC-500) this drove the JsonHost embedding pipeline (parseSceneDocument
// -> SceneReconciler -> IngestProcessor -> buffer store -> render -> readback) on
// BOTH backends and asserted the Dawn readback matched the GL JsonHost baseline
// within the parity tolerance. Dawn is now the proven default renderer and dc_gl is
// being deleted (ENC-501), so this drives the EXACT JsonHost embedding pipeline on
// the Dawn backend ONLY and asserts the readback against captured GOLDEN probe
// pixels (the same Dawn pixels that matched the GL baseline while dc_gl existed).
// This is the same render path core/src/host/JsonHost.cpp takes.
//
// Then a STREAMING PERF SWEEP on the Dawn live-ingest hot path (CpuBufferStore +
// DeviceBufferResolver -> queue.writeBuffer): times N writeRange()+uploadDirty()
// cycles and reports the per-cycle cost as a perf sanity number (the GL leg of the
// old sweep is dropped with dc_gl; the d2_4 / d81_3 tests cover ingest correctness).
//
// On a headless box set the lavapipe ICD if Dawn finds no Vulkan adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
//
// Graceful skip: if Dawn is unavailable the test prints SKIP and exits 0.

#include "dc/document/SceneDocument.hpp"
#include "dc/document/SceneReconciler.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/render/CpuBufferStore.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static int g_failed = 0;
static int g_passed = 0;
static bool g_capture = false;

static void check(bool cond, const char* name) {
  if (cond) { std::printf("  PASS: %s\n", name); ++g_passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++g_failed; }
}

// A representative multi-pipeline JsonHost scene document: a filled triangle
// (triSolid@1), an instanced rect (instancedRect@1), and a line (line2d@1) on a
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

// Build the same scene the host builds (parse + reconcile + ingest).
static bool buildHostScene(dc::SceneDocument& doc, dc::Scene& scene,
                           dc::ResourceRegistry& /*reg*/, dc::CommandProcessor& cp,
                           dc::IngestProcessor& ingest) {
  if (!dc::parseSceneDocument(kSceneJson, doc)) return false;
  cp.setIngestProcessor(&ingest);
  dc::SceneReconciler reconciler(cp);
  auto result = reconciler.reconcile(doc, scene);
  if (!result.ok) return false;
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

// One golden probe (on-screen pixel -> expected RGB within tol).
struct Probe { int x, y, r, g, b, tol; };

int main(int argc, char** argv) {
  if ((argc > 1 && std::strcmp(argv[1], "--capture") == 0) ||
      std::getenv("DC_GOLDEN_CAPTURE"))
    g_capture = true;

  std::printf("=== ENC-501 Dawn-only JsonHost golden conformance + perf ===\n");
  constexpr int W = 96, H = 64;

  // --- Dawn JsonHost render (the cutover path; top-left RGBA readback). -----
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

  auto at = [&](int x, int y) -> const std::uint8_t* {
    return &dawnFb[(static_cast<std::size_t>(y) * W + x) * 4];
  };

  // --- Golden probes (captured from the GL-validated Dawn JsonHost output). --
  // triSolid RED triangle (left third), instancedRect GREEN (center), line2d
  // BLUE (right third, y=0 -> mid row), plus a clear corner. Solid-fill scene:
  // small per-channel tolerance.
  const std::vector<Probe> probes = {
      {18, 38, 255, 0, 0, 16},   // inside the red triangle (left third)
      {48, 32, 0, 255, 0, 16},   // inside the green instanced rect (center)
      {80, 31, 0, 0, 255, 60},   // on the blue line (right third, mid row)
      {88, 4, 0, 0, 0, 16},      // clear top-right corner
  };
  bool ok = true;
  for (const auto& p : probes) {
    const std::uint8_t* px = at(p.x, p.y);
    if (g_capture) {
      std::printf("  [capture jsonhost] (%2d,%2d) => %3d %3d %3d\n", p.x, p.y,
                  px[0], px[1], px[2]);
      continue;
    }
    bool hit = std::abs(int(px[0]) - p.r) <= p.tol &&
               std::abs(int(px[1]) - p.g) <= p.tol &&
               std::abs(int(px[2]) - p.b) <= p.tol;
    if (!hit) {
      ok = false;
      std::fprintf(stderr,
                   "  probe(%d,%d): got %d %d %d  want %d %d %d (+-%d)\n",
                   p.x, p.y, px[0], px[1], px[2], p.r, p.g, p.b, p.tol);
    }
  }
  if (!g_capture)
    check(ok, "JsonHost Dawn readback matches captured golden probes");

  // --- Streaming perf sweep: N writeRange()+uploadDirty() cycles on Dawn. ----
  {
    const int kCycles = 2000;
    const std::uint32_t kChunk = 256;  // bytes appended per cycle
    std::vector<std::uint8_t> payload(kChunk);
    for (std::uint32_t i = 0; i < kChunk; ++i)
      payload[i] = static_cast<std::uint8_t>(i & 0xFF);

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
        "    Dawn CpuBufferStore   : %s\n",
        kCycles, kChunk,
        dawnMs >= 0 ? (std::to_string(dawnMs) + " ms").c_str()
                    : "(Dawn unavailable)");
    if (dawnMs >= 0)
      std::printf("    per-cycle Dawn        : %.4f ms\n", dawnMs / kCycles);
  }

  if (g_capture) { std::printf("=== capture complete ===\n"); return 0; }
  std::printf("=== JsonHost-Dawn golden: %d passed, %d failed (dawn=%s) ===\n",
              g_passed, g_failed, dawnBackend.c_str());
  return g_failed > 0 ? 1 : 0;
}
