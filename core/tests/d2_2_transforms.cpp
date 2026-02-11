#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>

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
    return 0;
  }

  // 2. Build scene
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);

  requireOk(cp.applyJsonText(R"({"cmd":"createPane","name":"P1"})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L1"})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":24})"), "createBuffer");
  requireOk(cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})"),
    "createGeometry");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Tri"})"), "createDrawItem");
  requireOk(cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":4})"),
    "bindDrawItem");

  // 3. Create a transform: scale 0.25, translate (0.5, 0.5)
  requireOk(cp.applyJsonText(R"({"cmd":"createTransform"})"), "createTransform");
  // Transform gets auto-id 6
  requireOk(cp.applyJsonText(
    R"({"cmd":"setTransform","id":6,"sx":0.25,"sy":0.25,"tx":0.5,"ty":0.5})"),
    "setTransform");
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":5,"transformId":6})"),
    "attachTransform");

  // 4. Supply triangle vertex data
  float verts[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
  };
  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(3, verts, sizeof(verts));

  // 5. Init renderer and render
  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  gpuBufs.uploadDirty();

  dc::Stats stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();

  requireTrue(stats.drawCalls == 1, "drawCalls == 1");

  // 6. Read back pixels
  auto pixels = ctx.readPixels();
  auto px = [&](int x, int y) -> const std::uint8_t* {
    return &pixels[static_cast<std::size_t>((y * W + x) * 4)];
  };

  // With transform: scale 0.25 + translate (0.5, 0.5),
  // triangle centroid moves to clip (0.5, 0.5).
  // In pixel coords: ((0.5+1)/2)*64 = 48
  int offsetX = 48;
  int offsetY = 48;

  const auto* shifted = px(offsetX, offsetY);
  std::printf("Shifted pixel (%d,%d): R=%u G=%u B=%u A=%u\n",
              offsetX, offsetY, shifted[0], shifted[1], shifted[2], shifted[3]);
  requireTrue(shifted[0] > 200, "shifted pixel is red");

  // Old center (32, 32) should be black since triangle moved away
  const auto* oldCenter = px(W / 2, H / 2);
  std::printf("Old center pixel: R=%u G=%u B=%u A=%u\n",
              oldCenter[0], oldCenter[1], oldCenter[2], oldCenter[3]);
  requireTrue(oldCenter[0] < 10, "old center is black");

  // 7. Detach transform → renders back to center
  requireOk(cp.applyJsonText(
    R"({"cmd":"attachTransform","drawItemId":5,"transformId":0})"),
    "detachTransform");

  stats = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  pixels = ctx.readPixels();

  const auto* centerAgain = px(W / 2, H / 2);
  std::printf("Center after detach: R=%u G=%u B=%u A=%u\n",
              centerAgain[0], centerAgain[1], centerAgain[2], centerAgain[3]);
  requireTrue(centerAgain[0] > 200, "center is red after detach");

  std::printf("\nD2.2 transforms PASS\n");
  return 0;
}
