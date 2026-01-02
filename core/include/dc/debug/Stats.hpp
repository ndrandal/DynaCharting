#pragma once
#include <cstdint>

namespace dc {

struct DebugToggles {
  bool showBounds = false;
  bool wireframe  = false; // host may ignore if not available
};

struct Stats {
  // Timing
  double frameMs = 0.0;

  // Rendering
  std::uint32_t drawCalls = 0;

  // Upload activity (host-side typically)
  std::uint64_t uploadedBytesThisFrame = 0;

  // Resource counts
  std::uint32_t activeBuffers = 0;

  DebugToggles debug{};
};

} // namespace dc
