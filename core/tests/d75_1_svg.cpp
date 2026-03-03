// D75.1 — SvgExporter: basic export with a triangle
// Create a scene with 1 pane, 1 layer, 1 draw item (triSolid@1),
// export to SVG, verify structural correctness.
#include "dc/export/SvgExporter.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdio>
#include <cstring>
#include <string>

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

static bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

int main() {
  std::printf("=== D75.1 SvgExporter Basic Tests ===\n");

  // ---- Build a simple scene ----
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::IngestProcessor ingest;

  // Create buffer (id=10)
  cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":0})");

  // Create geometry (id=20): 3 vertices (1 triangle), Pos2_Clip
  cp.applyJsonText(R"({"cmd":"createGeometry","id":20,"vertexBufferId":10,"format":"pos2_clip","vertexCount":3})");

  // Populate buffer with triangle vertices (clip-space):
  // v0=(0.0, 0.5), v1=(-0.5, -0.5), v2=(0.5, -0.5)
  {
    float verts[] = {
      0.0f,  0.5f,
     -0.5f, -0.5f,
      0.5f, -0.5f
    };
    ingest.setBufferData(10, reinterpret_cast<const std::uint8_t*>(verts),
                         static_cast<std::uint32_t>(sizeof(verts)));
  }

  // Create pane (id=30)
  cp.applyJsonText(R"({"cmd":"createPane","id":30,"name":"MainPane"})");

  // Create layer (id=40)
  cp.applyJsonText(R"({"cmd":"createLayer","id":40,"paneId":30,"name":"DataLayer"})");

  // Create draw item (id=50)
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":50,"layerId":40,"name":"Triangle"})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":50,"pipeline":"triSolid@1","geometryId":20})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":50,"r":1.0,"g":0.0,"b":0.0,"a":1.0})");

  // ---- Test 1: Export entire scene ----
  dc::SvgExportOptions opts;
  opts.width = 400;
  opts.height = 300;
  opts.title = "Test Chart";

  std::string svg = dc::SvgExporter::exportScene(scene, &ingest, opts);
  check(!svg.empty(), "SVG output is non-empty");
  check(contains(svg, "<svg"), "output contains <svg");
  check(contains(svg, "xmlns=\"http://www.w3.org/2000/svg\""), "SVG namespace present");
  check(contains(svg, "width=\"400\""), "width attribute correct");
  check(contains(svg, "height=\"300\""), "height attribute correct");
  check(contains(svg, "</svg>"), "output contains closing </svg>");
  check(contains(svg, "<title>Test Chart</title>"), "title element present");

  // ---- Test 2: Contains polygon for the triangle ----
  check(contains(svg, "<polygon"), "output contains <polygon");
  check(contains(svg, "#ff0000"), "output contains red color (#ff0000)");

  // ---- Test 3: Verify coordinate mapping ----
  // v0 = (0.0, 0.5) -> SVG: x=(0+1)*0.5*400 = 200, y=(1-0.5)*0.5*300 = 75
  check(contains(svg, "200.00,75.00"), "vertex 0 mapped correctly (200,75)");
  // v1 = (-0.5, -0.5) -> SVG: x=(-0.5+1)*0.5*400 = 100, y=(1+0.5)*0.5*300 = 225
  check(contains(svg, "100.00,225.00"), "vertex 1 mapped correctly (100,225)");
  // v2 = (0.5, -0.5) -> SVG: x=(0.5+1)*0.5*400 = 300, y=(1+0.5)*0.5*300 = 225
  check(contains(svg, "300.00,225.00"), "vertex 2 mapped correctly (300,225)");

  // ---- Test 4: Background rect ----
  check(contains(svg, "width=\"100%\""), "background rect has 100% width");
  check(contains(svg, "height=\"100%\""), "background rect has 100% height");

  // ---- Test 5: Pane group ----
  check(contains(svg, "id=\"pane_30\""), "pane group has correct id");
  check(contains(svg, "id=\"layer_40\""), "layer group has correct id");

  // ---- Test 6: Export without background ----
  dc::SvgExportOptions noBgOpts;
  noBgOpts.includeBackground = false;
  std::string svgNoBg = dc::SvgExporter::exportScene(scene, &ingest, noBgOpts);
  check(!contains(svgNoBg, "width=\"100%\""), "no background when disabled");

  // ---- Test 7: Export a single pane ----
  std::string paneSvg = dc::SvgExporter::exportPane(scene, &ingest, 30, opts);
  check(!paneSvg.empty(), "pane SVG is non-empty");
  check(contains(paneSvg, "<polygon"), "pane SVG contains polygon");
  check(contains(paneSvg, "id=\"pane_30\""), "pane SVG has correct pane id");

  // ---- Test 8: Export non-existent pane returns empty ----
  std::string emptyPaneSvg = dc::SvgExporter::exportPane(scene, &ingest, 999, opts);
  check(emptyPaneSvg.empty(), "non-existent pane returns empty string");

  // ---- Test 9: Export with nullptr ingest ----
  std::string noDataSvg = dc::SvgExporter::exportScene(scene, nullptr, opts);
  check(!noDataSvg.empty(), "SVG with null ingest is non-empty");
  check(contains(noDataSvg, "<svg"), "null ingest still produces valid SVG structure");
  // No polygon because there's no buffer data
  check(!contains(noDataSvg, "<polygon"), "null ingest produces no polygon");

  // ---- Test 10: Points pipeline ----
  cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":21,"vertexBufferId":11,"format":"pos2_clip","vertexCount":2})");
  {
    float pts[] = { 0.0f, 0.0f,  0.5f, 0.5f };
    ingest.setBufferData(11, reinterpret_cast<const std::uint8_t*>(pts),
                         static_cast<std::uint32_t>(sizeof(pts)));
  }
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":51,"layerId":40})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":51,"pipeline":"points@1","geometryId":21})");
  cp.applyJsonText(R"({"cmd":"setDrawItemColor","drawItemId":51,"r":0.0,"g":1.0,"b":0.0,"a":1.0})");

  std::string svg2 = dc::SvgExporter::exportScene(scene, &ingest, opts);
  check(contains(svg2, "<circle"), "points pipeline produces <circle> elements");
  check(contains(svg2, "#00ff00"), "green color for points");

  // ---- Test 11: Lines pipeline ----
  cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":0})");
  cp.applyJsonText(R"({"cmd":"createGeometry","id":22,"vertexBufferId":12,"format":"pos2_clip","vertexCount":2})");
  {
    float lineVerts[] = { -0.5f, 0.0f,  0.5f, 0.0f };
    ingest.setBufferData(12, reinterpret_cast<const std::uint8_t*>(lineVerts),
                         static_cast<std::uint32_t>(sizeof(lineVerts)));
  }
  cp.applyJsonText(R"({"cmd":"createDrawItem","id":52,"layerId":40})");
  cp.applyJsonText(R"({"cmd":"bindDrawItem","drawItemId":52,"pipeline":"line2d@1","geometryId":22})");

  std::string svg3 = dc::SvgExporter::exportScene(scene, &ingest, opts);
  check(contains(svg3, "<line"), "line2d pipeline produces <line> elements");

  std::printf("=== D75.1 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
