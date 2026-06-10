// ENC-480 (P0.2) — Headless Dawn device clear + synchronous readback test.
//
// Acceptance for the first WebGPU/Dawn device: stand up a headless DawnDevice,
// clear an offscreen RGBA8Unorm target to a KNOWN color, read the center pixel
// back synchronously, and assert it matches. This is the foundation every later
// Dawn render test (ENC-484+) builds on.
//
// On this headless box the only Vulkan backend is lavapipe (software). If Dawn
// can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

int main() {
  constexpr std::uint32_t W = 64;
  constexpr std::uint32_t H = 64;

  // Known clear color (the ticket's reference value).
  constexpr std::uint8_t kR = 32;
  constexpr std::uint8_t kG = 64;
  constexpr std::uint8_t kB = 128;
  constexpr std::uint8_t kA = 255;

  dc::DawnDevice dev;
  if (!dev.init()) {
    // No adapter available: treat as a clear FAIL since this box has lavapipe.
    // (A genuinely adapterless environment would need VK_ICD_FILENAMES set.)
    std::fprintf(stderr,
                 "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: set "
                 "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json "
                 "to force lavapipe (software Vulkan).\n");
    return 1;
  }

  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // Clear the offscreen target to the known color via a render pass.
  dc::RenderPassDesc rp;
  rp.target = {};  // offscreen headless target
  rp.viewportWidth = W;
  rp.viewportHeight = H;
  rp.clear = true;
  rp.clearColor[0] = kR / 255.0f;
  rp.clearColor[1] = kG / 255.0f;
  rp.clearColor[2] = kB / 255.0f;
  rp.clearColor[3] = kA / 255.0f;
  dev.beginRenderPass(rp);
  dev.endRenderPass();

  // Synchronous readback of the center pixel.
  std::uint8_t px[4] = {0, 0, 0, 0};
  dev.readPixel(static_cast<std::int32_t>(W / 2),
                static_cast<std::int32_t>(H / 2), px);

  std::printf("center pixel = (%u, %u, %u, %u), expected (%u, %u, %u, %u)\n",
              px[0], px[1], px[2], px[3], kR, kG, kB, kA);

  // lavapipe clears are exact; allow a tolerance of 1 LSB for robustness.
  auto close = [](std::uint8_t a, std::uint8_t b) {
    int d = static_cast<int>(a) - static_cast<int>(b);
    return d <= 1 && d >= -1;
  };
  requireTrue(close(px[0], kR), "red channel mismatch");
  requireTrue(close(px[1], kG), "green channel mismatch");
  requireTrue(close(px[2], kB), "blue channel mismatch");
  requireTrue(close(px[3], kA), "alpha channel mismatch");

  std::printf("PASS: Dawn headless clear + synchronous readback matches.\n");
  return 0;
}
