// ENC-619 (Epic ENC-619) — the shipped marching-squares escape-hatch kernel on
// Dawn: GPU iso-line segments == the CPU marching-squares reference on a known
// field (native / lavapipe).
//
// RESEARCH §7.2: topology extraction (variable cardinality) — marching-squares
// produces a DATA-DEPENDENT number of segments (0..2 per cell), so it rides the
// engine-owned MAX-BOUNDED output buffer + ATOMIC COUNTER + compaction path. The
// GPU claims segment slots via atomicAdd (arbitrary cross-cell ORDER) and writes
// 4 f32 (x0,y0,x1,y1) each; the host reads the counter and compacts to the live
// prefix. The CPU reference (referenceMarchingSquares) emits the SAME segment set
// (same case table + saddle disambiguation), so a SORTED comparison matches
// bit-for-bit (the f32 lerp + comparisons are exact on both sides — no
// transcendentals). The variable cardinality is the whole point: the segment count
// is not knowable until the field is scanned.
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"
#include "dc/transform/CustomCompute.hpp"

#include <algorithm>
#include <array>
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

// Canonicalize a segment (order-insensitive endpoints) and sort the set, so the
// GPU's arbitrary atomic ordering compares equal to the CPU scan order.
using Seg4 = std::array<float, 4>;
static Seg4 canon(float x0, float y0, float x1, float y1) {
  // Order the two endpoints lexicographically so (A,B) == (B,A).
  if (x1 < x0 || (x1 == x0 && y1 < y0)) return {x1, y1, x0, y0};
  return {x0, y0, x1, y1};
}
static bool less4(const Seg4& a, const Seg4& b) {
  for (int i = 0; i < 4; ++i) {
    if (a[i] < b[i]) return true;
    if (a[i] > b[i]) return false;
  }
  return false;
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

  // A known scalar field: a radial bump centered in a 24x18 grid. The iso-contour
  // at a mid-level is a closed loop crossing many cells (data-dependent count).
  const std::uint32_t W = 24, H = 18;
  std::vector<float> field(static_cast<std::size_t>(W) * H);
  const float cx = 11.0f, cy = 8.0f;
  for (std::uint32_t r = 0; r < H; ++r) {
    for (std::uint32_t c = 0; c < W; ++c) {
      const float dx = static_cast<float>(c) - cx;
      const float dy = static_cast<float>(r) - cy;
      // A smooth bump (exactly representable enough; lerp/compare are exact f32).
      field[r * W + c] = 1.0f / (1.0f + 0.05f * (dx * dx + dy * dy));
    }
  }
  const float iso = 0.4f;
  const std::uint32_t cap = 4096;

  std::printf("\n-- MARCHING SQUARES (GPU iso-lines == CPU reference) --\n");
  std::vector<float> gpuSegs;
  std::uint32_t gpuCount = 0;
  if (!stage.runMarchingSquares(field, W, H, iso, cap, gpuSegs, gpuCount)) {
    std::fprintf(stderr, "  FAIL: runMarchingSquares returned false\n");
    ++failures;
  } else {
    auto cpu = referenceMarchingSquares(field, W, H, iso);
    check(gpuCount == cpu.size(),
          "marching: GPU atomic count == CPU segment count (variable cardinality)");
    check(gpuCount <= cap, "marching: count within the declared cap");
    check(gpuSegs.size() == static_cast<std::size_t>(gpuCount) * 4u,
          "marching: compacted buffer holds exactly count*4 f32");
    check(!cpu.empty(), "marching: the contour is non-trivial");

    // Canonicalize + sort both sides, then assert bit-equality.
    std::vector<Seg4> g, c;
    for (std::uint32_t i = 0; i < gpuCount; ++i)
      g.push_back(canon(gpuSegs[i * 4 + 0], gpuSegs[i * 4 + 1],
                        gpuSegs[i * 4 + 2], gpuSegs[i * 4 + 3]));
    for (const auto& s : cpu) c.push_back(canon(s.x0, s.y0, s.x1, s.y1));
    std::sort(g.begin(), g.end(), less4);
    std::sort(c.begin(), c.end(), less4);
    bool eq = (g.size() == c.size());
    for (std::size_t k = 0; eq && k < g.size(); ++k)
      for (int j = 0; j < 4; ++j)
        if (g[k][j] != c[k][j]) eq = false;
    check(eq, "marching: GPU iso-line set == CPU reference (sorted, bit-exact)");
    std::printf("  (iso=%.2f -> %u segments, gpu==cpu)\n", iso, gpuCount);
  }

  // Cap clamp: a tiny cap forces overflow; count reports the TRUE total but the
  // buffer holds only `cap` segments (the bounded-buffer compaction).
  {
    std::vector<float> segs;
    std::uint32_t count = 0;
    const std::uint32_t tinyCap = 4;
    if (stage.runMarchingSquares(field, W, H, iso, tinyCap, segs, count)) {
      check(count > tinyCap, "marching: overflow -> count exceeds cap");
      check(segs.size() == static_cast<std::size_t>(tinyCap) * 4u,
            "marching: overflow -> buffer clamped to cap segments");
    } else {
      std::fprintf(stderr, "  FAIL: runMarchingSquares (tiny cap) returned false\n");
      ++failures;
    }
  }

  if (failures == 0) {
    std::printf(
        "\nENC-619 GPU marching-squares: OK (backend=%s)\n"
        "VERDICT(native): the per-cell iso-line extraction (variable cardinality, "
        "bounded buffer + atomic count + compaction) matches the CPU reference.\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-619 GPU marching-squares: %d FAILURES\n", failures);
  return 1;
}
