// ENC-501 (P5 cutover) — shared Dawn render helper for the headless demo servers.
//
// The showcase / live / dashboard servers and the gallery PNG demo used to render
// through the GL path (OsMesaContext + Renderer + GpuBufferManager, bottom-up
// readback). dc_gl is deleted, so they now render through DawnSceneRenderer +
// CpuBufferStore and read the offscreen target back per-pixel via
// DawnDevice::readPixel (TOP-DOWN origin). This header centralises the readback
// loop so each server's migration stays small and consistent. It mirrors the
// JsonHost Dawn path (core/src/host/JsonHost.cpp, DawnHostBackend::render).
//
// Windowed/interactive on-screen demos are NOT migrated here — windowing on Dawn
// is ENC-497; those demos were removed from the build.
#pragma once

#include "dc/gpu/DawnSceneRenderer.hpp"
#include "dc/scene/Scene.hpp"

#include <cstdint>
#include <vector>

namespace dc::demo {

// Render `scene` (with CPU bytes already pushed into `store`) through
// `renderer` at (W,H) and fill `outRgba` with a TOP-DOWN, row-major RGBA frame
// (size == W*H*4). DawnDevice::readPixel is already top-down origin.
inline void renderTopDown(dc::DawnSceneRenderer& renderer, const dc::Scene& scene,
                          dc::CpuBufferStore& store, int W, int H,
                          std::vector<std::uint8_t>& outRgba) {
  renderer.render(scene, store, W, H);
  outRgba.assign(static_cast<std::size_t>(W) * H * 4, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      std::uint8_t px[4] = {0, 0, 0, 0};
      renderer.device().readPixel(x, y, px);
      std::size_t idx = (static_cast<std::size_t>(y) * W + x) * 4;
      outRgba[idx + 0] = px[0];
      outRgba[idx + 1] = px[1];
      outRgba[idx + 2] = px[2];
      outRgba[idx + 3] = px[3];
    }
  }
}

}  // namespace dc::demo
