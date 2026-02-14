// D10.5 — Frustum Culling test (OSMesa)
// Tests: visible/partially visible/off-screen items, stats.culledDrawCalls.

#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

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
  constexpr int W = 200;
  constexpr int H = 200;

  dc::OsMesaContext ctx;
  if (!ctx.init(W, H)) {
    std::fprintf(stderr, "Could not init OSMesa — skipping test\n");
    return 0;
  }

  // --- Test 1: Three DrawItems — visible, partial, off-screen ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");

    // Transform: data range [0,100] maps to clip [-1,1]
    // sx = 2/100 = 0.02, tx = -1
    requireOk(cp.applyJsonText(
      R"({"cmd":"setTransform","id":50,"sx":0.02,"sy":0.02,"tx":-1.0,"ty":-1.0})"),
      "setTransform");

    // DrawItem A: visible (bounds in data space [20,30] to [40,50])
    // Clip: minX = 0.02*20-1 = -0.6, maxX = 0.02*40-1 = -0.2 → inside [-1,1]
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBufA");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeoA");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryBounds","geometryId":30,"minX":20,"minY":30,"maxX":40,"maxY":50})"),
      "setBoundsA");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI_A");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"), "bindA");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachA");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":40,"r":1.0,"g":0.0,"b":0.0})"), "colorA");

    // DrawItem B: partially visible (bounds extend beyond clip but overlap)
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":21,"byteLength":24})"), "createBufB");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":31,"vertexBufferId":21,"format":"pos2_clip","vertexCount":3})"),
      "createGeoB");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryBounds","geometryId":31,"minX":80,"minY":80,"maxX":120,"maxY":120})"),
      "setBoundsB");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":41,"layerId":10})"), "createDI_B");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":41,"pipeline":"triSolid@1","geometryId":31})"), "bindB");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":41,"transformId":50})"), "attachB");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":41,"r":0.0,"g":1.0,"b":0.0})"), "colorB");

    // DrawItem C: fully off-screen (bounds [200,200] to [300,300])
    // Clip: minX = 0.02*200-1 = 3.0 → fully right of clipXMax=1.0
    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":22,"byteLength":24})"), "createBufC");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":32,"vertexBufferId":22,"format":"pos2_clip","vertexCount":3})"),
      "createGeoC");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryBounds","geometryId":32,"minX":200,"minY":200,"maxX":300,"maxY":300})"),
      "setBoundsC");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":42,"layerId":10})"), "createDI_C");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":42,"pipeline":"triSolid@1","geometryId":32})"), "bindC");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":42,"transformId":50})"), "attachC");

    // Vertex data (same simple triangle for all)
    float verts[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };

    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));
    gpuBufs.setCpuData(21, verts, sizeof(verts));
    gpuBufs.setCpuData(22, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    ctx.swapBuffers();

    std::printf("  drawCalls=%u, culledDrawCalls=%u\n", stats.drawCalls, stats.culledDrawCalls);
    requireTrue(stats.drawCalls == 2, "2 draw calls (A + B visible/partial)");
    requireTrue(stats.culledDrawCalls == 1, "1 culled (C off-screen)");

    std::printf("  Test 1 (visible/partial/off-screen) PASS\n");
  }

  // --- Test 2: Pan viewport to bring off-screen item visible ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");

    // Transform centered on data [200, 300]: sx=0.02, tx = -(200+300)/2 * 0.02 = -5
    // Actually, let's just set a transform that makes [200,300] visible in clip
    // Map [150,350] → [-1,1]: sx = 2/200 = 0.01, tx = -(150+350)/2 * 0.01 = -2.5
    // clip_x = 0.01 * data_x - 2.5
    // At data_x=200: clip_x = 0.01*200 - 2.5 = -0.5 → visible
    // At data_x=300: clip_x = 0.01*300 - 2.5 = 0.5 → visible
    requireOk(cp.applyJsonText(
      R"({"cmd":"setTransform","id":50,"sx":0.01,"sy":0.01,"tx":-2.5,"ty":-2.5})"),
      "setTransform panned");

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBuf");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeo");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setGeometryBounds","geometryId":30,"minX":200,"minY":200,"maxX":300,"maxY":300})"),
      "setBounds");
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"), "bindDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachXf");

    float verts[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };
    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    std::printf("  drawCalls=%u, culledDrawCalls=%u\n", stats.drawCalls, stats.culledDrawCalls);
    requireTrue(stats.culledDrawCalls == 0, "0 culled after pan");
    requireTrue(stats.drawCalls == 1, "1 draw call");

    std::printf("  Test 2 (pan brings item visible → 0 culled) PASS\n");
  }

  // --- Test 3: DrawItem without bounds → always drawn (no culling) ---
  {
    dc::Scene scene;
    dc::ResourceRegistry reg;
    dc::CommandProcessor cp(scene, reg);

    requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1})"), "createPane");
    requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":10,"paneId":1})"), "createLayer");
    requireOk(cp.applyJsonText(R"({"cmd":"createTransform","id":50})"), "createTransform");
    requireOk(cp.applyJsonText(
      R"({"cmd":"setTransform","id":50,"sx":0.02,"sy":0.02,"tx":-1.0,"ty":-1.0})"),
      "setTransform");

    requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":20,"byteLength":24})"), "createBuf");
    requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":30,"vertexBufferId":20,"format":"pos2_clip","vertexCount":3})"),
      "createGeo");
    // NOT setting bounds
    requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":40,"layerId":10})"), "createDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":40,"pipeline":"triSolid@1","geometryId":30})"), "bindDI");
    requireOk(cp.applyJsonText(
      R"({"cmd":"attachTransform","drawItemId":40,"transformId":50})"), "attachXf");

    float verts[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f };
    dc::GpuBufferManager gpuBufs;
    gpuBufs.setCpuData(20, verts, sizeof(verts));

    dc::Renderer renderer;
    requireTrue(renderer.init(), "Renderer::init");
    gpuBufs.uploadDirty();

    auto stats = renderer.render(scene, gpuBufs, W, H);
    requireTrue(stats.culledDrawCalls == 0, "no culling without bounds");
    requireTrue(stats.drawCalls == 1, "1 draw call");

    std::printf("  Test 3 (no bounds → always drawn) PASS\n");
  }

  std::printf("D10.5 frustum_cull: ALL PASS\n");
  return 0;
}
