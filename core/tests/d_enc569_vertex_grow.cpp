// ENC-569 — VERTEX-based Dawn backends re-read + re-count when their streaming
// CPU buffer GROWS / is UPDATE_RANGE'd, so streaming line + tessellated geometry
// ANIMATES.
//
// REGRESSION for the gap that left SMA lines / streamgraphs / ridgelines static
// (the ENC-561 spike got stuck here): the vertex-based Dawn backends
// (DawnTriSolidBackend / DawnTriGradientBackend / DawnLine2dBackend /
// DawnLineAABackend) cached their GPU vertex buffer per geometryId on the FIRST
// render AND drew the geometry's STATIC declared vertexCount. So when a
// line2d/triGradient buffer grew (streaming a growing line) or was re-tessellated
// in place, the backend neither re-read the new bytes nor drew the new vertices —
// the view froze on the first frame.
//
// The fix mirrors ENC-558 (instanced backends): stamp the CpuBufferStore version
// the GPU buffer was built from, re-upload on a version bump, AND derive the DRAW
// vertex count from the CURRENT buffer size (bufferBytes / strideOf(format))
// rather than geometry.vertexCount. Static geometry stays a pure cache hit.
//
// This test renders a SHORT line2d (and a SMALL triGradient triangle), then GROWS
// the geometry's vertex buffer (same geometryId) via writeRange + a vertexCount
// bump, renders again, and asserts pixels that were CLEAR in render 1 (where the
// newly-appended geometry now sits) are LIT in render 2. Before the fix, render 2
// == render 1 (frozen first frame) and these fail.
//
// Headless: force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnLine2dBackend.hpp"
#include "dc/gpu/DawnTriGradientBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"

#include <cstdint>
#include <cstdio>

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
  if (cond) { std::printf("  PASS: %s\n", name); ++passed; }
  else      { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

int main() {
  std::printf("=== ENC-569 vertex backend buffer GROWTH re-read + re-count ===\n");

  constexpr std::uint32_t W = 128;
  constexpr std::uint32_t H = 128;

  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json to force lavapipe.\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  auto px = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* o) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y), o);
  };
  auto lit = [](const std::uint8_t* p) {
    return p[0] > 80 || p[1] > 80 || p[2] > 80;
  };
  // A 1px line (line2d / LineList, no width control) rasterizes onto exactly one
  // row that may differ from the geometric center by a pixel; scan a small column
  // around (x, yCenter) so the thin line is reliably sampled.
  auto litCol = [&](std::uint32_t x, std::uint32_t yCenter) {
    for (int dy = -2; dy <= 2; ++dy) {
      const int y = static_cast<int>(yCenter) + dy;
      if (y < 0 || y >= static_cast<int>(H)) continue;
      std::uint8_t p[4];
      px(x, static_cast<std::uint32_t>(y), p);
      if (lit(p)) return true;
    }
    return false;
  };

  // clip x in [-1,1] maps to pixel x in [0,W]; clip y is negated in the shaders
  // (top-left framebuffer) so clip y in [-1,1] maps top->bottom as y grows.
  auto clipToPx = [&](float cx, float cy, std::uint32_t& ox, std::uint32_t& oy) {
    ox = static_cast<std::uint32_t>((cx * 0.5f + 0.5f) * W);
    oy = static_cast<std::uint32_t>((cy * 0.5f + 0.5f) * H);  // shader flips y
  };

  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = rp.clearColor[1] = rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;

  // ---------------------------------------------------------------- LINE2D ---
  // Render 1: a single short horizontal segment on the LEFT half (2 verts).
  // Grow: append a second segment crossing the RIGHT half. The right-half pixels
  // start CLEAR and must light up after the grow.
  {
    dc::DawnLine2dBackend line;
    if (!line.init(dev)) { std::fprintf(stderr, "line init failed\n"); return 1; }
    dc::BackendRegistry backends;
    backends.registerBackend(dc::DeviceKind::Dawn, &line);

    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // pos2_clip (8B/vertex). One segment on the left half at clip y = 0.
    float v0[] = { -0.8f, 0.0f,  -0.2f, 0.0f };  // 2 verts = 1 segment
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":16})"), "buf");
    store.setCpuData(10, v0, sizeof(v0));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":2,"format":"pos2_clip"})"), "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"line2d@1","geometryId":100})"), "bind");
    requireOk(cp.applyJsonText(
        R"({"cmd":"setDrawItemStyle","drawItemId":3,"r":1,"g":1,"b":1,"a":1})"), "style");

    const dc::DrawItem* di = scene.getDrawItem(3);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);

    // Sample points: left-half segment midpoint, right-half (future) midpoint.
    std::uint32_t lx, ly, rx, ry;
    clipToPx(-0.5f, 0.0f, lx, ly);
    clipToPx(0.5f, 0.0f, rx, ry);

    // --- Render 1: one segment (left only). ---
    dev.beginRenderPass(rp);
    dc::BackendStats bs1 = be->renderDrawItem(dev, scene, store, *di, (int)W, (int)H);
    dev.endRenderPass();
    check(bs1.verticesSubmitted == 2, "line render1: 2 vertices submitted");
    check(litCol(lx, ly), "line render1: left segment lit");
    check(!litCol(rx, ry), "line render1: right half clear");

    // --- GROW: append a second segment on the right half (streaming tail-append
    // via writeRange + the vertexCount bump the app issues). SAME geometryId. ---
    float v1[] = { 0.2f, 0.0f,  0.8f, 0.0f };  // 2 more verts = 2nd segment
    store.writeRange(10, sizeof(v0), v1, sizeof(v1));
    requireOk(cp.applyJsonText(
        R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":4})"), "grow");

    // --- Render 2: must now draw BOTH segments (4 vertices). ---
    const dc::DrawItem* di2 = scene.getDrawItem(3);
    dev.beginRenderPass(rp);
    dc::BackendStats bs2 = be->renderDrawItem(dev, scene, store, *di2, (int)W, (int)H);
    dev.endRenderPass();
    std::printf("  line r2 verts=%u\n", bs2.verticesSubmitted);
    check(bs2.verticesSubmitted == 4, "line render2: 4 vertices (buffer grew -> re-counted)");
    check(litCol(lx, ly), "line render2: left segment still lit");
    check(litCol(rx, ry), "line render2: right segment now lit (buffer grew -> re-uploaded)");
  }

  // ------------------------------------------------------------ TRIGRADIENT ---
  // Render 1: one filled triangle on the LEFT half (3 verts, pos2_color4 = 24B).
  // Grow: append a second triangle on the RIGHT half. Right pixels start clear.
  {
    dc::DawnTriGradientBackend grad;
    if (!grad.init(dev)) { std::fprintf(stderr, "grad init failed\n"); return 1; }
    dc::BackendRegistry backends;
    backends.registerBackend(dc::DeviceKind::Dawn, &grad);

    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);
    dc::CpuBufferStore store;

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di");

    // pos2_color4 vertex = (x, y, r, g, b, a). One green triangle on the left half.
    float t0[] = {
      -0.8f, -0.4f, 0.0f, 1.0f, 0.0f, 1.0f,
      -0.2f, -0.4f, 0.0f, 1.0f, 0.0f, 1.0f,
      -0.5f,  0.4f, 0.0f, 1.0f, 0.0f, 1.0f,
    };
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":72})"), "buf");
    store.setCpuData(10, t0, sizeof(t0));
    requireOk(cp.applyJsonText(
        R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_color4"})"), "geom");
    requireOk(cp.applyJsonText(
        R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triGradient@1","geometryId":100})"), "bind");

    const dc::DrawItem* di = scene.getDrawItem(3);
    dc::IRendererBackend* be = backends.find(dc::DeviceKind::Dawn, di->pipeline);

    std::uint32_t lx, ly, rx, ry;
    clipToPx(-0.5f, 0.0f, lx, ly);
    clipToPx(0.5f, 0.0f, rx, ry);

    dev.beginRenderPass(rp);
    dc::BackendStats bs1 = be->renderDrawItem(dev, scene, store, *di, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r1L[4], r1R[4];
    px(lx, ly, r1L);
    px(rx, ry, r1R);
    check(bs1.verticesSubmitted == 3, "grad render1: 3 vertices submitted");
    check(lit(r1L), "grad render1: left triangle lit");
    check(!lit(r1R), "grad render1: right half clear");

    // GROW: append a second triangle on the right half. SAME geometryId.
    float t1[] = {
       0.2f, -0.4f, 0.0f, 0.0f, 1.0f, 1.0f,
       0.8f, -0.4f, 0.0f, 0.0f, 1.0f, 1.0f,
       0.5f,  0.4f, 0.0f, 0.0f, 1.0f, 1.0f,
    };
    store.writeRange(10, sizeof(t0), t1, sizeof(t1));
    requireOk(cp.applyJsonText(
        R"({"cmd":"setGeometryVertexCount","geometryId":100,"vertexCount":6})"), "grow");

    const dc::DrawItem* di2 = scene.getDrawItem(3);
    dev.beginRenderPass(rp);
    dc::BackendStats bs2 = be->renderDrawItem(dev, scene, store, *di2, (int)W, (int)H);
    dev.endRenderPass();
    std::uint8_t r2L[4], r2R[4];
    px(lx, ly, r2L);
    px(rx, ry, r2R);
    std::printf("  grad r2 L=%u,%u,%u  R=%u,%u,%u  verts=%u\n",
                r2L[0], r2L[1], r2L[2], r2R[0], r2R[1], r2R[2],
                bs2.verticesSubmitted);
    check(bs2.verticesSubmitted == 6, "grad render2: 6 vertices (buffer grew -> re-counted)");
    check(lit(r2L), "grad render2: left triangle still lit");
    check(lit(r2R), "grad render2: right triangle now lit (buffer grew -> re-uploaded)");
  }

  std::printf("=== ENC-569 vertex growth: %d passed, %d failed ===\n",
              passed, failed);
  return failed > 0 ? 1 : 0;
}
