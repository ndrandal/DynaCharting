// ENC-484 (P2.1) — triSolid@1 end-to-end on Dawn (D2.1 first render).
//
// The FIRST real WebGPU geometry render in the DynaCharting migration. This is
// the Dawn counterpart of d2_1_first_render.cpp (the GL baseline): it builds the
// SAME colored-triangle scene, renders it through the backend registry with
// DeviceKind::Dawn into the headless DawnDevice offscreen RGBA8 target, reads
// the result back synchronously, and asserts:
//   * pixels INSIDE the triangle equal the fill color (red), and
//   * a pixel in a CLEAR region equals the clear color (black),
// and cross-checks the triangle interior against the GL baseline's fill color.
//
// Scene (identical to d2_1_first_render): a single red triSolid@1 triangle with
// clip-space vertices (-0.5,-0.5),(0.5,-0.5),(0.0,0.5), no transform (identity),
// cleared to black.
//
// NDC NOTE: DawnTriSolidBackend negates clip-space y so the WebGPU top-left
// framebuffer matches the GL bottom-left readback. The triangle is symmetric in
// x about 0 and its apex (0,0.5) maps below center after the flip; the interior
// sample points below are chosen to lie inside the triangle in BOTH orientations
// (the center is always covered), and the clear sample is a far corner.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"
#include "dc/gpu/DawnTriSolidBackend.hpp"

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

int main() {
  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

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

  // --- 2. Build the triSolid scene (same as d2_1_first_render). -----------
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
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":5,"r":1.0,"g":0.0,"b":0.0,"a":1.0})"),
      "setDrawItemColor");

  // Triangle: (-0.5,-0.5), (0.5,-0.5), (0.0,0.5) in clip space.
  float verts[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f,
  };
  dc::GpuBufferManager gpuBufs;
  gpuBufs.setCpuData(3, verts, sizeof(verts));  // CPU bytes only; no GL upload.

  // --- 3. Register the Dawn triSolid backend in the registry. -------------
  dc::DawnTriSolidBackend triSolid;
  requireTrue(triSolid.init(dev), "DawnTriSolidBackend::init (pipeline create)");

  dc::BackendRegistry backends;
  backends.registerBackend(dc::DeviceKind::Dawn, &triSolid);

  // --- 4. Render through the registry into the offscreen target. ----------
  dc::RenderPassDesc rp;
  rp.target = {};  // headless offscreen target
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = 0.0f;  // black background (matches the GL baseline)
  rp.clearColor[1] = 0.0f;
  rp.clearColor[2] = 0.0f;
  rp.clearColor[3] = 1.0f;
  dev.beginRenderPass(rp);

  std::uint32_t drawCalls = 0;
  // Mirror the Renderer dispatcher: look the backend up by (kind, pipelineId)
  // and route the DrawItem through it. (Single draw item id 5 here.)
  const dc::DrawItem* di = scene.getDrawItem(5);
  requireTrue(di != nullptr, "drawItem 5 exists");
  if (dc::IRendererBackend* be =
          backends.find(dc::DeviceKind::Dawn, di->pipeline)) {
    dc::BackendStats bs = be->renderDrawItem(dev, scene, gpuBufs, *di,
                                             static_cast<int>(W),
                                             static_cast<int>(H));
    drawCalls += bs.drawCalls;
  } else {
    requireTrue(false, "no Dawn backend registered for triSolid@1");
  }

  dev.endRenderPass();
  std::printf("Draw calls: %u\n", drawCalls);
  requireTrue(drawCalls == 1, "drawCalls == 1");

  // --- 5. Read back + assert. ---------------------------------------------
  // DawnDevice::readPixel reads from the top-left-origin offscreen texture.
  auto readPx = [&](std::uint32_t x, std::uint32_t y, std::uint8_t* out) {
    dev.readPixel(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
                  out);
  };

  std::uint8_t center[4] = {0, 0, 0, 0};
  readPx(W / 2, H / 2, center);
  std::printf("Center pixel: R=%u G=%u B=%u A=%u\n",
              center[0], center[1], center[2], center[3]);

  // Interior sample: the centroid lies at clip (0, -1/6); after the y-flip it
  // maps just above framebuffer center. Sample a couple of clearly-interior
  // points to be robust to the flip and to rasterization of the centroid edge.
  std::uint8_t interior[4] = {0, 0, 0, 0};
  readPx(W / 2, H / 2 - 6, interior);  // a few px toward the wide base
  std::printf("Interior pixel: R=%u G=%u B=%u A=%u\n",
              interior[0], interior[1], interior[2], interior[3]);

  std::uint8_t interior2[4] = {0, 0, 0, 0};
  readPx(W / 2, H / 2 + 6, interior2);
  std::printf("Interior pixel 2: R=%u G=%u B=%u A=%u\n",
              interior2[0], interior2[1], interior2[2], interior2[3]);

  // Clear region: top-left corner is always outside the triangle.
  std::uint8_t corner[4] = {0, 0, 0, 0};
  readPx(1, 1, corner);
  std::printf("Corner pixel: R=%u G=%u B=%u A=%u\n",
              corner[0], corner[1], corner[2], corner[3]);

  // The triangle is solid (non-AA) and the fill is opaque red; lavapipe/NVK
  // produce exact 255/0/0/255. Allow a small tolerance for robustness.
  auto isRed = [](const std::uint8_t* p) {
    return p[0] > 200 && p[1] < 16 && p[2] < 16;
  };
  auto isBlack = [](const std::uint8_t* p) {
    return p[0] < 16 && p[1] < 16 && p[2] < 16;
  };

  requireTrue(isRed(center), "center is fill color (red)");
  requireTrue(isRed(interior), "interior is fill color (red)");
  requireTrue(isRed(interior2), "interior 2 is fill color (red)");
  requireTrue(isBlack(corner), "corner is clear color (black)");

  // Cross-check vs the GL baseline's expected triSolid output for this scene:
  // d2_1_first_render asserts center R>200, G<10, B<10 — the SAME fill color.
  requireTrue(center[0] > 200 && center[1] < 10 && center[2] < 10,
              "center matches GL baseline fill (R>200,G<10,B<10)");

  std::printf("\nD2.1 Dawn first render PASS "
              "(triSolid@1 on %s matches GL baseline)\n",
              dev.backendName().c_str());
  return 0;
}
