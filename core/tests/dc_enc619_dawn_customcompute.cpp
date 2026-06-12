// ENC-619 (Epic ENC-619, the FINAL epic) — the customCompute escape hatch on Dawn.
//
// Two proofs (native / lavapipe):
//   1. A TRIVIAL author-supplied custom kernel RUNS end-to-end through
//      ComputeStage::runCustomCompute — a sandbox-validated CustomComputeSpec with
//      one input + one output, dispatched and read back, with the right values.
//      This is the §5.3 escape hatch working: I/O only through declared storage
//      bindings, Tint-validated at pipeline creation.
//   2. A SANDBOX/WGSL-VIOLATING kernel is REJECTED — a kernel with malformed WGSL
//      fails pipeline creation, surfaces via lastDeviceError(), and runCustomCompute
//      returns false (no partial render — the §5.3 "reject the node" semantics).
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"
#include "dc/transform/CustomCompute.hpp"

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

  // ---------- 1. a trivial custom kernel runs (out[i] = a[i]*2 + 1) --------
  {
    const std::uint32_t N = 200;  // not a multiple of 64
    std::vector<float> a(N);
    for (std::uint32_t i = 0; i < N; ++i) a[i] = static_cast<float>(i);

    CustomComputeSpec spec;
    spec.id = "doubleplus";
    // The GENERAL runCustomCompute path uploads f32 inputs + f32 outputs (the row
    // count is the input length, read via arrayLength — no params buffer needed).
    spec.wgsl =
        "@group(0) @binding(0) var<storage, read> a : array<f32>;\n"
        "@group(0) @binding(1) var<storage, read_write> o : array<f32>;\n"
        "@compute @workgroup_size(64)\n"
        "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n"
        "  let i = gid.x;\n"
        "  if (i >= arrayLength(&a)) { return; }\n"
        "  o[i] = a[i] * 2.0f + 1.0f;\n"
        "}\n";
    spec.entryPoint = "main";
    spec.workgroupX = 64;
    spec.dispatchX = (N + 63u) / 64u;
    CcBinding in; in.binding = 0; in.name = "a"; in.column = "a";
    in.dtype = CcDType::F32; in.access = CcAccess::Read;
    CcBinding out; out.binding = 1; out.name = "o"; out.dtype = CcDType::F32;
    out.access = CcAccess::Write; out.capElements = N;
    spec.inputs = {in};
    spec.outputs = {out};

    std::vector<std::vector<float>> outputs;
    std::vector<std::uint32_t> counts;
    const bool ok = stage.runCustomCompute(spec, {a}, outputs, counts);
    check(ok, "custom kernel: runCustomCompute succeeded");
    if (ok) {
      bool bitEq = (outputs.size() == 1 && outputs[0].size() == N);
      for (std::uint32_t i = 0; bitEq && i < N; ++i) {
        const float want = a[i] * 2.0f + 1.0f;
        if (outputs[0][i] != want) {
          std::fprintf(stderr, "    row %u: got %.9g want %.9g\n", i,
                       outputs[0][i], want);
          bitEq = false;
        }
      }
      check(bitEq, "custom kernel: output == a*2+1 bit-exact");
      check(counts.size() == 1 && counts[0] == 0,
            "custom kernel: fixed-cardinality output has no atomic count");
    }
  }

  // ---------- 2. a WGSL-violating kernel is rejected via lastDeviceError ----
  {
    CustomComputeSpec spec;
    spec.id = "broken";
    // Reference an undeclared identifier -> Tint rejects at pipeline creation.
    spec.wgsl =
        "@group(0) @binding(0) var<storage, read> a : array<f32>;\n"
        "@group(0) @binding(1) var<storage, read_write> o : array<f32>;\n"
        "@compute @workgroup_size(64)\n"
        "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n"
        "  o[gid.x] = totally_undeclared_symbol + a[gid.x];\n"
        "}\n";
    spec.workgroupX = 64;
    spec.dispatchX = 1;
    CcBinding in; in.binding = 0; in.name = "a"; in.dtype = CcDType::F32;
    in.access = CcAccess::Read;
    CcBinding out; out.binding = 1; out.name = "o"; out.dtype = CcDType::F32;
    out.access = CcAccess::Write; out.capElements = 64;
    spec.inputs = {in};
    spec.outputs = {out};

    std::vector<float> a(64, 1.0f);
    std::vector<std::vector<float>> outputs;
    std::vector<std::uint32_t> counts;
    const bool ok = stage.runCustomCompute(spec, {a}, outputs, counts);
    check(!ok, "broken kernel: runCustomCompute returns false (node rejected)");
    check(!dev.lastDeviceError().empty(),
          "broken kernel: Tint rejection surfaced via lastDeviceError()");
    std::printf("  (lastDeviceError: %.120s)\n", dev.lastDeviceError().c_str());
  }

  if (failures == 0) {
    std::printf(
        "\nENC-619 customCompute: OK (backend=%s)\n"
        "VERDICT(native): an author custom WGSL kernel runs through the sandbox "
        "and a malformed kernel is REJECTED (no partial render).\n",
        dev.backendName().c_str());
    return 0;
  }
  std::fprintf(stderr, "\nENC-619 customCompute: %d FAILURES\n", failures);
  return 1;
}
