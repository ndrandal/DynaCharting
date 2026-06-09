// dc_gpu — WebGPU/Dawn rendering backend (scaffold).
//
// ENC-479 (P0.1): this header exists so future dc_gpu sources/consumers have a
// public include path (core/include/gpu/...). The real WebGPU backend lands in
// later P0/P1 tickets. For now it only exposes a build-marker symbol so the
// dc_gpu static library is a real, linkable target.
#pragma once

namespace dc {
namespace gpu {

// Returns true. Exists purely so dc_gpu has a defined, linkable symbol and so
// downstream code can confirm it linked against the WebGPU/Dawn backend.
bool gpu_backend_available();

} // namespace gpu
} // namespace dc
