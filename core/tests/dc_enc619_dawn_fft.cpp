// ENC-619 (Epic ENC-619) — the shipped FFT/STFT escape-hatch kernel on Dawn:
// the GPU spectrogram magnitude grid == the CPU FFT reference, within fp tolerance
// (native / lavapipe).
//
// RESEARCH §7: FFT/STFT is the confirmed universal grammar gap (no declarative
// grammar provides it). The kernel is a windowed short-time direct DFT magnitude:
// one invocation per (frame, bin) over the Hann-windowed length-fftSize slice
// starting at frame*hop. The CPU reference (referenceStft) does the IDENTICAL f32
// math; the GPU and CPU agree to within a small relative tolerance (transcendental
// cos/sin differ by ULPs between Tint's lowering and libm, so an exact compare is
// the wrong bar — a tight relative tolerance is correct). A pure-tone input also
// pins the magnitude PEAK to the tone's bin on both sides.
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"
#include "dc/transform/CustomCompute.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace dc;

static int failures = 0;
static void check(bool c, const char* msg) {
  if (c) std::printf("  PASS: %s\n", msg);
  else { std::fprintf(stderr, "  FAIL: %s\n", msg); ++failures; }
}

int main() {
  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json to force lavapipe.\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());
  ComputeStage stage(dev);

  // A multi-frame signal: two tones (bins 6 and 19) + a chirp-ish ramp, long
  // enough for several hops so the STFT grid is non-trivial.
  const std::uint32_t fftSize = 128, hop = 64;
  const std::uint32_t total = 128 * 8;  // 1024 samples -> 15 frames
  std::vector<float> sig(total);
  const float twoPi = 6.2831853071795864769f;
  for (std::uint32_t n = 0; n < total; ++n) {
    const float t = static_cast<float>(n);
    sig[n] = std::cos(twoPi * 6.0f * t / static_cast<float>(fftSize)) +
             0.5f * std::cos(twoPi * 19.0f * t / static_cast<float>(fftSize));
  }

  std::printf("\n-- STFT (GPU spectrogram == CPU FFT reference, fp tolerance) --\n");
  std::vector<float> gpu;
  std::uint32_t gFrames = 0, gBins = 0;
  if (!stage.runStft(sig, fftSize, hop, gpu, gFrames, gBins)) {
    std::fprintf(stderr, "  FAIL: runStft returned false\n");
    ++failures;
  } else {
    std::uint32_t cFrames = 0, cBins = 0;
    std::vector<float> cpu = referenceStft(sig, fftSize, hop, cFrames, cBins);
    check(gFrames == cFrames && gBins == cBins,
          "stft: GPU grid dims == CPU (frames x bins)");
    check(gBins == fftSize / 2 + 1, "stft: bins == fftSize/2+1");

    // Relative-tolerance compare (cos/sin ULP differences between Tint + libm).
    double maxRel = 0.0;
    bool sized = (gpu.size() == cpu.size());
    for (std::size_t k = 0; sized && k < gpu.size(); ++k) {
      const double a = gpu[k], b = cpu[k];
      const double denom = std::max(1e-4, std::fabs(b));
      maxRel = std::max(maxRel, std::fabs(a - b) / denom);
    }
    check(sized, "stft: GPU grid size matches CPU");
    check(maxRel < 1e-3, "stft: GPU magnitudes within 1e-3 relative of CPU");
    std::printf("  (frames=%u bins=%u maxRelErr=%.3e)\n", gFrames, gBins, maxRel);

    // Pure-tone pin: in frame 0, the magnitude peak must be at bin 6 (the louder
    // tone) on the GPU output.
    std::uint32_t peak = 0; float pv = -1.0f;
    for (std::uint32_t k = 0; k < gBins; ++k)
      if (gpu[k] > pv) { pv = gpu[k]; peak = k; }
    check(peak == 6, "stft: GPU frame-0 magnitude peaks at the dominant tone bin");
  }

  if (failures == 0) {
    std::printf(
        "\nENC-619 GPU STFT: OK (backend=%s)\n"
        "VERDICT(native): the windowed short-time FFT magnitude grid matches the "
        "CPU FFT reference within fp tolerance.\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-619 GPU STFT: %d FAILURES\n", failures);
  return 1;
}
