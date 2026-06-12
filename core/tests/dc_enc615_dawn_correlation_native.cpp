// ENC-615 (P2) — PROOF: an N×N CORRELATION matrix goes native FROM RAW DATA, with
// ZERO precompute. Each cell's correlation value in [-1,1] colors through a
// DivergingColorScale (red…neutral…green, fixed mid 0) and renders in ONE
// instancedRectColor@1 draw (RESEARCH §4.2/§4.3): table -> diverging scale ->
// encode -> instancedRectColor.
//
// THE CHAIN (no precomputed geometry / color)
// -------------------------------------------
//   1. RAW data: an N×N grid of clip-space cells + a raw f32 `corr` column with
//      values across [-1,1] (incl. the diagonal == +1 self-correlation).
//   2. DivergingColorScale (FixedEpoch baseline policy, mid 0) maps each value to
//      a packed RGBA8: negative -> red side, ~0 -> neutral white, positive ->
//      green side. The mid (0) always lands on the neutral ramp color.
//   3. EncodePass(Mark::RectColor) -> byte-exact Rect4Color + instancedRectColor.
//   4. Dawn renders all cells in ONE draw; we assert each cell == its diverging
//      color, that POSITIVE cells are green-dominant, NEGATIVE red-dominant, the
//      diagonal is the strongest green, and the colors are distinct + non-blank.
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
  std::printf("=== ENC-615 PROOF: CORRELATION matrix native (raw->diverging->encode->draw) ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;
  constexpr int kN = 4;  // 4x4 correlation matrix
  constexpr int kCells = kN * kN;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // ----- RAW DATA: an N×N matrix of cells + a correlation value per cell. -------
  // corr(i,j) is symmetric, diagonal == +1, off-diagonals span [-1,1] so the
  // diverging scale exercises BOTH sides + the neutral mid.
  auto corrVal = [](int i, int j) -> float {
    if (i == j) return 1.0f;
    // A spread of values: alternate sign, magnitude grows with distance.
    const float d = static_cast<float>((i - j));
    float v = d * 0.5f;
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
  };

  std::vector<float> x0(kCells), y0(kCells), x1(kCells), y1(kCells),
      corr(kCells);
  const float cw = 2.0f / kN;
  const float ch = 2.0f / kN;
  for (int i = 0; i < kN; ++i) {
    for (int j = 0; j < kN; ++j) {
      const int k = i * kN + j;
      x0[k] = -1.0f + static_cast<float>(j) * cw;
      x1[k] = x0[k] + cw;
      y0[k] = -1.0f + static_cast<float>(i) * ch;
      y1[k] = y0[k] + ch;
      corr[k] = corrVal(i, j);
    }
  }

  dc::IngestProcessor ingest;
  auto src = dc::makeBufferByteSource(ingest);
  dc::TableStore tables;
  const dc::Id kTable = 1;
  const dc::Id kX0 = 10, kY0 = 11, kX1 = 12, kY1 = 13, kCorr = 14, kCol = 15;
  tables.defineTable(kTable, "corr");
  tables.addColumn(kTable, "x0", dc::DType::F32, kX0);
  tables.addColumn(kTable, "y0", dc::DType::F32, kY0);
  tables.addColumn(kTable, "x1", dc::DType::F32, kX1);
  tables.addColumn(kTable, "y1", dc::DType::F32, kY1);
  tables.addColumn(kTable, "corr", dc::DType::F32, kCorr);
  tables.addColumn(kTable, "col", dc::DType::I32, kCol);
  appendF32(ingest, kX0, x0);
  appendF32(ingest, kY0, y0);
  appendF32(ingest, kX1, x1);
  appendF32(ingest, kY1, y1);
  appendF32(ingest, kCorr, corr);

  // ----- DivergingColorScale: fixed mid 0, domain [-1,1], red…white…green. ------
  // Class-4: a diverging scale REQUIRES a baseline policy (else nullptr). Use
  // FixedEpoch (the streaming-defined accumulation in this PR).
  auto colorScale = dc::makeDivergingColorScale(
      0.0, dc::ColorRamp::redNeutralGreen(), dc::BaselinePolicy::fixedEpoch());
  check(colorScale != nullptr,
        "scale: diverging scale built WITH a baseline policy (class-4 accepted)");
  // Reject path: no policy -> nullptr (the class-4 guard).
  check(dc::makeDivergingColorScale(0.0, dc::ColorRamp::redNeutralGreen(),
                                    dc::BaselinePolicy{}) == nullptr,
        "scale: policy-less diverging scale REJECTED (class-4 guard)");

  colorScale->bindColumn(kTable, "corr");
  check(colorScale->updateDomain(tables, src), "scale: auto-domained over corr");
  check(colorScale->domain().min == -1.0 && colorScale->domain().max == 1.0,
        "scale: domain == [-1,1]");
  check(colorScale->mid() == 0.0, "scale: fixed mid == 0");

  std::vector<std::int32_t> colorCol(kCells);
  for (int k = 0; k < kCells; ++k) {
    colorCol[k] = static_cast<std::int32_t>(colorScale->mapU32(corr[k]));
  }
  appendI32(ingest, kCol, colorCol);

  // ----- EncodePass(Mark::RectColor) -> byte-exact Rect4Color buffer. -----------
  dc::Encoding enc;
  enc.field(dc::Channel::X, "x0");
  enc.field(dc::Channel::Y, "y0");
  enc.field(dc::Channel::X2, "x1");
  enc.field(dc::Channel::Y2, "y1");
  enc.setColorField("col");

  dc::EncodePass pass;
  dc::EncodeResult res = pass.compile(dc::Mark::RectColor, enc, tables, kTable,
                                      src, 100, 3, 50);
  check(res.ok, "encode: RectColor compiled (raw corr table -> Rect4Color)");
  check(res.bytes.size() == static_cast<std::size_t>(kCells) * 24u,
        "encode: 16 cells * 24B Rect4Color");

  // ----- Render on Dawn. --------------------------------------------------------
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
  requireOk(cp.applyJsonText(
                R"({"cmd":"createBuffer","id":50,"byteLength":384})"), "buf");
  store.setCpuData(50, res.bytes.data(),
                   static_cast<std::uint32_t>(res.bytes.size()));
  requireOk(cp.applyJsonText(
                R"({"cmd":"createGeometry","id":100,"vertexBufferId":50,"vertexCount":16,"format":"rect4_color"})"),
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
  check(bs.drawCalls == 1, "render: ONE instanced draw call (16 cells)");

  auto cellPixel = [&](int i, int j, std::uint8_t* o) {
    const int k = i * kN + j;
    const float clipX = (x0[k] + x1[k]) * 0.5f;
    const float clipY = (y0[k] + y1[k]) * 0.5f;
    const int pxX = static_cast<int>((clipX + 1.0f) * 0.5f * W);
    const int pxY = static_cast<int>((clipY + 1.0f) * 0.5f * H);
    dev.readPixel(pxX, pxY, o);
  };

  // Ground-truth ASCII: '+' green-dominant (positive), '-' red-dominant (negative),
  // '0' neutral.
  std::printf("  correlation sign grid (+ green / - red / 0 neutral):\n");
  for (int i = kN - 1; i >= 0; --i) {
    std::printf("  ");
    for (int j = 0; j < kN; ++j) {
      std::uint8_t px[4];
      cellPixel(i, j, px);
      char g = '0';
      if (px[1] > px[0] + 20) g = '+';
      else if (px[0] > px[1] + 20) g = '-';
      std::putchar(g);
    }
    std::putchar('\n');
  }

  // Every cell == its diverging color (mapColor).
  bool allMatch = true, anyNonBlank = false;
  for (int i = 0; i < kN; ++i) {
    for (int j = 0; j < kN; ++j) {
      std::uint8_t px[4];
      cellPixel(i, j, px);
      const dc::Rgba8 want = colorScale->mapColor(corr[i * kN + j]);
      if (std::abs(int(px[0]) - int(want.r)) > 6 ||
          std::abs(int(px[1]) - int(want.g)) > 6 ||
          std::abs(int(px[2]) - int(want.b)) > 6)
        allMatch = false;
      if (px[0] + px[1] + px[2] > 24) anyNonBlank = true;
    }
  }
  check(allMatch, "render: EVERY cell == its DivergingColorScale value color");
  check(anyNonBlank, "render: cells non-blank");

  // The diagonal (+1) is green-dominant; a strongly-negative cell is red-dominant.
  std::uint8_t diag[4], neg[4], midc[4];
  cellPixel(0, 0, diag);  // corr +1 -> strong green
  cellPixel(0, 3, neg);   // corr (0-3)*0.5 clamped -> -1 -> strong red
  cellPixel(0, 1, midc);  // corr (0-1)*0.5 = -0.5 -> mild red
  std::printf("  diag(+1)  R=%u G=%u B=%u\n", diag[0], diag[1], diag[2]);
  std::printf("  cell(-1)  R=%u G=%u B=%u\n", neg[0], neg[1], neg[2]);
  check(diag[1] > diag[0], "render: diagonal (+1) is GREEN-dominant (positive)");
  check(neg[0] > neg[1], "render: corr=-1 cell is RED-dominant (negative)");
  check((diag[0] != neg[0] || diag[1] != neg[1] || diag[2] != neg[2]),
        "render: +1 and -1 cells are DISTINCT (diverging spans both sides)");

  std::printf("=== ENC-615 correlation native: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
