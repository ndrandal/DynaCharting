// ENC-486 (P2.3) — line2d@1 + points@1 end-to-end on Dawn.
//
// The Dawn counterpart of the GL line2d/points paths (Renderer::drawPos2 with
// GL_LINES / GL_POINTS). It renders a line2d scene and a points scene through
// the backend registry with DeviceKind::Dawn into the headless DawnDevice
// offscreen RGBA8 target, reads the result back, and asserts the drawn pixels
// appear where expected and a clear region stays the clear color.
//
// SCENES (clip space, no transform / identity):
//   line2d : a horizontal red line from (-0.7, 0.0) to (0.7, 0.0) — a 2-vertex
//            LineList. After the WGSL NDC y-flip, y=0 stays on the framebuffer
//            center row, so the lit pixels run along the middle scanline.
//   points : two green points at (-0.4, 0.0) and (0.4, 0.0) — a 2-vertex
//            PointList. After the y-flip they land on the center row, left and
//            right of center. WebGPU points are 1px.
//
// 1px NOTE: WebGPU LineList/PointList have NO width/size control. di.lineWidth /
// di.pointSize are ignored by the backends (TODO(ENC-490) quad-expansion). So we
// assert single-pixel-thin coverage: the exact lit row/columns, and clear pixels
// a few rows away.
//
// ORIENTATION cross-check: the GL baseline (d2_3_pipelines.cpp) and the pos2
// WGSL both put clip y=0 on the framebuffer's center row (the y-flip is a no-op
// at y=0 and symmetric about it), so the line/points land in the SAME screen row
// as the GL baseline convention.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnLine2dBackend.hpp"
#include "dc/gpu/DawnPointsBackend.hpp"

#include "dc/render/BackendRegistry.hpp"
#include "dc/render/IRendererBackend.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/GpuBufferManager.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx,
                 r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

namespace {

constexpr std::uint32_t W = 64;
constexpr std::uint32_t H = 64;

// Render a single DrawItem through the registry into a freshly-cleared offscreen
// target and return the draw-call count.
std::uint32_t renderOne(dc::DawnDevice& dev, dc::BackendRegistry& backends,
                        dc::Scene& scene, dc::GpuBufferManager& gpuBufs,
                        dc::Id drawItemId) {
  dc::RenderPassDesc rp;
  rp.target = {};
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  dev.beginRenderPass(rp);

  std::uint32_t drawCalls = 0;
  const dc::DrawItem* di = scene.getDrawItem(drawItemId);
  requireTrue(di != nullptr, "drawItem exists");
  if (dc::IRendererBackend* be =
          backends.find(dc::DeviceKind::Dawn, di->pipeline)) {
    dc::BackendStats bs = be->renderDrawItem(dev, scene, gpuBufs, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    drawCalls += bs.drawCalls;
  } else {
    requireTrue(false, "no Dawn backend registered for pipeline");
  }
  dev.endRenderPass();
  return drawCalls;
}

bool isColor(const std::uint8_t* p, std::uint8_t r, std::uint8_t g,
             std::uint8_t b) {
  auto near = [](std::uint8_t v, std::uint8_t t) {
    return (v >= t ? v - t : t - v) < 24;
  };
  return near(p[0], r) && near(p[1], g) && near(p[2], b);
}

// True if any pixel in column x within +/-1 of row y matches color (robust to
// 1px rasterization landing on the adjacent row).
bool anyLitNear(dc::DawnDevice& dev, std::uint32_t x, std::uint32_t y,
                std::uint8_t r, std::uint8_t g, std::uint8_t b) {
  for (int dy = -1; dy <= 1; ++dy) {
    std::uint8_t px[4] = {0, 0, 0, 0};
    dev.readPixel(static_cast<std::int32_t>(x),
                  static_cast<std::int32_t>(static_cast<int>(y) + dy), px);
    if (isColor(px, r, g, b)) return true;
  }
  return false;
}

}  // namespace

int main() {
  // --- 1. Bring up the headless Dawn device. ------------------------------
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

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::GpuBufferManager gpuBufs;

  // --- 2. Register the Dawn line2d + points backends. ---------------------
  dc::DawnLine2dBackend line2d;
  dc::DawnPointsBackend points;
  requireTrue(line2d.init(dev), "DawnLine2dBackend::init (pipeline create)");
  requireTrue(points.init(dev), "DawnPointsBackend::init (pipeline create)");

  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &line2d);
  backends.registerBackend(dc::DeviceKind::Dawn, &points);

  // ====================================================================
  // SCENE A — line2d@1: horizontal red line at clip y=0.
  // ====================================================================
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P1"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L1"})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "createBuffer(line)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":2})"),
      "createGeometry(line)");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Line"})"),
            "createDrawItem(line)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"line2d@1","geometryId":4})"),
      "bindDrawItem(line)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1.0,"g":0.0,"b":0.0,"a":1.0})"),
      "setDrawItemColor(line)");

  float lineVerts[] = {-0.7f, 0.0f, 0.7f, 0.0f};  // 2 vertices, one LineList seg
  gpuBufs.setCpuData(3, lineVerts, sizeof(lineVerts));

  std::uint32_t lineDraws = renderOne(dev, backends, scene, gpuBufs, 5);
  std::printf("line2d draw calls: %u\n", lineDraws);
  requireTrue(lineDraws == 1, "line2d drawCalls == 1");

  // The line spans the center row (y=H/2). Sample lit pixels along it and a
  // clear pixel several rows away.
  requireTrue(anyLitNear(dev, W / 2, H / 2, 255, 0, 0),
              "line2d: center of line is lit red");
  requireTrue(anyLitNear(dev, W / 2 - 16, H / 2, 255, 0, 0),
              "line2d: left of line is lit red");
  requireTrue(anyLitNear(dev, W / 2 + 16, H / 2, 255, 0, 0),
              "line2d: right of line is lit red");

  std::uint8_t lineClear[4] = {9, 9, 9, 9};
  dev.readPixel(static_cast<std::int32_t>(W / 2),
                static_cast<std::int32_t>(H / 2 - 12), lineClear);
  std::printf("line2d clear pixel (12px above): R=%u G=%u B=%u A=%u\n",
              lineClear[0], lineClear[1], lineClear[2], lineClear[3]);
  requireTrue(isColor(lineClear, 0, 0, 0), "line2d: pixel away from line is clear (black)");

  std::printf("line2d@1 PASS (1px LineList, lit center row, clear elsewhere)\n");

  // ====================================================================
  // SCENE B — points@1: two green points at clip y=0, x=-0.4 and x=0.4.
  // ====================================================================
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "createBuffer(pts)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","vertexBufferId":6,"format":"pos2_clip","vertexCount":2})"),
      "createGeometry(pts)");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Pts"})"),
            "createDrawItem(pts)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":8,"pipeline":"points@1","geometryId":7})"),
      "bindDrawItem(pts)");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":8,"r":0.0,"g":1.0,"b":0.0,"a":1.0})"),
      "setDrawItemColor(pts)");

  float ptVerts[] = {-0.4f, 0.0f, 0.4f, 0.0f};  // 2 vertices, PointList
  gpuBufs.setCpuData(6, ptVerts, sizeof(ptVerts));

  std::uint32_t ptDraws = renderOne(dev, backends, scene, gpuBufs, 8);
  std::printf("points draw calls: %u\n", ptDraws);
  requireTrue(ptDraws == 1, "points drawCalls == 1");

  // Map clip x -> framebuffer column: x_fb = (x_clip*0.5 + 0.5)*W.
  // x=-0.4 -> 0.3*W ~= 19 ; x=0.4 -> 0.7*W ~= 44 ; both on center row.
  const std::uint32_t leftX = static_cast<std::uint32_t>((-0.4f * 0.5f + 0.5f) * W);
  const std::uint32_t rightX = static_cast<std::uint32_t>((0.4f * 0.5f + 0.5f) * W);
  std::printf("points expected cols: left=%u right=%u (row=%u)\n",
              leftX, rightX, H / 2);

  // Each point is 1px; check it (and the immediate neighbor column, robust to
  // rounding) is lit green on the center row.
  bool leftLit = anyLitNear(dev, leftX, H / 2, 0, 255, 0) ||
                 anyLitNear(dev, leftX + 1, H / 2, 0, 255, 0) ||
                 anyLitNear(dev, leftX - 1, H / 2, 0, 255, 0);
  bool rightLit = anyLitNear(dev, rightX, H / 2, 0, 255, 0) ||
                  anyLitNear(dev, rightX + 1, H / 2, 0, 255, 0) ||
                  anyLitNear(dev, rightX - 1, H / 2, 0, 255, 0);
  requireTrue(leftLit, "points: left point is lit green");
  requireTrue(rightLit, "points: right point is lit green");

  // Between the two points (center column) should be clear — confirms these are
  // discrete 1px points, not a line.
  std::uint8_t ptClear[4] = {9, 9, 9, 9};
  dev.readPixel(static_cast<std::int32_t>(W / 2),
                static_cast<std::int32_t>(H / 2), ptClear);
  std::printf("points clear pixel (center, between points): R=%u G=%u B=%u A=%u\n",
              ptClear[0], ptClear[1], ptClear[2], ptClear[3]);
  requireTrue(isColor(ptClear, 0, 0, 0),
              "points: gap between points is clear (black) — discrete 1px points");

  std::printf("points@1 PASS (1px PointList, two lit points, clear gap)\n");

  std::printf("\nD2.3 Dawn line2d@1 + points@1 PASS (on %s)\n",
              dev.backendName().c_str());
  return 0;
}
