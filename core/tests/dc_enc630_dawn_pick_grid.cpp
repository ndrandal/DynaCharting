// ENC-630 (C4) — per-instance pick golden / round-trip on an instanced grid.
//
// THE END-TO-END PROOF of Phase C: ONE instancedRectColor@1 DrawItem holding a
// 4x4 grid of rects (16 instances, the treemap/heatmap shape). For EVERY cell we
// compute its center pixel, click it, and assert the pick resolves back to that
// cell's durable SOURCE ROW id — exercising the full chain: id pass (which
// DrawItem) -> instance-index pass (ENC-628, which instance) -> PickInstanceTable
// (ENC-627/C1, instance -> row id). A click in the border gap resolves to no hit.
//
// PIXEL MAPPING: the color backends negate clip-y in the WGSL and DawnDevice reads
// back top-down; the two cancel, so a clip-space center (cx, cy) is read at
// px = (cx + 1)/2 * W,  py = (cy + 1)/2 * H  (verified against d_enc608's quadrant
// reads). Instances are laid out row-major from clip bottom-left: instance index
// = r*4 + c, durable row id = 1000 + index.
//
// Force the software Vulkan ICD on a headless box:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnPickBackend.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Types.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    ++passed;  // quiet on pass — 16 cells would be noisy; the summary prints totals
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

// Pack one Rect4Color instance (24B): rect4 f32 @0 + packed RGBA8 @16 + reserved
// scalar/row-id lane @20.
static void pushRectColor(std::vector<std::uint8_t>& out, float x0, float y0,
                          float x1, float y1, std::uint32_t rgba8) {
  auto pf = [&](float v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  auto pu = [&](std::uint32_t v) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  pf(x0); pf(y0); pf(x1); pf(y1);
  pu(rgba8);
  pu(0u);
}

int main() {
  std::printf("=== ENC-630 (C4) Dawn per-instance pick grid round-trip ===\n");

  constexpr int W = 64;
  constexpr int H = 64;
  constexpr int N = 4;  // 4x4 grid -> 16 instances

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

  dc::DawnPickBackend pick;
  if (!pick.init(dev)) {
    std::fprintf(stderr, "DawnPickBackend::init failed\n");
    return 1;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di");

  // The grid spans clip [-0.8, 0.8] in both axes (a centered 4x4 with a border so
  // a corner pick is background). Cell pitch 0.4 clip; each rect is inset 0.04 on
  // every side so adjacent cells leave a thin gap (centers stay solid).
  const float kOrigin = -0.8f;
  const float kPitch = 0.4f;
  const float kInset = 0.04f;
  const std::uint32_t kColors[4] = {0xFF0000FFu, 0xFF00FF00u, 0xFFFF0000u,
                                    0xFF00FFFFu};

  std::vector<std::uint8_t> verts;
  std::vector<std::int32_t> rowIds;
  for (int r = 0; r < N; ++r) {
    for (int c = 0; c < N; ++c) {
      const float x0 = kOrigin + c * kPitch + kInset;
      const float x1 = kOrigin + (c + 1) * kPitch - kInset;
      const float y0 = kOrigin + r * kPitch + kInset;
      const float y1 = kOrigin + (r + 1) * kPitch - kInset;
      pushRectColor(verts, x0, y0, x1, y1, kColors[(r + c) % 4]);
      rowIds.push_back(1000 + (r * N + c));  // instance order == record order
    }
  }

  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":384})"), "buf");
  store.setCpuData(10, verts.data(), static_cast<std::uint32_t>(verts.size()));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":16,"format":"rect4_color"})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"instancedRectColor@1","geometryId":100})"),
      "bind");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":5,"cornerRadius":0})"), "style");

  // C1: register the durable row ids (instance order).
  pick.setInstanceRowIds(5, rowIds);

  // Click every cell center; assert the pick resolves to that cell's instance and
  // durable row id.
  int cellsOk = 0;
  for (int r = 0; r < N; ++r) {
    for (int c = 0; c < N; ++c) {
      const float cx = kOrigin + (c + 0.5f) * kPitch;
      const float cy = kOrigin + (r + 0.5f) * kPitch;
      const int px = static_cast<int>((cx + 1.0f) * 0.5f * W + 0.5f);
      const int py = static_cast<int>((cy + 1.0f) * 0.5f * H + 0.5f);
      const int idx = r * N + c;
      const std::int32_t expectRow = 1000 + idx;

      auto res = pick.renderPick(dev, scene, store, W, H, px, py);
      const bool ok = res.drawItemId == 5 && res.instanceIndex == idx &&
                      res.rowId == expectRow;
      if (!ok) {
        std::fprintf(stderr,
                     "  cell(c=%d,r=%d) px(%d,%d): id=%u inst=%d row=%d "
                     "(expect id=5 inst=%d row=%d)\n",
                     c, r, px, py, res.drawItemId, res.instanceIndex, res.rowId,
                     idx, expectRow);
      }
      check(ok, "grid cell round-trip");
      if (ok) ++cellsOk;
    }
  }
  std::printf("  %d/%d grid cells round-tripped (id -> instance -> rowId)\n",
              cellsOk, N * N);
  check(cellsOk == N * N, "ALL 16 grid cells resolve to the correct source row");

  // A border-gap / corner click is background: no DrawItem, no instance, no row.
  auto bg = pick.renderPick(dev, scene, store, W, H, 2, 2);
  std::printf("  corner (2,2): id=%u inst=%d row=%d (expect 0, -1, -1)\n",
              bg.drawItemId, bg.instanceIndex, bg.rowId);
  check(bg.drawItemId == 0, "corner -> no DrawItem");
  check(bg.instanceIndex == -1, "corner -> no instance");
  check(bg.rowId == -1, "corner -> no row id");

  std::printf("=== ENC-630 pick grid: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
