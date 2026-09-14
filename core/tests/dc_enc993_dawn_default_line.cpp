// ENC-993 — PIXEL PROOF that the default line mark is now antialiased.
//
// dc_enc993_lineaa_default.cpp proves the *routing* (a line mark resolves to
// lineAA@1) in pure logic. This test closes the loop on the GPU: it takes the
// SAME table + encoding, compiles it twice through the real EncodePass — once
// with the DEFAULT arguments and once with the explicit LineStyle::Line2d
// escape hatch — renders both through the real Dawn backends into the headless
// offscreen target, and reads the framebuffer back.
//
// The claim being proved is about COVERAGE, which is the only antialiasing the
// engine has (every render target is sampleCount == 1, there is no MSAA):
//
//   * default (lineAA@1): the diagonal has PARTIAL-coverage pixels along its
//     edges — the fragment shader's distance-to-line falloff.
//   * escape hatch (line2d@1): the same diagonal has ZERO partial-coverage
//     pixels. Every lit pixel is full-intensity: a hard, stair-stepped 1px
//     LineList. That is exactly what every SMA / MACD / Bollinger / axis-tick
//     line looked like before this ticket.
//
// Both maps are printed as ASCII so the difference is legible in the ctest log.
//
// On a headless box the only Vulkan backend may be lavapipe (software):
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/commands/CommandProcessor.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnLine2dBackend.hpp"
#include "dc/gpu/DawnLineAABackend.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/render/BackendRegistry.hpp"
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

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx, r.err.code.c_str(),
                 r.err.message.c_str());
    std::exit(1);
  }
}

static void appendF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> wire;
  auto u32 = [&wire](std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      wire.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
  };
  const auto len = static_cast<std::uint32_t>(vals.size() * sizeof(float));
  wire.push_back(1);
  u32(static_cast<std::uint32_t>(buf));
  u32(0);
  u32(len);
  const auto* p = reinterpret_cast<const std::uint8_t*>(vals.data());
  wire.insert(wire.end(), p, p + len);
  ingest.processBatch(wire.data(), static_cast<std::uint32_t>(wire.size()));
}

static const char* formatName(dc::VertexFormat f) {
  switch (f) {
    case dc::VertexFormat::Pos2_Clip: return "pos2_clip";
    case dc::VertexFormat::Rect4: return "rect4";
    default: return "";
  }
}

constexpr std::uint32_t W = 64;
constexpr std::uint32_t H = 64;

struct Coverage {
  std::uint32_t lit{0};      // green > 16
  std::uint32_t partial{0};  // 16 < green < 240  (an AA fringe pixel)
  std::uint32_t full{0};     // green >= 240
};

int main() {
  std::printf("=== ENC-993 pixel proof: the DEFAULT line mark is antialiased ===\n");

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n", dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json forces lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  dc::DawnLineAABackend lineAA;
  dc::DawnLine2dBackend line2d;
  if (!lineAA.init(dev) || !line2d.init(dev)) {
    std::fprintf(stderr, "backend init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &lineAA);
  backends.registerBackend(dc::DeviceKind::Dawn, &line2d);

  // ---- the ONE table + encoding both compiles read ------------------------
  // A diagonal, so the difference between coverage AA and a hard LineList is
  // unmistakable: a diagonal line2d line is a stair-step of full-intensity
  // pixels, with nothing in between.
  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);
  const dc::Id kTable = 1, kX = 10, kY = 11;
  tables.defineTable(kTable, "series");
  tables.addColumn(kTable, "x", dc::DType::F32, kX);
  tables.addColumn(kTable, "y", dc::DType::F32, kY);
  appendF32(ingest, kX, {-0.8f, 0.8f});
  appendF32(ingest, kY, {-0.8f, 0.8f});

  dc::Encoding enc;
  enc.field(dc::Channel::X, "x").field(dc::Channel::Y, "y");
  dc::EncodePass pass;

  // Render one EncodeResult and return the coverage histogram + the ASCII map.
  auto renderCoverage = [&](const dc::EncodeResult& res, bool applyWidth,
                            const char* label) -> Coverage {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":)" +
                               std::to_string(res.bytes.size()) + "}"),
              "buf");
    store.setCpuData(10, res.bytes.data(),
                     static_cast<std::uint32_t>(res.bytes.size()));

    requireOk(cp.applyJsonText(
                  R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":)" +
                  std::to_string(res.geometry.vertexCount) + R"(,"format":")" +
                  formatName(res.geometry.format) + R"("})"),
              "geom");
    // The pipeline key comes STRAIGHT off the encode result — this is the
    // routing under test, not a hardcoded string.
    requireOk(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":")" +
                               res.drawItem.pipeline + R"(","geometryId":100})"),
              "bind");
    requireOk(cp.applyJsonText(
                  R"({"cmd":"setDrawItemColor","drawItemId":3,"r":0,"g":1,"b":0,"a":1})"),
              "color");
    if (applyWidth) {
      requireOk(cp.applyJsonText(
                    R"({"cmd":"setDrawItemStyle","drawItemId":3,"lineWidth":5})"),
                "style");
    }

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
    if (!be) {
      std::fprintf(stderr, "no backend for %s\n", di->pipeline.c_str());
      std::exit(1);
    }
    be->renderDrawItem(dev, scene, store, *di, static_cast<int>(W),
                       static_cast<int>(H));
    dev.endRenderPass();

    std::vector<std::uint8_t> fb(static_cast<std::size_t>(W) * H * 4, 0);
    std::uint32_t gotW = 0, gotH = 0;
    if (!dev.readFramebufferRGBA(fb.data(), fb.size(), &gotW, &gotH)) {
      std::fprintf(stderr, "readFramebufferRGBA failed\n");
      std::exit(1);
    }

    Coverage c;
    std::printf("\n  --- %s  (pipeline=%s, format=%s) ---\n", label,
                res.drawItem.pipeline.c_str(), formatName(res.geometry.format));
    std::printf("  '#' full coverage   '+' PARTIAL (antialiased edge)   '.' clear\n");
    for (std::uint32_t y = 0; y < H; ++y) {
      std::printf("  ");
      for (std::uint32_t x = 0; x < W; ++x) {
        const std::uint8_t g = fb[(static_cast<std::size_t>(y) * W + x) * 4 + 1];
        char ch = '.';
        if (g >= 240) { ++c.full; ++c.lit; ch = '#'; }
        else if (g > 16) { ++c.partial; ++c.lit; ch = '+'; }
        std::printf("%c", ch);
      }
      std::printf("\n");
    }
    std::printf("  lit=%u  full=%u  PARTIAL(AA)=%u\n", c.lit, c.full, c.partial);
    return c;
  };

  // =======================================================================
  // A. The DEFAULT compile — no lineStyle argument at all, no style command.
  //    This is exactly what an unannotated line mark gets today.
  // =======================================================================
  auto def = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 100, 200, 300);
  check(def.ok && def.drawItem.pipeline == "lineAA@1",
        "default compile routes to lineAA@1");
  Coverage aaDefault = renderCoverage(def, /*applyWidth=*/false,
                                      "DEFAULT line mark (lineWidth untouched)");

  // =======================================================================
  // B. The ESCAPE HATCH compile — explicit LineStyle::Line2d, same data.
  // =======================================================================
  auto flat = pass.compile(dc::Mark::Line, enc, tables, kTable, src, 101, 201, 301,
                           nullptr, dc::LineStyle::Line2d);
  check(flat.ok && flat.drawItem.pipeline == "line2d@1",
        "escape-hatch compile routes to line2d@1");
  Coverage flatCov = renderCoverage(flat, /*applyWidth=*/false,
                                    "ESCAPE HATCH line2d@1 (what it used to be)");

  // =======================================================================
  // C. The default ALSO honours lineWidth — line2d@1 structurally cannot.
  // =======================================================================
  Coverage aaThick = renderCoverage(def, /*applyWidth=*/true,
                                    "DEFAULT line mark, lineWidth=5");

  std::printf("\n");
  check(flatCov.lit > 0, "line2d@1 drew the segment at all");
  check(flatCov.partial == 0,
        "line2d@1 has ZERO partial-coverage pixels — a hard aliased LineList");
  check(aaDefault.lit > 0, "lineAA@1 drew the segment at all");
  check(aaDefault.partial > 0,
        "lineAA@1 HAS partial-coverage pixels — the default is antialiased");
  check(aaThick.partial > aaDefault.partial,
        "lineAA@1 at lineWidth=5 has a wider AA fringe than at the default width");
  check(aaThick.lit > flatCov.lit,
        "lineAA@1 honours lineWidth; line2d@1 is stuck at 1px");

  std::printf("\nENC-993 pixel proof: %d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
