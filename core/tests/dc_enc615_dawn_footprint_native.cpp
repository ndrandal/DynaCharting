// ENC-615 (P2) — PROOF: a price×time FOOTPRINT (orderflow) grid goes native FROM
// RAW DATA, with ZERO precompute. A footprint chart is a grid of per-cell rects:
// one column per time bar, one row per price level, each cell colored by its
// traded VOLUME at that (time, price). It renders in ONE instancedRectColor@1
// draw (RESEARCH §4.3): table -> SequentialColorScale -> encode -> draw.
//
// THE CHAIN (no precomputed geometry / color)
// -------------------------------------------
//   1. RAW data: a price×time grid of clip-space cells + a raw f32 `vol` column
//      (volume traded in each price/time bucket). The ONLY substrate.
//   2. SequentialColorScale auto-domains over `vol` and bakes each cell's volume
//      -> packed RGBA8 (magma ramp). Empty buckets (vol 0) -> the ramp's low end.
//   3. EncodePass(Mark::RectColor) -> byte-exact Rect4Color + instancedRectColor.
//   4. Dawn renders the WHOLE footprint in ONE instanced draw; we assert each cell
//      shows ITS OWN volume color, hot vs cold cells are DISTINCT, non-blank.
//
// Y-FLIP: WGSL negates clip.y; sample pxX=(clipX+1)/2*W, pxY=(clipY+1)/2*H.
//
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnInstancedRectColorBackend.hpp"

#include "dc/commands/CommandProcessor.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/render/BackendRegistry.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/scale/ColorScale.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx, r.err.code.c_str(),
                 r.err.message.c_str());
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
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(1);
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}
static void appendF32(dc::IngestProcessor& ig, dc::Id buf,
                      const std::vector<float>& v) {
  std::vector<std::uint8_t> b;
  appendRecord(b, buf, v.data(), static_cast<std::uint32_t>(v.size() * 4));
  ig.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
}
static void appendI32(dc::IngestProcessor& ig, dc::Id buf,
                      const std::vector<std::int32_t>& v) {
  std::vector<std::uint8_t> b;
  appendRecord(b, buf, v.data(), static_cast<std::uint32_t>(v.size() * 4));
  ig.processBatch(b.data(), static_cast<std::uint32_t>(b.size()));
}

int main() {
  std::printf("=== ENC-615 PROOF: FOOTPRINT (price×time) native (raw->scale->encode->draw) ===\n");

  constexpr std::uint32_t W = 96;
  constexpr std::uint32_t H = 64;
  constexpr int kTimes = 6;   // time bars (columns)
  constexpr int kPrices = 4;  // price levels (rows)
  constexpr int kCells = kTimes * kPrices;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // ----- RAW DATA: a price×time grid + a volume per cell. ----------------------
  // Volume has a "hot zone" mid-grid (a price/time cluster of heavy trading) and
  // cold edges — exactly the structure a footprint chart reveals.
  std::vector<float> x0(kCells), y0(kCells), x1(kCells), y1(kCells), vol(kCells);
  const float cw = 2.0f / kTimes;
  const float ch = 2.0f / kPrices;
  for (int p = 0; p < kPrices; ++p) {
    for (int t = 0; t < kTimes; ++t) {
      const int k = p * kTimes + t;
      x0[k] = -1.0f + static_cast<float>(t) * cw;
      x1[k] = x0[k] + cw;
      y0[k] = -1.0f + static_cast<float>(p) * ch;
      y1[k] = y0[k] + ch;
      // Volume: peak near (t=3, p=2), tapering to the edges.
      const float dt = static_cast<float>(t - 3);
      const float dp = static_cast<float>(p - 2);
      const float dist2 = dt * dt + dp * dp;
      vol[k] = 1000.0f / (1.0f + dist2);  // distinct per cell, never zero
    }
  }

  dc::IngestProcessor ingest;
  auto src = dc::makeBufferByteSource(ingest);
  dc::TableStore tables;
  const dc::Id kTable = 1;
  const dc::Id kX0 = 10, kY0 = 11, kX1 = 12, kY1 = 13, kVol = 14, kCol = 15;
  tables.defineTable(kTable, "fp");
  tables.addColumn(kTable, "x0", dc::DType::F32, kX0);
  tables.addColumn(kTable, "y0", dc::DType::F32, kY0);
  tables.addColumn(kTable, "x1", dc::DType::F32, kX1);
  tables.addColumn(kTable, "y1", dc::DType::F32, kY1);
  tables.addColumn(kTable, "vol", dc::DType::F32, kVol);
  tables.addColumn(kTable, "col", dc::DType::I32, kCol);
  appendF32(ingest, kX0, x0);
  appendF32(ingest, kY0, y0);
  appendF32(ingest, kX1, x1);
  appendF32(ingest, kY1, y1);
  appendF32(ingest, kVol, vol);

  // ----- SequentialColorScale over volume (magma: cold->hot). ------------------
  dc::SequentialColorScale colorScale(dc::ColorRamp::magma());
  colorScale.bindColumn(kTable, "vol");
  check(colorScale.updateDomain(tables, src), "scale: auto-domained over vol");

  std::vector<std::int32_t> colorCol(kCells);
  for (int k = 0; k < kCells; ++k) {
    colorCol[k] = static_cast<std::int32_t>(colorScale.mapU32(vol[k]));
  }
  appendI32(ingest, kCol, colorCol);

  // ----- EncodePass(Mark::RectColor). ------------------------------------------
  dc::Encoding enc;
  enc.field(dc::Channel::X, "x0");
  enc.field(dc::Channel::Y, "y0");
  enc.field(dc::Channel::X2, "x1");
  enc.field(dc::Channel::Y2, "y1");
  enc.setColorField("col");

  dc::EncodePass pass;
  dc::EncodeResult res = pass.compile(dc::Mark::RectColor, enc, tables, kTable,
                                      src, 100, 3, 50);
  check(res.ok, "encode: RectColor compiled (raw footprint -> Rect4Color)");
  check(res.bytes.size() == static_cast<std::size_t>(kCells) * 24u,
        "encode: 24 cells * 24B Rect4Color");

  // ----- Render on Dawn. -------------------------------------------------------
  dc::DawnInstancedRectColorBackend backend;
  if (!backend.init(dev)) {
    std::fprintf(stderr, "DawnInstancedRectColorBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &backend);

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");
  const std::uint32_t byteLen = static_cast<std::uint32_t>(res.bytes.size());
  requireOk(cp.applyJsonText(std::string(R"({"cmd":"createBuffer","id":50,"byteLength":)") +
                             std::to_string(byteLen) + "}"),
            "buf");
  store.setCpuData(50, res.bytes.data(), byteLen);
  requireOk(cp.applyJsonText(
                R"({"cmd":"createGeometry","id":100,"vertexBufferId":50,"vertexCount":24,"format":"rect4_color"})"),
            "geom");
  requireOk(cp.applyJsonText(
                R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"instancedRectColor@1","geometryId":100})"),
            "bind");
  requireOk(cp.applyJsonText(
                R"({"cmd":"setDrawItemStyle","drawItemId":3,"cornerRadius":0})"),
            "style");

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
  dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                           static_cast<int>(W),
                                           static_cast<int>(H));
  dev.endRenderPass();
  check(bs.drawCalls == 1, "render: ONE instanced draw call (24 cells)");

  auto cellPixel = [&](int p, int t, std::uint8_t* o) {
    const int k = p * kTimes + t;
    const float clipX = (x0[k] + x1[k]) * 0.5f;
    const float clipY = (y0[k] + y1[k]) * 0.5f;
    const int pxX = static_cast<int>((clipX + 1.0f) * 0.5f * W);
    const int pxY = static_cast<int>((clipY + 1.0f) * 0.5f * H);
    dev.readPixel(pxX, pxY, o);
  };

  // Ground-truth ASCII: footprint intensity grid (R+G+B coarse glyph).
  std::printf("  footprint volume grid (cold ' .:+*#' hot):\n");
  for (int p = kPrices - 1; p >= 0; --p) {
    std::printf("  ");
    for (int t = 0; t < kTimes; ++t) {
      std::uint8_t px[4];
      cellPixel(p, t, px);
      const int s = px[0] + px[1] + px[2];
      const char* g = " .:+*#";
      int idx = s * 5 / (255 * 3);
      if (idx > 5) idx = 5;
      std::putchar(g[idx]);
    }
    std::putchar('\n');
  }

  // Every cell == its volume color.
  bool allMatch = true, anyNonBlank = false;
  for (int p = 0; p < kPrices; ++p) {
    for (int t = 0; t < kTimes; ++t) {
      std::uint8_t px[4];
      cellPixel(p, t, px);
      const dc::Rgba8 want = colorScale.mapColor(vol[p * kTimes + t]);
      if (std::abs(int(px[0]) - int(want.r)) > 6 ||
          std::abs(int(px[1]) - int(want.g)) > 6 ||
          std::abs(int(px[2]) - int(want.b)) > 6)
        allMatch = false;
      if (px[0] + px[1] + px[2] > 12) anyNonBlank = true;
    }
  }
  check(allMatch, "render: EVERY cell == its volume color (per-cell, one draw)");
  check(anyNonBlank, "render: footprint cells non-blank");

  // The hot cell (t=3,p=2) vs a cold corner (t=0,p=0) are DISTINCT.
  std::uint8_t hot[4], cold[4];
  cellPixel(2, 3, hot);
  cellPixel(0, 0, cold);
  std::printf("  hot(t3,p2)  R=%u G=%u B=%u\n", hot[0], hot[1], hot[2]);
  std::printf("  cold(t0,p0) R=%u G=%u B=%u\n", cold[0], cold[1], cold[2]);
  check(hot[0] != cold[0] || hot[1] != cold[1] || hot[2] != cold[2],
        "render: hot vs cold footprint cells are DISTINCT colors");
  // Hot cell brighter than cold (magma low end is dark).
  check(hot[0] + hot[1] + hot[2] > cold[0] + cold[1] + cold[2],
        "render: hot cell brighter than cold (volume magnitude -> ramp)");

  std::printf("=== ENC-615 footprint native: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
