// ENC-653 (C2c) — per-instance pick: gather-order <-> table-order consistency.
//
// The instance-index pick pass (ENC-628) numbers instances in GATHERED (draw)
// order. For a D26-INDEXED instanced geometry the gather repacks records
// (scratch[i] == record[index[i]]), so the gathered index is NOT the original
// record index — yet the PickInstanceTable (C1) is keyed in record/emit order
// (EncodeResult::instanceRowIds). DawnPickBackend::renderPick therefore maps the
// gathered index back THROUGH the index buffer before the table lookup.
//
// This golden test proves that mapping. Three Rect4Color records sit in distinct
// x bands in RECORD order (R0 left, R1 mid, R2 right), but the index buffer draws
// them in a SCRAMBLED order [2, 0, 1]. The durable row ids are registered in
// record order {100, 200, 300}. A click on the LEFT band must return R0's row
// (100) even though R0 is drawn at gathered position 1 — a naive gathered-order
// lookup would wrongly return 200. We assert the click maps to the original
// record index and row at each band.
//
// On this headless box force the software Vulkan ICD if no HW adapter:
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
    std::printf("  PASS: %s\n", name);
    ++passed;
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
  std::printf("=== ENC-653 Dawn per-instance pick ordering (indexed gather) ===\n");

  constexpr int W = 64;
  constexpr int H = 64;

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
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  // Three rects in RECORD order, distinct x bands (full height):
  //   R0 clip x[-0.9,-0.35] -> px center ~12  (LEFT)
  //   R1 clip x[-0.25,0.25] -> px center  32  (MID)
  //   R2 clip x[ 0.35,0.9 ] -> px center ~52  (RIGHT)
  std::vector<std::uint8_t> verts;
  pushRectColor(verts, -0.90f, -0.9f, -0.35f, 0.9f, 0xFF0000FFu);  // R0 red
  pushRectColor(verts, -0.25f, -0.9f,  0.25f, 0.9f, 0xFF00FF00u);  // R1 green
  pushRectColor(verts,  0.35f, -0.9f,  0.90f, 0.9f, 0xFFFF0000u);  // R2 blue
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})"), "vbuf");
  store.setCpuData(10, verts.data(), static_cast<std::uint32_t>(verts.size()));

  // SCRAMBLED draw order via the index buffer: draw R2, then R0, then R1. So
  // gathered position 0->R2, 1->R0, 2->R1 — gathered index != record index.
  std::uint32_t indices[3] = {2u, 0u, 1u};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":12})"), "ibuf");
  store.setCpuData(11, indices, sizeof(indices));

  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"rect4_color","indexBufferId":11,"indexCount":3})"),
      "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRectColor@1","geometryId":100})"),
      "bind");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})"), "style");

  // Durable row ids in RECORD order: R0->100, R1->200, R2->300.
  pick.setInstanceRowIds(3, {100, 200, 300});

  const int yMid = H / 2;
  auto rL = pick.renderPick(dev, scene, store, W, H, 12, yMid);  // LEFT  -> R0
  auto rM = pick.renderPick(dev, scene, store, W, H, 32, yMid);  // MID   -> R1
  auto rR = pick.renderPick(dev, scene, store, W, H, 52, yMid);  // RIGHT -> R2

  std::printf("  LEFT  x12 -> id=%u inst=%d row=%d (expect 3, 0, 100)\n",
              rL.drawItemId, rL.instanceIndex, rL.rowId);
  std::printf("  MID   x32 -> id=%u inst=%d row=%d (expect 3, 1, 200)\n",
              rM.drawItemId, rM.instanceIndex, rM.rowId);
  std::printf("  RIGHT x52 -> id=%u inst=%d row=%d (expect 3, 2, 300)\n",
              rR.drawItemId, rR.instanceIndex, rR.rowId);

  // The hit DrawItem is the same for all three.
  check(rL.drawItemId == 3 && rM.drawItemId == 3 && rR.drawItemId == 3,
        "all bands hit DrawItem 3");

  // The gathered index is mapped back to the ORIGINAL record index, so each band
  // reports its record index (not the scrambled draw position).
  check(rL.instanceIndex == 0, "LEFT band -> original record index 0 (R0)");
  check(rM.instanceIndex == 1, "MID band -> original record index 1 (R1)");
  check(rR.instanceIndex == 2, "RIGHT band -> original record index 2 (R2)");

  // And the durable row id is the one registered for that ORIGINAL record. A
  // naive gathered-order lookup would yield LEFT->200, which this refutes.
  check(rL.rowId == 100, "LEFT band -> row 100 (R0), NOT the gathered-order 200");
  check(rM.rowId == 200, "MID band -> row 200 (R1)");
  check(rR.rowId == 300, "RIGHT band -> row 300 (R2)");

  std::printf("=== ENC-653 pick ordering: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
