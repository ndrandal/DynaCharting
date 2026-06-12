// ENC-591 (P0.3 — DE-RISK SPIKE) — WebGPU COMPUTE pipeline end-to-end on
// DawnDevice (the functional proof, native / lavapipe).
//
// This is the SINGLE HIGHEST-UNCERTAINTY item in the "GPU-Native Streaming
// Grammar of Graphics" plan: it determines whether the GPU-compute half (Phases
// 4 & 6 — in-engine KDE/FFT/contour + the custom-WGSL escape hatch) is viable.
// The question this test answers: does a WebGPU compute shader run end-to-end
// through DynaCharting's Dawn integration?
//
// It builds on ENC-590's storage-buffer path (createStorageBuffer + the map-pump
// readback) and exercises the new ENC-591 compute scaffolding
// (createComputePipeline + dispatchCompute) with a TRIVIAL kernel:
//
//     @compute @workgroup_size(64) fn main(...) { data[i] = data[i] * 2.0; }
//
//   * create a Storage buffer of N f32s with a known pattern,
//   * compile + build the compute pipeline (auto bind-group layout),
//   * dispatch ceil(N/64) workgroups over it,
//   * read the result back via the existing async map-pump (readBuffer), and
//   * assert EVERY element is exactly 2x the input (exact f32 equality — *2.0 is
//     a power-of-two scale, lossless for these inputs, so an exact compare is the
//     right correctness bar).
//
// SCOPE: only the compute scaffolding + the trivial kernel round-trip. No
// transforms, no real algorithms (those are Phases 4/6). The render path is
// untouched.
//
// On this headless box the only Vulkan backend may be lavapipe (software). If
// Dawn can't find an adapter, force the ICD:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/DawnDevice.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void requireTrue(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "ASSERT FAIL: %s\n", msg);
    std::exit(1);
  }
}

// The trivial de-risk kernel: multiply each f32 element by 2.0 in place. A single
// read_write storage buffer at @group(0) @binding(0). workgroup_size(64) is well
// under WebGPU's hard cap of 256 invocations/workgroup (RESEARCH.md §5.1). The
// bounds guard makes the dispatch safe to round the workgroup count up.
static const char* kDoubleKernelWgsl = R"WGSL(
@group(0) @binding(0) var<storage, read_write> data : array<f32>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
  let i = gid.x;
  if (i < arrayLength(&data)) {
    data[i] = data[i] * 2.0;
  }
}
)WGSL";

int main() {
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

  // --- 1. Input: N f32s with a deterministic, non-trivial pattern. ---------
  // N chosen NOT a multiple of 64 so the workgroup round-up + the in-kernel
  // bounds guard are both exercised (the last workgroup runs partially).
  const std::uint32_t kN = 1000;
  std::vector<float> input(kN);
  for (std::uint32_t i = 0; i < kN; ++i) {
    // Mix sign, magnitude, and a fractional part; all exactly representable so
    // *2.0 (a lossless power-of-two scale) gives an exact expected value.
    input[i] = (static_cast<float>(i) - 500.0f) * 0.25f;
  }
  const std::size_t kBytes = static_cast<std::size_t>(kN) * sizeof(float);

  // Size the storage buffer to capacity up front (ENC-590 note: the grow-past-
  // capacity realloc path falls back to non-storage usage, so no growth here).
  dc::BufferHandle storage =
      dev.createStorageBuffer(kBytes, input.data(), kBytes);
  requireTrue(storage.valid(), "createStorageBuffer returned a valid handle");

  // --- 2. Build the compute pipeline (compile WGSL + auto bind-group layout).
  dc::ComputePipelineHandle pipe =
      dev.createComputePipeline(kDoubleKernelWgsl, "main");
  requireTrue(pipe.valid(),
              "createComputePipeline returned a valid handle (WGSL compiled, "
              "compute pipeline built)");

  // --- 3. Dispatch ceil(N / 64) workgroups over the buffer. ----------------
  const std::uint32_t kWorkgroupSize = 64;
  const std::uint32_t kGroups = (kN + kWorkgroupSize - 1) / kWorkgroupSize;
  requireTrue(dev.dispatchCompute(pipe, {storage}, kGroups),
              "dispatchCompute succeeded");

  // --- 4. Read the result back via the existing map-pump readback. ---------
  std::vector<float> result(kN, -123.0f);  // poison the destination first
  requireTrue(
      dev.readBuffer(storage, 0, kBytes,
                     reinterpret_cast<std::uint8_t*>(result.data())),
      "readBuffer (map-pump readback) succeeded");

  // --- 5. Assert EVERY element is exactly 2x the input. --------------------
  for (std::uint32_t i = 0; i < kN; ++i) {
    const float expected = input[i] * 2.0f;
    if (result[i] != expected) {
      std::fprintf(stderr,
                   "ASSERT FAIL: compute result mismatch at element %u: "
                   "in=%.6f want=%.6f got=%.6f\n",
                   i, input[i], expected, result[i]);
      return 1;
    }
  }

  std::printf(
      "\nENC-591 compute round-trip: OK "
      "(%u f32s, @compute @workgroup_size(%u), %u workgroups dispatched — "
      "every element exactly doubled; backend=%s)\n",
      kN, kWorkgroupSize, kGroups, dev.backendName().c_str());
  std::printf(
      "VERDICT(native): WebGPU compute pipeline runs end-to-end through "
      "DawnDevice's Dawn integration.\n");
  return 0;
}
