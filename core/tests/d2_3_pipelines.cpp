#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/gl/OsMesaContext.hpp"
#include "dc/gl/GpuBufferManager.hpp"
#include "dc/gl/Renderer.hpp"

#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>

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

struct TestCtx {
  dc::OsMesaContext glCtx;
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp;
  dc::GpuBufferManager gpuBufs;
  dc::Renderer renderer;

  TestCtx() : cp(scene, reg) {}

  bool setup() {
    if (!glCtx.init(64, 64)) return false;
    return renderer.init();
  }

  dc::Stats renderAndRead(std::vector<std::uint8_t>& pixels) {
    gpuBufs.uploadDirty();
    dc::Stats stats = renderer.render(scene, gpuBufs, 64, 64);
    glCtx.swapBuffers();
    pixels = glCtx.readPixels();
    return stats;
  }

  const std::uint8_t* px(const std::vector<std::uint8_t>& pixels, int x, int y) {
    return &pixels[static_cast<std::size_t>((y * 64 + x) * 4)];
  }

  bool isNonBlack(const std::uint8_t* p) {
    return p[0] > 10 || p[1] > 10 || p[2] > 10;
  }
};

static void testLine2d() {
  std::printf("  sub-test: line2d@1\n");
  TestCtx t;
  if (!t.setup()) { std::printf("    skipped (no OSMesa)\n"); return; }

  requireOk(t.cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "buf");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":2})"), "geom");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Line"})"), "di");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"line2d@1","geometryId":4})"), "bind");

  // Horizontal line through center
  float verts[] = { -1.0f, 0.0f,  1.0f, 0.0f };
  t.gpuBufs.setCpuData(3, verts, sizeof(verts));

  std::vector<std::uint8_t> pixels;
  dc::Stats stats = t.renderAndRead(pixels);

  requireTrue(stats.drawCalls == 1, "line drawCalls==1");
  // Line at y=0 maps to pixel row 32; rasterization may hit row 31 or 32
  bool lineFound = t.isNonBlack(t.px(pixels, 32, 32)) ||
                   t.isNonBlack(t.px(pixels, 32, 31));
  requireTrue(lineFound, "line near-center pixel non-black");
  std::printf("    PASS\n");
}

static void testPoints() {
  std::printf("  sub-test: points@1\n");
  TestCtx t;
  if (!t.setup()) { std::printf("    skipped (no OSMesa)\n"); return; }

  requireOk(t.cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":8})"), "buf");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":1})"), "geom");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Pt"})"), "di");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"points@1","geometryId":4})"), "bind");

  // Point at origin
  float verts[] = { 0.0f, 0.0f };
  t.gpuBufs.setCpuData(3, verts, sizeof(verts));

  std::vector<std::uint8_t> pixels;
  dc::Stats stats = t.renderAndRead(pixels);

  requireTrue(stats.drawCalls == 1, "points drawCalls==1");
  requireTrue(t.isNonBlack(t.px(pixels, 32, 32)), "point center pixel non-black");
  std::printf("    PASS\n");
}

static void testInstancedRect() {
  std::printf("  sub-test: instancedRect@1\n");
  TestCtx t;
  if (!t.setup()) { std::printf("    skipped (no OSMesa)\n"); return; }

  requireOk(t.cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "buf");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"rect4","vertexCount":1})"), "geom");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Rect"})"), "di");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"instancedRect@1","geometryId":4})"), "bind");

  // Rect covering center: (-0.5, -0.5) to (0.5, 0.5)
  float rect[] = { -0.5f, -0.5f, 0.5f, 0.5f };
  t.gpuBufs.setCpuData(3, rect, sizeof(rect));

  std::vector<std::uint8_t> pixels;
  dc::Stats stats = t.renderAndRead(pixels);

  requireTrue(stats.drawCalls == 1, "rect drawCalls==1");
  requireTrue(t.isNonBlack(t.px(pixels, 32, 32)), "rect center pixel colored");
  std::printf("    PASS\n");
}

static void testInstancedCandle() {
  std::printf("  sub-test: instancedCandle@1\n");
  TestCtx t;
  if (!t.setup()) { std::printf("    skipped (no OSMesa)\n"); return; }

  requireOk(t.cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":24})"), "buf");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"candle6","vertexCount":1})"), "geom");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Candle"})"), "di");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"instancedCandle@1","geometryId":4})"), "bind");

  // Candle at center: x=0, open=-0.3, high=0.5, low=-0.5, close=0.3, halfWidth=0.3
  float candle[] = { 0.0f, -0.3f, 0.5f, -0.5f, 0.3f, 0.3f };
  t.gpuBufs.setCpuData(3, candle, sizeof(candle));

  std::vector<std::uint8_t> pixels;
  dc::Stats stats = t.renderAndRead(pixels);

  requireTrue(stats.drawCalls == 1, "candle drawCalls==1");
  // Body covers center
  requireTrue(t.isNonBlack(t.px(pixels, 32, 32)), "candle body pixel non-black");
  std::printf("    PASS\n");
}

static void testMultiPipeline() {
  std::printf("  sub-test: multi-pipeline scene\n");
  TestCtx t;
  if (!t.setup()) { std::printf("    skipped (no OSMesa)\n"); return; }

  requireOk(t.cp.applyJsonText(R"({"cmd":"createPane","name":"P"})"), "pane");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createLayer","paneId":1,"name":"L"})"), "layer");

  // Buffer for triangle (id=3)
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":24})"), "buf1");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":3,"format":"pos2_clip","vertexCount":3})"), "geom1");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Tri"})"), "di1");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"triSolid@1","geometryId":4})"), "bind1");
  float tri[] = { -0.9f,-0.9f, -0.7f,-0.9f, -0.8f,-0.7f };
  t.gpuBufs.setCpuData(3, tri, sizeof(tri));

  // Buffer for line (id=6)
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "buf2");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":6,"format":"pos2_clip","vertexCount":2})"), "geom2");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Line"})"), "di2");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":8,"pipeline":"line2d@1","geometryId":7})"), "bind2");
  float line[] = { -1.0f, 0.0f, 1.0f, 0.0f };
  t.gpuBufs.setCpuData(6, line, sizeof(line));

  // Buffer for rect (id=9)
  requireOk(t.cp.applyJsonText(R"({"cmd":"createBuffer","byteLength":16})"), "buf3");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"createGeometry","vertexBufferId":9,"format":"rect4","vertexCount":1})"), "geom3");
  requireOk(t.cp.applyJsonText(R"({"cmd":"createDrawItem","layerId":2,"name":"Rect"})"), "di3");
  requireOk(t.cp.applyJsonText(
    R"({"cmd":"bindDrawItem","drawItemId":11,"pipeline":"instancedRect@1","geometryId":10})"), "bind3");
  float rect[] = { 0.5f, 0.5f, 0.9f, 0.9f };
  t.gpuBufs.setCpuData(9, rect, sizeof(rect));

  std::vector<std::uint8_t> pixels;
  dc::Stats stats = t.renderAndRead(pixels);

  std::printf("    drawCalls=%u\n", stats.drawCalls);
  requireTrue(stats.drawCalls == 3, "multi-pipeline drawCalls==3");
  std::printf("    PASS\n");
}

int main() {
  std::printf("D2.3 Pipeline Expansion Tests\n");

  testLine2d();
  testPoints();
  testInstancedRect();
  testInstancedCandle();
  testMultiPipeline();

  std::printf("\nD2.3 pipelines PASS\n");
  return 0;
}
