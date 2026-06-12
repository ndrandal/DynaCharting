// ENC-617d (Epic ENC-617) — 2D KDE splat-accumulate compute pass on Dawn:
// BYTE-IDENTICAL to a CPU splat reference, then tone-mapped to color (native /
// lavapipe).
//
// The functional proof of RESEARCH §5.1's "kde/density (2D) -> GPU compute
// (splat-accumulate) -> prime GPU case for density-heatmap". Each point is
// scattered into a grid accumulating a gaussian kernel; float atomics are absent
// so the GPU accumulates a FIXED-POINT u32 (round(w*scale) via atomicAdd) and the
// host divides by the same scale. The CPU reference applies the IDENTICAL per-cell
// quantization, so the read-back density field is bit-equal. A SequentialColorScale
// then tone-maps the density to a colored output (the density-heatmap sink).
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"
#include "dc/scale/ColorScale.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace dc;

static int failures = 0;
static void check(bool c, const char* msg) {
  if (c) {
    std::printf("  PASS: %s\n", msg);
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", msg);
    ++failures;
  }
}

// CPU reference: the SAME splat + fixed-point quantization the WGSL kernel does.
// Accumulates round(w*scale) as u32 per cell, then divides by scale -> f32.
static std::vector<float> cpuKde(const std::vector<float>& px,
                                 const std::vector<float>& py,
                                 const ComputeStage::KdeGrid& g) {
  const std::size_t cells = static_cast<std::size_t>(g.width) * g.height;
  std::vector<std::uint32_t> acc(cells, 0u);
  const float invCellW = 1.0f / g.cellW;
  const float invCellH = 1.0f / g.cellH;
  const float invTwoSigmaSq = 1.0f / (2.0f * g.sigma * g.sigma);
  const float scale = kKdeFixedScale;
  const int r = static_cast<int>(g.radius);
  for (std::size_t p = 0; p < px.size(); ++p) {
    const float cx = (px[p] - g.x0) * invCellW;
    const float cy = (py[p] - g.y0) * invCellH;
    const int ci = static_cast<int>(std::floor(cx));
    const int cj = static_cast<int>(std::floor(cy));
    for (int dj = -r; dj <= r; ++dj) {
      for (int di = -r; di <= r; ++di) {
        const int gx = ci + di;
        const int gy = cj + dj;
        if (gx < 0 || gy < 0 || gx >= static_cast<int>(g.width) ||
            gy >= static_cast<int>(g.height)) {
          continue;
        }
        const float ddx = (static_cast<float>(gx) + 0.5f) - cx;
        const float ddy = (static_cast<float>(gy) + 0.5f) - cy;
        const float r2 = ddx * ddx + ddy * ddy;
        const float w = std::exp(-r2 * invTwoSigmaSq);
        // WGSL round() is round-half-away-from-zero; w*scale >= 0 here so
        // std::round matches it exactly.
        const std::uint32_t q =
            static_cast<std::uint32_t>(std::round(w * scale));
        acc[static_cast<std::size_t>(gy) * g.width +
            static_cast<std::size_t>(gx)] += q;
      }
    }
  }
  std::vector<float> density(cells, 0.0f);
  for (std::size_t k = 0; k < cells; ++k) {
    density[k] = static_cast<float>(acc[k]) / scale;
  }
  return density;
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

  // A 32x24 density grid over a [0,32)x[0,24) world; 600 points (not a multiple
  // of 64) clustered so several splats overlap (the accumulation path matters).
  ComputeStage::KdeGrid grid;
  grid.width = 32;
  grid.height = 24;
  grid.x0 = 0.0f;
  grid.y0 = 0.0f;
  grid.cellW = 1.0f;
  grid.cellH = 1.0f;
  grid.sigma = 1.5f;
  grid.radius = 3;

  const std::uint32_t kN = 600;
  std::vector<float> px(kN), py(kN);
  for (std::uint32_t i = 0; i < kN; ++i) {
    // Two clusters + a sweep, all exactly representable in f32.
    const float a = static_cast<float>(i % 13) * 0.5f;
    const float b = static_cast<float>(i % 7) * 0.5f;
    px[i] = ((i % 2 == 0) ? 8.0f : 22.0f) + a;
    py[i] = ((i % 3 == 0) ? 6.0f : 16.0f) + b;
  }

  std::printf("\n-- KDE SPLAT (GPU density field == CPU reference, bit-exact) --\n");
  std::vector<float> gpu;
  if (!stage.runKde(px, py, grid, gpu)) {
    std::fprintf(stderr, "  FAIL: runKde returned false\n");
    ++failures;
  } else {
    std::vector<float> cpu = cpuKde(px, py, grid);
    bool ok = (gpu.size() == cpu.size());
    std::size_t nonZero = 0;
    for (std::size_t k = 0; ok && k < gpu.size(); ++k) {
      if (cpu[k] != 0.0f) ++nonZero;
      if (std::memcmp(&gpu[k], &cpu[k], sizeof(float)) != 0) {
        std::fprintf(stderr, "    cell %zu: cpu=%.9g gpu=%.9g\n", k, cpu[k],
                     gpu[k]);
        ok = false;
      }
    }
    check(ok, "kde: density field bit-equal to CPU splat");
    check(nonZero > 0, "kde: density field is non-trivial (splats landed)");

    // Tone-map the density to color via a SequentialColorScale (the heatmap
    // sink). The scale auto-domains over the field; we just sanity-check that the
    // peak cell maps to a hotter ramp parameter than an empty cell.
    float peak = 0.0f;
    for (float d : gpu) peak = std::max(peak, d);
    SequentialColorScale colorScale(ColorRamp::viridis());
    colorScale.setDomain(0.0, static_cast<double>(peak));
    const double tPeak = colorScale.map(static_cast<double>(peak));
    const double tZero = colorScale.map(0.0);
    check(tPeak > tZero, "kde: tone-map peak density hotter than empty");
    std::printf("  (peak density=%.6g, t(peak)=%.4f, t(0)=%.4f)\n", peak, tPeak,
                tZero);
  }

  if (failures == 0) {
    std::printf(
        "\nENC-617d GPU KDE: OK (backend=%s)\n"
        "VERDICT(native): the 2D splat-accumulate density field is "
        "BYTE-IDENTICAL to the CPU reference and tone-maps to color.\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-617d GPU KDE: %d FAILURES\n", failures);
  return 1;
}
