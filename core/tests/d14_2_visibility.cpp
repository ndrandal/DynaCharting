// D14.2 — DrawItem visibility test (GL)
// Verifies that setDrawItemVisible command works and Renderer skips invisible items.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

static void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n",
                 ctx, r.err.code.c_str(), r.err.message.c_str());
    std::exit(1);
  }
}

int main() {
  constexpr int W = 100, H = 100;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::printf("D14.2 visibility: SKIPPED (no OSMesa)\n");
    return 0;
  }

  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;
  cp.setIngestProcessor(&ingest);

  // Setup: pane + layer + triangle draw item
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":100,"byteLength":0})"), "createBuf");
  requireOk(cp.applyJsonText(R"({"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"pos2_clip","vertexCount":3})"), "createGeom");
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":102,"layerId":10,"name":"tri"})"), "createDI");
  requireOk(cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":102,"pipeline":"triSolid@1","geometryId":101})"), "bind");
  requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":102,"r":1,"g":0,"b":0,"a":1})"), "setColor");

  // Triangle vertices
  float verts[] = {0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f};
  ingest.ensureBuffer(100);
  ingest.setBufferData(100, reinterpret_cast<const std::uint8_t*>(verts), sizeof(verts));

  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(100, ingest.getBufferData(100), ingest.getBufferSize(100));

  dc::Renderer renderer;
  requireTrue(renderer.init(), "Renderer::init");
  gpuBufs.uploadDirty();

  // ---- Render with visible=true (default) ----
  auto stats1 = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  requireTrue(stats1.drawCalls == 1, "1 draw call when visible");

  auto pixels1 = ctx.readPixels();
  int red1 = 0;
  for (int i = 0; i < W * H; i++) {
    if (pixels1[i * 4] > 200 && pixels1[i * 4 + 1] < 50) red1++;
  }
  requireTrue(red1 > 100, "red pixels when visible");
  std::printf("  visible: %d draw calls, %d red pixels\n", stats1.drawCalls, red1);

  // ---- Set invisible ----
  requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":102,"visible":false})"), "setInvisible");
  const auto* di = scene.getDrawItem(102);
  requireTrue(di && !di->visible, "DrawItem.visible == false");

  auto stats2 = renderer.render(scene, gpuBufs, W, H);
  ctx.swapBuffers();
  requireTrue(stats2.drawCalls == 0, "0 draw calls when invisible");

  auto pixels2 = ctx.readPixels();
  int red2 = 0;
  for (int i = 0; i < W * H; i++) {
    if (pixels2[i * 4] > 200 && pixels2[i * 4 + 1] < 50) red2++;
  }
  requireTrue(red2 == 0, "no red pixels when invisible");
  std::printf("  invisible: %d draw calls, %d red pixels\n", stats2.drawCalls, red2);

  // ---- Set visible again ----
  requireOk(cp.applyJsonText(R"({"cmd":"setDrawItemVisible","drawItemId":102,"visible":true})"), "setVisible");
  auto stats3 = renderer.render(scene, gpuBufs, W, H);
  requireTrue(stats3.drawCalls == 1, "1 draw call when re-visible");
  std::printf("  re-visible: %d draw calls\n", stats3.drawCalls);

  std::printf("D14.2 visibility: ALL PASS\n");
  return 0;
}
