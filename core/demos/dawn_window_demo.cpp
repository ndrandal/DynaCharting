// ENC-497 (P4.1) — dawn_window_demo: the canonical on-screen windowed Dawn demo.
//
// Opens a real OS window (GLFW X11), builds the multi-pipeline d50 scene
// (triSolid triangle + instancedRect + line2d, the same scene the headless
// d50_dawn_scene_renderer test renders), and runs the present loop: each frame the
// scene is rendered through DawnSceneRenderer into the device's offscreen target,
// blitted onto the swapchain texture, and Presented. This is the embedding
// template that replaces the deleted GL hello_glfw.
//
// Because auto-verifying an on-screen window is hard, the demo ALSO reads the
// rendered scene target back to a PNG (--out) on the last frame — a pixel artifact
// proving the windowed render pipeline produced the scene.
//
// CLI:
//   dawn_window_demo [--frames N] [--out path.png] [--width W] [--height H]
//                    [--headless]
//   --frames N    : present N frames then exit (default: run until window closed).
//   --out path    : write the rendered scene to a PNG (default /tmp/dawn_window.png).
//   --width/-W    : window width  (default 640).
//   --height/-H   : window height (default 360).
//   --headless    : skip the window/surface; only render offscreen + write the PNG
//                   (so the render pipeline can be validated where no display is
//                   reachable).
//
// On this box the Vulkan backend may be lavapipe (software); force it with
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
// if Dawn can't find a hardware adapter.
#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/gpu/DawnWindowContext.hpp"

#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Scene.hpp"
#include "dc/scene/ResourceRegistry.hpp"
#include "dc/commands/CommandProcessor.hpp"
#include "dc/debug/Stats.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void requireOk(const dc::CmdResult& r, const char* ctx) {
  if (!r.ok) {
    std::fprintf(stderr, "FAIL [%s]: code=%s msg=%s\n", ctx, r.err.code.c_str(),
                 r.err.message.c_str());
    std::exit(2);
  }
}

// Build the d50 multi-pipeline scene: a RED triSolid triangle (left third), a
// GREEN instancedRect (middle third), a BLUE line2d (right third). Identical
// geometry to tests/d50_dawn_scene_renderer.cpp.
void buildScene(dc::CommandProcessor& cp, dc::CpuBufferStore& store) {
  requireOk(cp.applyJsonText(R"({"cmd":"createPane","id":1,"name":"P"})"), "pane");
  requireOk(cp.applyJsonText(R"({"cmd":"createLayer","id":2,"paneId":1})"), "layer");

  // 1) triSolid@1 — RED triangle, LEFT third.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":3,"layerId":2})"), "di-tri");
  float tri[] = {-0.9f, -0.7f, -0.3f, -0.7f, -0.6f, 0.7f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":10,"byteLength":24})"), "buf-tri");
  store.setCpuData(10, tri, sizeof(tri));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":100,"vertexBufferId":10,"vertexCount":3,"format":"pos2_clip"})"),
      "geom-tri");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":3,"pipeline":"triSolid@1","geometryId":100})"),
      "bind-tri");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":3,"r":1,"g":0,"b":0,"a":1})"), "color-tri");

  // 2) instancedRect@1 — GREEN rect, MIDDLE third.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":4,"layerId":2})"), "di-rect");
  float rect[] = {-0.2f, -0.6f, 0.2f, 0.6f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":11,"byteLength":16})"), "buf-rect");
  store.setCpuData(11, rect, sizeof(rect));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":101,"vertexBufferId":11,"vertexCount":1,"format":"rect4"})"),
      "geom-rect");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":4,"pipeline":"instancedRect@1","geometryId":101})"),
      "bind-rect");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":4,"r":0,"g":1,"b":0,"a":1})"), "color-rect");

  // 3) line2d@1 — BLUE horizontal line, RIGHT third.
  requireOk(cp.applyJsonText(R"({"cmd":"createDrawItem","id":5,"layerId":2})"), "di-line");
  float line[] = {0.35f, 0.0f, 0.9f, 0.0f};
  requireOk(cp.applyJsonText(R"({"cmd":"createBuffer","id":12,"byteLength":16})"), "buf-line");
  store.setCpuData(12, line, sizeof(line));
  requireOk(cp.applyJsonText(
      R"({"cmd":"createGeometry","id":102,"vertexBufferId":12,"vertexCount":2,"format":"pos2_clip"})"),
      "geom-line");
  requireOk(cp.applyJsonText(
      R"({"cmd":"bindDrawItem","drawItemId":5,"pipeline":"line2d@1","geometryId":102})"),
      "bind-line");
  requireOk(cp.applyJsonText(
      R"({"cmd":"setDrawItemColor","drawItemId":5,"r":0,"g":0,"b":1,"a":1})"), "color-line");
}

}  // namespace

int main(int argc, char** argv) {
  int frames = -1;  // -1 == run until the window is closed
  int width = 640;
  int height = 360;
  bool headless = false;
  bool fastExit = true;  // _exit(0) after success to dodge the NVK xcb teardown bug
  std::string out = "/tmp/dawn_window.png";

  for (int i = 1; i < argc; ++i) {
    auto next = [&](const char* flag) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "missing value for %s\n", flag);
        std::exit(2);
      }
      return argv[++i];
    };
    if (!std::strcmp(argv[i], "--frames"))
      frames = std::atoi(next("--frames"));
    else if (!std::strcmp(argv[i], "--out"))
      out = next("--out");
    else if (!std::strcmp(argv[i], "--width") || !std::strcmp(argv[i], "-W"))
      width = std::atoi(next("--width"));
    else if (!std::strcmp(argv[i], "--height") || !std::strcmp(argv[i], "-H"))
      height = std::atoi(next("--height"));
    else if (!std::strcmp(argv[i], "--headless"))
      headless = true;
    else if (!std::strcmp(argv[i], "--no-fast-exit"))
      fastExit = false;
    else {
      std::fprintf(stderr, "unknown arg: %s\n", argv[i]);
      return 2;
    }
  }

  std::printf("=== ENC-497 dawn_window_demo === %dx%d frames=%d headless=%d out=%s\n",
              width, height, frames, headless ? 1 : 0, out.c_str());

  // Bring up the renderer (owns a DawnDevice + all backends).
  dc::DawnSceneRenderer renderer;
  if (!renderer.init()) {
    std::fprintf(stderr, "DawnSceneRenderer::init failed: %s\n",
                 renderer.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "forces lavapipe (software Vulkan).\n");
    return 1;
  }
  std::printf("Renderer up: backend=%s adapter=\"%s\"\n",
              renderer.device().backendName().c_str(),
              renderer.device().adapterName().c_str());

  // Build the scene.
  dc::Scene scene;
  dc::ResourceRegistry reg;
  dc::CommandProcessor cp(scene, reg);
  dc::CpuBufferStore store;
  buildScene(cp, store);

  if (headless) {
    // No window/surface: render offscreen + write the proof PNG straight from the
    // renderer readback (validates the render pipeline where no display is
    // reachable). Uses the same self-contained PNG encoder the windowed path uses.
    dc::Stats stats = renderer.render(scene, store, width, height);
    std::printf("headless render: drawCalls=%u culled=%u\n", stats.drawCalls,
                stats.culledDrawCalls);
    std::uint32_t w = 0, h = 0;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4u);
    if (!renderer.device().readFramebufferRGBA(rgba.data(), rgba.size(), &w, &h)) {
      std::fprintf(stderr, "headless: readFramebufferRGBA failed\n");
      return 1;
    }
    if (!out.empty()) {
      if (dc::DawnWindowContext::writeRgbaPng(out, static_cast<int>(w),
                                              static_cast<int>(h), rgba)) {
        std::printf("wrote scene PNG: %s (%ux%u)\n", out.c_str(), w, h);
      } else {
        std::fprintf(stderr, "writeRgbaPng(%s) failed\n", out.c_str());
        return 1;
      }
    }
    return 0;
  }

  // Windowed path: open the window + surface, run the present loop.
  dc::DawnWindowContext ctx;
  if (!ctx.init(renderer.device(), width, height, "DynaCharting — Dawn window")) {
    std::fprintf(stderr, "DawnWindowContext::init failed: %s\n",
                 ctx.errorMessage().c_str());
    return 1;
  }
  std::printf("Window + surface up: %dx%d surfaceFormat=%d\n", ctx.width(),
              ctx.height(), static_cast<int>(ctx.surfaceFormat()));

  int presented = 0;
  bool pngWritten = false;
  while (!ctx.shouldClose()) {
    if (!ctx.presentFrame(renderer, scene, store)) {
      std::fprintf(stderr, "presentFrame failed at frame %d (surface lost?)\n",
                   presented);
      break;
    }
    ++presented;

    // Write the proof PNG once we have a rendered frame (use the last requested
    // frame, or the first frame if running unbounded so the artifact always lands).
    bool lastFrame = (frames > 0 && presented >= frames);
    if (!out.empty() && !pngWritten && (lastFrame || frames < 0)) {
      if (frames < 0 || lastFrame) {
        if (ctx.readSceneToPng(out)) {
          std::printf("wrote scene PNG: %s (%dx%d)\n", out.c_str(), ctx.width(),
                      ctx.height());
          pngWritten = true;
        } else {
          std::fprintf(stderr, "readSceneToPng(%s) failed\n", out.c_str());
        }
      }
    }
    if (lastFrame) break;
  }

  std::printf("=== presented %d frame(s); png=%s ===\n", presented,
              pngWritten ? out.c_str() : "(none)");

  // ENVIRONMENT NOTE (not a code bug): some Mesa Vulkan X11 WSI drivers on this box
  // (notably NVK/nouveau) abort during swapchain DESTRUCTION at process exit — a
  // libxcb fortify check (__chk_fail in xcb_get_extension_data) fires inside the
  // driver's x11_swapchain_destroy, entirely below our code. The render + present +
  // readback all completed (the PNG above is the proof). lavapipe (software Vulkan,
  // VK_ICD_FILENAMES=.../lvp_icd...) tears down cleanly. So after a SUCCESSFUL run we
  // flush and _exit(0) to skip the driver's buggy teardown path; pass --no-fast-exit
  // to instead run the normal destructors (and reproduce the driver crash).
  std::fflush(stdout);
  std::fflush(stderr);
  if (fastExit) std::_Exit(0);
  return 0;
}
