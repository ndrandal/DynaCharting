// dc_gpu placeholder translation unit (ENC-479, P0.1).
//
// Gives the dc_gpu static library a single real symbol so it is a genuine,
// linkable target before the WebGPU/Dawn rendering backend is implemented in
// later tickets. Replace/extend as the backend grows.
#include "gpu/Gpu.hpp"

namespace dc {
namespace gpu {

bool gpu_backend_available() {
  return true;
}

} // namespace gpu
} // namespace dc
