// ENC-615 (P2) — PROOF: a weather-radar / KDE-style HEATMAP grid goes native FROM
// RAW DATA, with ZERO precompute. A grid of cells, each colored by its MAGNITUDE
// through a SequentialColorScale, renders in ONE instancedRectColor@1 draw
// (RESEARCH §4.2/§4.3): table -> scale -> encode -> instancedRectColor.
//
// THE CHAIN (no precomputed geometry / no precomputed color anywhere)
// -------------------------------------------------------------------
//   1. RAW data: a TableStore with cell rect columns (x0,y0,x1,y1 in clip space)
//      + a raw f32 `mag` magnitude column. The ONLY substrate.
//   2. SequentialColorScale auto-domains over `mag` (running [min,max]) and bakes
//      each row's magnitude -> packed RGBA8 (viridis ramp). The packed u32s are
//      appended as an i32 color column (the keystone per-cell color input).
//   3. EncodePass(Mark::RectColor) compiles (table, encoding) -> a byte-exact
//      Rect4Color (24B) instance buffer + an instancedRectColor@1 DrawItem, the
//      per-cell color riding setColorField("col").
//   4. DawnInstancedRectColorBackend renders all cells in ONE instanced draw into
//      the headless RGBA8 target; we read back and assert each cell shows ITS OWN
//      magnitude color, the colors are DISTINCT across magnitudes, and non-blank.
//
// Y-FLIP (RESEARCH note): the WGSL NEGATES clip.y. Sample framebuffer pixels with
//   pxX = (clipX+1)/2 * W,  pxY = (clipY+1)/2 * H
// (NOT (1-(clipY+1)/2)*H) — clip-top maps to framebuffer-BOTTOM, and this formula
// already accounts for it. An ASCII dump establishes ground truth.
//
// Force lavapipe on a headless box:
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

// One 13-byte ingest APPEND record (op=1) — the EXACT existing wire format.
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
  std::printf("=== ENC-615 PROOF: weather-radar HEATMAP native (raw->scale->encode->draw) ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;
  constexpr int kCols = 4;
  constexpr int kRows = 4;
  constexpr int kN = kCols * kRows;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json (lavapipe).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // ----- RAW DATA: a 4x4 grid of clip-space cells + a raw magnitude column. ----
  // Cell (r,c) spans clip x in [-1+c*0.5, -1+(c+1)*0.5], y in [-1+r*0.5, ...].
  // Magnitude ramps with the linear cell index so each cell gets a distinct color.
  std::vector<float> x0(kN), y0(kN), x1(kN), y1(kN), mag(kN);
  const float cw = 2.0f / kCols;
  const float ch = 2.0f / kRows;
  for (int r = 0; r < kRows; ++r) {
    for (int c = 0; c < kCols; ++c) {
      const int i = r * kCols + c;
      x0[i] = -1.0f + static_cast<float>(c) * cw;
      x1[i] = x0[i] + cw;
      y0[i] = -1.0f + static_cast<float>(r) * ch;
      y1[i] = y0[i] + ch;
      mag[i] = static_cast<float>(i);  // 0..15 magnitude ramp
    }
  }

  dc::IngestProcessor ingest;
  auto src = dc::makeBufferByteSource(ingest);
  dc::TableStore tables;
  const dc::Id kTable = 1;
  const dc::Id kX0 = 10, kY0 = 11, kX1 = 12, kY1 = 13, kMag = 14, kCol = 15;
  tables.defineTable(kTable, "grid");
  tables.addColumn(kTable, "x0", dc::DType::F32, kX0);
  tables.addColumn(kTable, "y0", dc::DType::F32, kY0);
  tables.addColumn(kTable, "x1", dc::DType::F32, kX1);
  tables.addColumn(kTable, "y1", dc::DType::F32, kY1);
  tables.addColumn(kTable, "mag", dc::DType::F32, kMag);
  tables.addColumn(kTable, "col", dc::DType::I32, kCol);
  appendF32(ingest, kX0, x0);
  appendF32(ingest, kY0, y0);
  appendF32(ingest, kX1, x1);
  appendF32(ingest, kY1, y1);
  appendF32(ingest, kMag, mag);

  // ----- SequentialColorScale: auto-domain over `mag`, bake RGBA8 per cell. -----
  dc::SequentialColorScale colorScale(dc::ColorRamp::viridis());
  colorScale.bindColumn(kTable, "mag");
  check(colorScale.updateDomain(tables, src), "scale: auto-domained over mag");
  check(colorScale.domain().min == 0.0 && colorScale.domain().max == 15.0,
        "scale: domain == [0,15] (running min/max over raw mag)");

  std::vector<std::int32_t> colorCol(kN);
  for (int i = 0; i < kN; ++i) {
    colorCol[i] = static_cast<std::int32_t>(colorScale.mapU32(mag[i]));
  }
  appendI32(ingest, kCol, colorCol);  // the keystone per-cell color column

  // ----- EncodePass(Mark::RectColor): table -> byte-exact Rect4Color buffer. ----
  dc::Encoding enc;
  enc.field(dc::Channel::X, "x0");
  enc.field(dc::Channel::Y, "y0");
  enc.field(dc::Channel::X2, "x1");
  enc.field(dc::Channel::Y2, "y1");
  enc.setColorField("col");

  dc::EncodePass pass;
  dc::EncodeResult res = pass.compile(dc::Mark::RectColor, enc, tables, kTable,
                                      src, /*geo*/ 100, /*di*/ 3, /*vbuf*/ 50);
  check(res.ok, "encode: RectColor compiled (raw table -> Rect4Color)");
  check(res.drawItem.pipeline == std::string("instancedRectColor@1"),
        "encode: pipeline == instancedRectColor@1");
  check(res.geometry.format == dc::VertexFormat::Rect4Color,
        "encode: geometry format == Rect4Color");
  check(res.bytes.size() == static_cast<std::size_t>(kN) * 24u,
        "encode: 16 cells * 24B Rect4Color stride");

  // ----- Wire the encoded geometry + draw item into a Scene, render on Dawn. ----
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

  // Sample a cell center: clip center -> framebuffer pixel via the Y-FLIP formula
  //   pxX = (clipX+1)/2 * W,  pxY = (clipY+1)/2 * H.
  auto cellPixel = [&](int r, int c, std::uint8_t* o) {
    const float clipX = (x0[r * kCols + c] + x1[r * kCols + c]) * 0.5f;
    const float clipY = (y0[r * kCols + c] + y1[r * kCols + c]) * 0.5f;
    const int pxX = static_cast<int>((clipX + 1.0f) * 0.5f * W);
    const int pxY = static_cast<int>((clipY + 1.0f) * 0.5f * H);
    dev.readPixel(pxX, pxY, o);
  };

  // Ground-truth ASCII dump: each cell's brightness (R+G+B) as a coarse glyph.
  std::printf("  framebuffer cell grid (intensity . : + * # by R+G+B):\n");
  for (int r = kRows - 1; r >= 0; --r) {  // print top clip row first visually
    std::printf("  ");
    for (int c = 0; c < kCols; ++c) {
      std::uint8_t px[4];
      cellPixel(r, c, px);
      const int s = px[0] + px[1] + px[2];
      const char* g = " .:+*#";
      int idx = s * 5 / (255 * 3);
      if (idx > 5) idx = 5;
      std::putchar(g[idx]);
    }
    std::putchar('\n');
  }

  // Each cell reads back ITS OWN magnitude color == the scale's mapU32(mag).
  bool allMatch = true;
  bool anyNonBlank = false;
  for (int i = 0; i < kN; ++i) {
    std::uint8_t px[4];
    cellPixel(i / kCols, i % kCols, px);
    const dc::Rgba8 want = colorScale.mapColor(mag[i]);
    // Allow a small tolerance for any internal rounding.
    const int dr = std::abs(int(px[0]) - int(want.r));
    const int dg = std::abs(int(px[1]) - int(want.g));
    const int db = std::abs(int(px[2]) - int(want.b));
    if (dr > 6 || dg > 6 || db > 6) allMatch = false;
    if (px[0] + px[1] + px[2] > 24) anyNonBlank = true;
  }
  check(allMatch, "render: EVERY cell == its SequentialColorScale magnitude color");
  check(anyNonBlank, "render: cells are non-blank");

  // The lowest and highest magnitude cells are DISTINCT colors (the scale spans).
  std::uint8_t lo[4], hi[4];
  cellPixel(0, 0, lo);                       // mag 0
  cellPixel(kRows - 1, kCols - 1, hi);       // mag 15
  std::printf("  mag=0  R=%u G=%u B=%u\n", lo[0], lo[1], lo[2]);
  std::printf("  mag=15 R=%u G=%u B=%u\n", hi[0], hi[1], hi[2]);
  check(lo[0] != hi[0] || lo[1] != hi[1] || lo[2] != hi[2],
        "render: lowest vs highest magnitude cells are DISTINCT colors");

  std::printf("=== ENC-615 heatmap native: %d passed, %d failed ===\n", passed,
              failed);
  return failed > 0 ? 1 : 0;
}
