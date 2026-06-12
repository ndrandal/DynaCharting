// ENC-617c (Epic ENC-617) — GPU stack via a work-efficient prefix-sum SCAN on
// Dawn: BYTE-IDENTICAL to the CPU StackTransform (native / lavapipe).
//
// The functional proof of RESEARCH §5.1's "stack -> GPU decoupled-lookback scan":
// a single-workgroup work-efficient (Blelloch) inclusive prefix-sum turns a
// per-row measure column into the cumulative stack band y0[i]=sum_{j<i}value[j],
// y1[i]=y0[i]+value[i] — exactly StackTransform's StackOffset::Zero running sum
// (single global group). With integer-valued measures the partial sums are
// f32-exact, so the GPU band is bit-equal to the CPU band.
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"

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

static bool bitEqual(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::memcmp(&a[i], &b[i], sizeof(float)) != 0) return false;
  }
  return true;
}

// CPU reference: the StackTransform::Zero running sum (single group).
static void cpuStack(const std::vector<float>& v, std::vector<float>& y0,
                     std::vector<float>& y1) {
  y0.assign(v.size(), 0.0f);
  y1.assign(v.size(), 0.0f);
  double running = 0.0;
  for (std::size_t i = 0; i < v.size(); ++i) {
    const double base = running;
    y0[i] = static_cast<float>(base);
    y1[i] = static_cast<float>(base + v[i]);
    running = base + v[i];
  }
}

static void testScan(ComputeStage& stage, const std::vector<float>& v,
                     const char* label) {
  std::vector<float> gy0, gy1;
  if (!stage.runStackScan(v, gy0, gy1)) {
    std::fprintf(stderr, "  FAIL: runStackScan(%s) returned false\n", label);
    ++failures;
    return;
  }
  std::vector<float> cy0, cy1;
  cpuStack(v, cy0, cy1);
  bool ok = bitEqual(gy0, cy0) && bitEqual(gy1, cy1);
  if (!ok) {
    for (std::size_t i = 0; i < v.size(); ++i) {
      if (std::memcmp(&gy1[i], &cy1[i], sizeof(float)) != 0) {
        std::fprintf(stderr,
                     "    row %zu: cpu y1=%.9g gpu y1=%.9g (%s)\n", i, cy1[i],
                     gy1[i], label);
        break;
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

  std::printf("\n-- STACK SCAN (GPU prefix-sum == CPU running sum, bit-exact) --\n");

  // A handful of row counts (incl. non-power-of-two, and the exact 2*wg cap),
  // integer-valued measures so the partial sums are f32-exact.
  for (std::uint32_t n : {1u, 7u, 64u, 100u, 256u, 511u, 512u}) {
    std::vector<float> v(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      v[i] = static_cast<float>((i % 9) + 1);  // 1..9
    }
    char label[64];
    std::snprintf(label, sizeof(label), "stack scan: n=%u", n);
    testScan(stage, v, label);
  }

  // A degenerate single row and a row of zeros.
  testScan(stage, {5.0f}, "stack scan: single row");
  testScan(stage, std::vector<float>(300, 0.0f), "stack scan: all zeros");

  if (failures == 0) {
    std::printf(
        "\nENC-617c GPU stack scan: OK (backend=%s)\n"
        "VERDICT(native): the work-efficient prefix-sum scan produces the stack "
        "band BYTE-IDENTICAL to the CPU StackTransform running sum.\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-617c GPU stack scan: %d FAILURES\n", failures);
  return 1;
}
