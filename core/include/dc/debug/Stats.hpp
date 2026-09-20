#pragma once
#include <cstdint>

namespace dc {

struct DebugToggles {
  bool showBounds = false;
  bool wireframe  = false; // host may ignore if not available
};

struct Stats {
  // Timing — ENC-1265.
  //
  // renderCpuMs is a WALL-CLOCK measurement, in milliseconds, of the CPU-side
  // work inside IRendererBackend::render. Precisely:
  //
  //   INCLUDES  the scene walk (pane -> layer -> drawItem traversal, frustum
  //             culling, scissor/blend/clip state changes), the per-pipeline
  //             backend draw encoding, and the render-pass begin/end — on Dawn,
  //             DawnDevice submits the command encoder at endRenderPass().
  //   EXCLUDES  GPU execution beyond whatever that submit happens to block on,
  //             the framebuffer readback, and any host-side blit to a canvas.
  //
  // It is therefore NOT a frame budget and NOT a GPU time. Every consumer that
  // displays it must say which of those three it is showing; the field name is
  // the first line of that defence.
  //
  // This field was `frameMs` from 2026-05 until 2026-09-20 and was assigned by
  // nothing in core/ — `DawnSceneRenderer::render` value-initialised `Stats{}`
  // and filled only drawCalls/culledDrawCalls. So it read 0.0 forever, the WASM
  // host copied the zero, EngineHost p95'd a window of zeros, and the showcase
  // HUD fell through to `1000 / fps` — printing one measurement twice under two
  // different unit labels, in 22 committed stills. The rename is deliberate: a
  // consumer left on the old name is now a COMPILE ERROR rather than a silent
  // zero, which is the property the original name did not have.
  //
  // Enforced by dc_enc1265_render_timing (DEFAULT build — see DC-L01), which
  // fails if any `*Ms` field declared here is assigned nowhere under core/src or
  // core/wasm.
  double renderCpuMs = 0.0;

  // Rendering
  std::uint32_t drawCalls = 0;
  std::uint32_t culledDrawCalls = 0;

  // Upload activity (host-side typically)
  std::uint64_t uploadedBytesThisFrame = 0;

  // Resource counts
  std::uint32_t activeBuffers = 0;

  DebugToggles debug{};
};

} // namespace dc
