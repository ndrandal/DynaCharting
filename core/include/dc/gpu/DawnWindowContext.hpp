// ENC-497 (P4.1) — DawnWindowContext: on-screen windowed presentation via a Dawn
// WebGPU surface + swapchain.
//
// GOAL
// ----
// Everything in dc_gpu up to now is HEADLESS: DawnDevice renders the Scene into an
// offscreen RGBA8Unorm target and reads it back to CPU. This adds the missing
// on-screen half — a real OS window whose backbuffer is driven by the same Dawn
// device + the same DawnSceneRenderer. It is the canonical embedding template that
// replaces the deleted GL hello_glfw demo.
//
// HOW THE SURFACE IS CREATED (GLFW X11 -> wgpu::Surface)
// ------------------------------------------------------
//   1. glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11) — force the X11 backend even
//      under a Wayland session (XWayland), because Dawn's Vulkan surface support
//      we re-enable is X11 (DAWN_USE_X11=ON), not Wayland. This matches how Chrome
//      runs WebGPU here with --ozone-platform=x11.
//   2. glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API) — WebGPU (not GLFW) owns the
//      rendering context; GLFW must not create a GL context on the window.
//   3. glfwGetX11Display() / glfwGetX11Window() (GLFW_EXPOSE_NATIVE_X11 +
//      glfw3native.h) -> a wgpu::SurfaceSourceXlibWindow chained struct ->
//      instance.CreateSurface(). This mirrors Dawn's own webgpu_glfw utils.cpp.
//   4. surface.GetCapabilities(adapter, ...) picks the preferred format +
//      present mode; surface.Configure() sets up the swapchain (Fifo / vsync).
//
// HOW RENDERING TARGETS THE SWAPCHAIN TEXTURE
// -------------------------------------------
// DawnDevice's pipelines are baked for an RGBA8Unorm color target, but a real
// X11/Vulkan swapchain is typically BGRA8Unorm. Rather than rebuild every pipeline
// per surface format, each frame we:
//   a. render the whole Scene through DawnSceneRenderer into the device's existing
//      offscreen target id 0 (unchanged headless path — its color texture already
//      carries TextureBinding usage so it is sampleable), then
//   b. acquire the swapchain texture (surface.GetCurrentTexture()) and run a tiny
//      fullscreen-triangle BLIT pass that samples the offscreen color view and
//      writes it to the swapchain texture, then
//   c. surface.Present() + glfwPollEvents().
// The blit pipeline's color target format is the CONFIGURED surface format, so it
// handles the RGBA8->BGRA8 channel swap implicitly (the sampler reads RGBA, the
// fragment writes the same vec4, the swapchain interprets the bytes as BGRA — and
// because we write through a typed render target, the GPU stores the right
// channels). This keeps the scene pipelines untouched and additive.
//
// VALIDATION HOOK
// ---------------
// Auto-verifying an on-screen window is hard, so the demo can ask the context to
// read the offscreen scene target back to a PNG (readSceneToPng) — a pixel
// artifact proving the windowed render pipeline produced the scene.
//
// GATING: this whole file compiles only under DC_DAWN_WINDOWED (the windowed CMake
// build path that re-enables DAWN_USE_X11 + links the system GLFW). The default
// headless build never sees it.
#pragma once

#include "dc/gpu/DawnDevice.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

namespace dc {

class Scene;
class CpuBufferStore;
class DawnSceneRenderer;

// Owns a GLFW X11 window + a Dawn wgpu::Surface/swapchain bound to a DawnDevice,
// and presents a DawnSceneRenderer's scene into it each frame. Borrows the device
// (the caller — typically a DawnSceneRenderer — owns it and keeps it alive).
class DawnWindowContext {
 public:
  DawnWindowContext() = default;
  ~DawnWindowContext();

  DawnWindowContext(const DawnWindowContext&) = delete;
  DawnWindowContext& operator=(const DawnWindowContext&) = delete;

  // Create the GLFW X11 window (w x h, titled), build the wgpu::Surface from the
  // device's instance + the window's X11 display/window, and Configure the
  // swapchain. `device` must already be init()'d. Returns false + errorMessage()
  // on any failure (GLFW init, window create, surface create, no usable format).
  bool init(DawnDevice& device, int width, int height, const char* title);

  const std::string& errorMessage() const { return errorMessage_; }

  // Should the present loop keep running? False once the user closes the window.
  bool shouldClose() const;

  // Render `scene` through `renderer` into the device's offscreen target, blit
  // that onto the current swapchain texture, Present, and pump window events.
  // Returns false if the swapchain texture could not be acquired (e.g. the window
  // was resized/lost) — the caller may reconfigure or stop.
  bool presentFrame(DawnSceneRenderer& renderer, const Scene& scene,
                    CpuBufferStore& store);

  // Read the device's offscreen scene target (the exact pixels just blitted to the
  // window) back to a tightly-packed top-down RGBA8 PNG. Proof artifact for the
  // headless validation of the windowed pipeline. Returns false on readback/encode
  // failure. Must be called after at least one presentFrame() (so target id 0
  // exists). Uses the device's readFramebufferRGBA path.
  bool readSceneToPng(const std::string& path);

  // Encode a tightly-packed top-down RGBA8 image to a PNG file. Self-contained
  // (no zlib/stb dep). Exposed static so the demo's --headless path (no window /
  // surface) can also write the proof artifact straight from a renderer readback.
  // Returns false on size mismatch or file-write failure.
  static bool writeRgbaPng(const std::string& path, int w, int h,
                           const std::vector<std::uint8_t>& rgba);

  int width() const { return width_; }
  int height() const { return height_; }
  GLFWwindow* window() const { return window_; }
  // The wgpu::TextureFormat the swapchain was configured with (for diagnostics).
  wgpu::TextureFormat surfaceFormat() const { return surfaceFormat_; }

 private:
  // Build the fullscreen-triangle blit pipeline whose color target format == the
  // configured surface format. Called once from init() after Configure().
  bool buildBlitPipeline();

  DawnDevice* device_{nullptr};  // borrowed
  GLFWwindow* window_{nullptr};
  wgpu::Surface surface_;
  wgpu::TextureFormat surfaceFormat_{wgpu::TextureFormat::BGRA8Unorm};
  int width_{0};
  int height_{0};
  bool glfwInited_{false};
  std::string errorMessage_;

  // The blit pipeline + its bind-group layout + a sampler. The bind group is
  // rebuilt each frame (it references the offscreen color view, which is recreated
  // on resize). The pipeline/layout/sampler are immutable after init().
  wgpu::RenderPipeline blitPipeline_;
  wgpu::BindGroupLayout blitBindGroupLayout_;
  wgpu::Sampler blitSampler_;
};

}  // namespace dc
