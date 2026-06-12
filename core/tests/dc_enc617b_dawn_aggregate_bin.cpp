// ENC-617b (Epic ENC-617) — GPU aggregate (parallel reduction) + bin (atomic-u32
// histogram) on Dawn: BYTE-IDENTICAL to the CPU transforms (native / lavapipe).
//
// The functional proof that the GPU fast paths of RESEARCH §5.1 ("aggregate ->
// parallel reduction", "bin -> atomic-u32 histogram") produce results IDENTICAL
// to the CPU AggregateTransform / BinTransform. Both run through the reusable
// ENC-617a ComputeStage (upload columns -> dispatch a kernel -> read back).
//
//   * aggregate : a dense integer GROUP-KEY column (the realistic large-N case:
//     a bin index feeding aggregate) + a NON-NEGATIVE INTEGER measure, so the
//     u32 reduction is EXACT. GPU runAggregate(sum/mean/min/max/count) is
//     compared cell-for-cell to a CPU reference that mirrors the reducer.
//   * bin       : GPU runBin (the atomic-u32 histogram over the SAME BinSpec the
//     CPU BinTransform resolves) is compared to a CPU bin -> count over the same
//     floor/clamp assignment.
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
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

// CPU reference for the GPU aggregate (dense integer keys, integer measure).
static std::vector<float> cpuAggregate(const std::vector<std::int32_t>& keys,
                                       const std::vector<float>& measure,
                                       GpuAggOp op, std::uint32_t nGroups) {
  std::vector<double> sum(nGroups, 0.0);
  std::vector<std::uint32_t> count(nGroups, 0u);
  std::vector<double> mn(nGroups, std::numeric_limits<double>::infinity());
  std::vector<double> mx(nGroups, -std::numeric_limits<double>::infinity());
  for (std::size_t i = 0; i < keys.size(); ++i) {
    const std::uint32_t g = static_cast<std::uint32_t>(keys[i]);
    ++count[g];
    const double v = measure[i];
    sum[g] += v;
    mn[g] = std::min(mn[g], v);
    mx[g] = std::max(mx[g], v);
  }
  std::vector<float> out(nGroups, 0.0f);
  for (std::uint32_t g = 0; g < nGroups; ++g) {
    switch (op) {
      case GpuAggOp::Count: out[g] = static_cast<float>(count[g]); break;
      case GpuAggOp::Sum: out[g] = static_cast<float>(sum[g]); break;
      case GpuAggOp::Mean:
        out[g] = count[g] > 0u
                     ? static_cast<float>(sum[g] / static_cast<double>(count[g]))
                     : 0.0f;
        break;
      case GpuAggOp::Min:
        out[g] = count[g] > 0u ? static_cast<float>(mn[g]) : 0.0f;
        break;
      case GpuAggOp::Max:
        out[g] = count[g] > 0u ? static_cast<float>(mx[g]) : 0.0f;
        break;
    }
  }
  return out;
}

static bool bitEqual(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::memcmp(&a[i], &b[i], sizeof(float)) != 0) return false;
  }
  return true;
}

static void testAgg(ComputeStage& stage, const std::vector<std::int32_t>& keys,
                    const std::vector<float>& measure, GpuAggOp op,
                    std::uint32_t nGroups, const char* label) {
  std::vector<float> gpu;
  if (!stage.runAggregate(keys, measure, op, nGroups, gpu)) {
    std::fprintf(stderr, "  FAIL: runAggregate(%s) returned false\n", label);
    ++failures;
    return;
  }
  std::vector<float> cpu = cpuAggregate(keys, measure, op, nGroups);
  bool ok = bitEqual(gpu, cpu);
  if (!ok) {
    for (std::uint32_t g = 0; g < nGroups; ++g) {
      if (std::memcmp(&gpu[g], &cpu[g], sizeof(float)) != 0) {
        std::fprintf(stderr, "    group %u: cpu=%.9g gpu=%.9g (%s)\n", g, cpu[g],
                     gpu[g], label);
      }
    }
  }
  check(ok, label);
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

  // --- aggregate: N not a multiple of 64; dense keys 0..nGroups-1; integer
  // measure so the u32 reduction is byte-identical to the f64 CPU path. ------
  const std::uint32_t kN = 1000;
  const std::uint32_t kGroups = 8;
  std::vector<std::int32_t> keys(kN);
  std::vector<float> measure(kN);
  for (std::uint32_t i = 0; i < kN; ++i) {
    keys[i] = static_cast<std::int32_t>(i % kGroups);
    measure[i] = static_cast<float>((i % 50) + 1);  // 1..50, non-negative ints
  }

  std::printf("\n-- AGGREGATE (GPU parallel reduction == CPU, bit-exact) --\n");
  testAgg(stage, keys, measure, GpuAggOp::Count, kGroups, "aggregate: count");
  testAgg(stage, keys, measure, GpuAggOp::Sum, kGroups, "aggregate: sum");
  testAgg(stage, keys, measure, GpuAggOp::Mean, kGroups, "aggregate: mean");
  testAgg(stage, keys, measure, GpuAggOp::Min, kGroups, "aggregate: min");
  testAgg(stage, keys, measure, GpuAggOp::Max, kGroups, "aggregate: max");

  // --- bin: atomic-u32 histogram over a fixed BinSpec. CPU reference applies
  // the SAME floor/clamp assignment BinTransform uses, then counts. ----------
  std::printf("\n-- BIN (GPU atomic-u32 histogram == CPU bin->count) --\n");
  {
    const float firstEdge = 0.0f;
    const float step = 2.0f;
    const std::uint32_t binCount = 10;  // covers [0, 20)
    std::vector<float> values(kN);
    for (std::uint32_t i = 0; i < kN; ++i) {
      values[i] = static_cast<float>(i % 25) * 0.9f;  // spread across bins
    }
    std::vector<std::uint32_t> gpu;
    bool ran = stage.runBin(values, firstEdge, step, binCount, gpu);
    if (!ran) {
      std::fprintf(stderr, "  FAIL: runBin returned false\n");
      ++failures;
    } else {
      std::vector<std::uint32_t> cpu(binCount, 0u);
      for (float v : values) {
        int b = 0;
        if (std::isfinite(v)) {
          b = static_cast<int>(std::floor((v - firstEdge) / step));
          b = std::max(0, std::min(b, static_cast<int>(binCount) - 1));
        }
        ++cpu[static_cast<std::size_t>(b)];
      }
      check(gpu == cpu, "bin: per-bin counts equal CPU");
    }
  }

  if (failures == 0) {
    std::printf(
        "\nENC-617b GPU aggregate+bin: OK (backend=%s)\n"
        "VERDICT(native): parallel-reduction aggregate + atomic-u32 bin run "
        "BYTE-IDENTICAL to the CPU transforms.\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-617b GPU aggregate+bin: %d FAILURES\n", failures);
  return 1;
}
