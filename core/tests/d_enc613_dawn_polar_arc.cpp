// ENC-613 (P2.3) — polar coords + arc/wedge mark on Dawn: the radial/pie render
// proof. Drives the WHOLE polar path end to end: a small table of (angle,radius)
// rows -> the encode pass Mark::Arc (POLAR coords) tessellates each wedge into a
// triGradient@1 (Pos2Color4) triangle fan in CLIP space (cx + r*cos, cy + r*sin)
// -> the DawnTriGradientBackend rasterizes it into the headless offscreen target.
//
// Asserts the result is NON-BLANK and that wedges at DIFFERENT angles show their
// OWN DISTINCT colors at the screen positions polar geometry predicts — which an
// affine mat3 could never produce (the whole reason polar is a coords mode).
//
// A 4-slice pie centered at clip origin, each quarter a different color:
//   slice0 [0,    pi/2 ] red    -> +x,+y  quadrant
//   slice1 [pi/2, pi   ] green  -> -x,+y  quadrant
//   slice2 [pi,   3pi/2] blue   -> -x,-y  quadrant
//   slice3 [3pi/2,2pi  ] yellow -> +x,-y  quadrant
// After the WGSL y-flip, clip +y lands at framebuffer-top.
//
// On this headless box the only Vulkan backend may be lavapipe (software). Force:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTriGradientBackend.hpp"

#include "dc/data/TableStore.hpp"
#include "dc/encode/EncodePass.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// One 13-byte ingest APPEND record (op=1) — the existing wire format.
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(1);  // op = APPEND
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);
  u32(len);
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

static void appendF32(dc::IngestProcessor& ingest, dc::Id buf,
                      const std::vector<float>& vals) {
  std::vector<std::uint8_t> batch;
  appendRecord(batch, buf, vals.data(),
               static_cast<std::uint32_t>(vals.size() * sizeof(float)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

int main() {
  std::printf("=== ENC-613 Dawn polar coords + arc/wedge mark (pie render) ===\n");

  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;
  const float PI = 3.14159265358979323846f;

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

  dc::DawnTriGradientBackend triGradient;
  if (!triGradient.init(dev)) {
    std::fprintf(stderr, "DawnTriGradientBackend::init failed\n");
    return 1;
  }
  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &triGradient);

  // ----- build a (angle,radius,color) pie table + compile the Arc mark --------
  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);
  const dc::Id kTable = 1, kT0 = 10, kR0 = 11, kT1 = 12, kR1 = 13, kCol = 14;
  tables.defineTable(kTable, "pie");
  tables.addColumn(kTable, "t0", dc::DType::F32, kT0);
  tables.addColumn(kTable, "r0", dc::DType::F32, kR0);
  tables.addColumn(kTable, "t1", dc::DType::F32, kT1);
  tables.addColumn(kTable, "r1", dc::DType::F32, kR1);
  tables.addColumn(kTable, "col", dc::DType::I32, kCol);

  const std::uint32_t kRed    = 0xFF0000FFu;
  const std::uint32_t kGreen  = 0xFF00FF00u;
  const std::uint32_t kBlue   = 0xFFFF0000u;
  const std::uint32_t kYellow = 0xFF00FFFFu;

  // Four pie slices from the center (r0=0) out to r1=0.85, each a quadrant.
  appendF32(ingest, kT0, {0.0f,        PI / 2.0f,   PI,           3.0f * PI / 2.0f});
  appendF32(ingest, kR0, {0.0f,        0.0f,        0.0f,         0.0f});
  appendF32(ingest, kT1, {PI / 2.0f,   PI,          3.0f * PI / 2.0f, 2.0f * PI});
  appendF32(ingest, kR1, {0.85f,       0.85f,       0.85f,        0.85f});
  {
    const std::int32_t cols[4] = {static_cast<std::int32_t>(kRed),
                                  static_cast<std::int32_t>(kGreen),
                                  static_cast<std::int32_t>(kBlue),
                                  static_cast<std::int32_t>(kYellow)};
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kCol, cols, sizeof(cols));
    ingest.processBatch(batch.data(),
                        static_cast<std::uint32_t>(batch.size()));
  }

  dc::Encoding enc;
  enc.field(dc::Channel::X, "t0").field(dc::Channel::Y, "r0")
     .field(dc::Channel::X2, "t1").field(dc::Channel::Y2, "r1");
  enc.setColorField("col");

  dc::ArcOptions arcOpts;
  arcOpts.polar.centerX = 0.0f;
  arcOpts.polar.centerY = 0.0f;
  arcOpts.segmentsPerArc = 16;  // smooth-enough arcs

  dc::EncodePass pass;
  auto res = pass.compile(dc::Mark::Arc, enc, tables, kTable, src, 100, 200, 300,
                          nullptr, dc::LineStyle::Line2d, arcOpts);
  if (!res.ok) {
    std::fprintf(stderr, "Arc compile failed: %s\n", res.message.c_str());
    return 1;
  }
  check(res.ok && res.geometry.format == dc::VertexFormat::Pos2Color4,
        "polar/arc: encode pass produced pos2_color4 wedge geometry");

  // ----- wire the compiled geometry into a Scene + render ----------------------
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

  const std::uint32_t vbytes = static_cast<std::uint32_t>(res.bytes.size());
  requireOk(cp.applyJsonText(
      std::string(R"({"cmd":"createBuffer","id":10,"byteLength":)") +
      std::to_string(vbytes) + "}"), "buf");
  store.setCpuData(10, res.bytes.data(), vbytes);
  requireOk(cp.applyJsonText(
      std::string(R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":)") +
      std::to_string(res.geometry.vertexCount) +
      R"(,"format":"pos2_color4"})"), "geom");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triGradient@1","geometryId":100})"),
      "bind");

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f; rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f; rp.clearColor[3] = 1.0f;
  dev.beginRenderPass(rp);
  const dc::DrawItem* di = scene.getDrawItem(3);
  dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);
  dc::BackendStats bs = be->renderDrawItem(dev, scene, store, *di,
                                           static_cast<int>(W),
                                           static_cast<int>(H));
  dev.endRenderPass();
  check(bs.drawCalls >= 1, "polar/arc: at least one draw call issued");

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };

  // Sample a point well inside each quadrant slice, ~0.45 clip units from the
  // center along the slice's mid-angle, mapped clip -> framebuffer pixel (the
  // WGSL negates y, so clip +y -> top of the framebuffer).
  // Mid-angles: slice0 45deg, slice1 135deg, slice2 225deg, slice3 315deg.
  const float rr = 0.45f;
  struct Sample { float ang; const char* name; std::uint8_t exp[3]; };
  Sample samples[4] = {
      {PI * 0.25f, "slice0 RED (+x+y)",    {255, 0,   0}},
      {PI * 0.75f, "slice1 GREEN (-x+y)",  {0,   255, 0}},
      {PI * 1.25f, "slice2 BLUE (-x-y)",   {0,   0,   255}},
      {PI * 1.75f, "slice3 YELLOW (+x-y)", {255, 255, 0}},
  };

  std::uint8_t read[4][4];
  for (int i = 0; i < 4; ++i) {
    const float cx = rr * std::cos(samples[i].ang);
    const float cy = rr * std::sin(samples[i].ang);
    // clip -> framebuffer pixel. The WGSL negates y, so clip +y -> top of fb.
    const std::uint32_t pxX = static_cast<std::uint32_t>((cx + 1.0f) * 0.5f * W);
    const std::uint32_t pxY =
        static_cast<std::uint32_t>((1.0f - (cy + 1.0f) * 0.5f) * H);
    px(pxX, pxY, read[i]);
    std::printf("  %s @ pix(%u,%u) R=%u G=%u B=%u A=%u\n", samples[i].name, pxX,
                pxY, read[i][0], read[i][1], read[i][2], read[i][3]);
  }

  // Each slice shows ITS OWN color at its mid-angle sample (the polar map placed
  // the wedge in the right quadrant — an affine could not).
  auto near = [](std::uint8_t v, std::uint8_t e) {
    return (e > 200) ? v > 200 : v < 64;
  };
  for (int i = 0; i < 4; ++i) {
    bool ok = near(read[i][0], samples[i].exp[0]) &&
              near(read[i][1], samples[i].exp[1]) &&
              near(read[i][2], samples[i].exp[2]);
    check(ok, samples[i].name);
  }

  // NON-BLANK + DISTINCT: the four samples are not all the clear color, and the
  // four colors are mutually distinct.
  auto rgbDiffer = [](const std::uint8_t* a, const std::uint8_t* b) {
    return a[0] != b[0] || a[1] != b[1] || a[2] != b[2];
  };
  bool anyLit = false;
  for (int i = 0; i < 4; ++i)
    if (read[i][0] + read[i][1] + read[i][2] > 32) anyLit = true;
  check(anyLit, "polar/arc: render is NON-BLANK (wedges drew)");
  bool allDistinct = rgbDiffer(read[0], read[1]) && rgbDiffer(read[0], read[2]) &&
                     rgbDiffer(read[0], read[3]) && rgbDiffer(read[1], read[2]) &&
                     rgbDiffer(read[1], read[3]) && rgbDiffer(read[2], read[3]);
  check(allDistinct,
        "polar/arc: 4 slices show 4 DISTINCT colors at distinct angles");

  // The CORNERS (outside r=0.85) must be the BLACK clear — proving the geometry
  // is a disc, not a full-frame fill (the polar radius bound is honored).
  std::uint8_t corner[4];
  px(1, 1, corner);
  check(corner[0] < 16 && corner[1] < 16 && corner[2] < 16,
        "polar/arc: the frame corner (outside the disc) is the clear color");

  std::printf("=== Dawn polar arc: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
