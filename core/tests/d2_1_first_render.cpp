#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  constexpr int W = 64;
  constexpr int H = 64;

  // 1. Create OSMesa context
  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0; // graceful skip
  }
  std::printf("OSMesa %dx%d context created\n", W, H);

  // 2. Build scene via CommandProcessor
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  // pane (auto-id 1)
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P1"})"), "createPane");
  // layer (auto-id 2)
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L1"})"), "createLayer");
  // buffer (auto-id 3)
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":24})"), "createBuffer");
  // geometry (auto-id 4) — 3 vertices, pos2_clip
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})"),
    "createGeometry");
  // drawItem (auto-id 5)
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Tri"})"), "createDrawItem");
  // bind drawItem to pipeline + geometry
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":4})"),
    "bindDrawItem");
  // set color to red (was hardcoded in renderer before per-DrawItem color)
  requireOk(cp.applyJsonText(
    R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1.0,"g":0.0,"b":0.0,"a":1.0})"),
    "setDrawItemColor");

  std::printf("Scene built: pane=1, layer=2, buffer=3, geom=4, drawItem=5\n");

  // 3. Supply triangle vertex data via GpuBufferManager
  //    Triangle: (-0.5,-0.5), (0.5,-0.5), (0.0,0.5) in clip space
  float verts[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
  };
  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(3, verts, sizeof(verts));

  // 4. Init renderer and render
  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");

  std::uint64_t uploaded = gpuBufs.uploadDirty();
  std::printf("Uploaded %llu bytes\n", (unsigned long long)uploaded);

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  std::printf("Draw calls: %u\n", stats.drawCalls);

  // 5. Read back pixels
  auto pixels = ctx.readPixels();
  requireTrue(pixels.size() == static_cast<std::size_t>(W * H * 4),
              "pixel buffer size");

  // Helper: get RGBA at (x, y). Origin is bottom-left.
  auto px = [&](int x, int y) -> const std::uint8_t* {
    return &pixels[static_cast<std::size_t>((y * W + x) * 4)];
  };

  // Center pixel should be red (the triangle covers center).
  const auto* center = px(W / 2, H / 2);
  std::printf("Center pixel: R=%u G=%u B=%u A=%u\n",
              center[0], center[1], center[2], center[3]);
  requireTrue(center[0] > 200, "center R > 200");
  requireTrue(center[1] < 10,  "center G < 10");
  requireTrue(center[2] < 10,  "center B < 10");

  // Corner pixel (0,0) should be black (background).
  const auto* corner = px(0, 0);
  std::printf("Corner pixel: R=%u G=%u B=%u A=%u\n",
              corner[0], corner[1], corner[2], corner[3]);
  requireTrue(corner[0] < 10, "corner R < 10");
  requireTrue(corner[1] < 10, "corner G < 10");
  requireTrue(corner[2] < 10, "corner B < 10");

  // Draw call count
  requireTrue(stats.drawCalls == 1, "drawCalls == 1");

  std::printf("\nD2.1 first render PASS\n");
  return 0;
}
