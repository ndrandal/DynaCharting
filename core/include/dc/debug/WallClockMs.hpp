#pragma once
#include <chrono>

namespace dc {

// WallClockMs — ENC-1265. A stopwatch: construct it to stamp a start, read
// elapsedMs() to get the wall-clock time since, in milliseconds.
//
// WHY THIS EXISTS AT ALL. `dc::Stats` carried a `frameMs` field from 2026-05
// until 2026-09-20 that was declared, marshalled across the WASM boundary,
// re-exposed on the TS `EngineHost`, p95'd over a rolling window, and rendered
// into a badge that was burned into 22 committed screenshots — and was assigned
// by nothing, anywhere. It read 0.0 forever, and the HUD silently substituted
// `1000 / fps`, so two spans of the badge showed one measurement under two unit
// labels. There was no timing primitive in `dc` to assign it *from* that did not
// drag in FrameProfiler's 120-entry history. This is that primitive.
//
// WHY IT IS A STOPWATCH AND NOT A SCOPE GUARD. The obvious shape is an RAII
// `ScopedMs(double& out)` that writes on destruction. It is wrong for the one
// caller that matters:
//
//     Stats render(...) { Stats s{}; ScopedMs g(s.renderCpuMs); ...; return s; }
//
// Local destructors run *after* the return object is initialised. Under NRVO `s`
// IS the return object and the guard writes through to the caller; without NRVO
// the move happens first and the caller gets a zero — i.e. the exact defect this
// ticket exists to remove, restored by an optimiser decision. A stopwatch read
// explicitly before `return` has one behaviour.
//
// steady_clock, not system_clock or high_resolution_clock: it is guaranteed
// monotonic, so no clock adjustment can make a frame take negative time.
// (high_resolution_clock is permitted to be an alias for system_clock.)
//
// Under Emscripten steady_clock is backed by emscripten_get_now(), i.e.
// performance.now() — sub-millisecond, monotonic, and the same clock the browser
// times rAF with.
class WallClockMs {
 public:
  WallClockMs() : t0_(std::chrono::steady_clock::now()) {}

  // Wall-clock milliseconds since construction (or since the last restart()).
  // Non-negative by construction. Reading does not stop or reset the clock.
  double elapsedMs() const {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0_)
        .count();
  }

  void restart() { t0_ = std::chrono::steady_clock::now(); }

 private:
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace dc
