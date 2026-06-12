// ENC-617b/c/d (Epic ENC-617) — WGSL kernel-builder unit test (GPU-FREE).
//
// The aggregate/bin/scan/KDE kernel builders are testable WITHOUT a GPU: emit
// each kernel and assert the generated WGSL has the right storage bindings,
// atomics, and structure (in particular that EVERY declared binding is touched
// so Dawn's auto bind-group layout does not prune it — the ENC-617a all-zeros
// lesson). The byte-identical GPU==CPU proofs are the Dawn tests
// (dc_enc617b_dawn_aggregate_bin / dc_enc617c_dawn_scan_stack /
// dc_enc617d_dawn_kde).
#include "dc/transform/ComputeWgsl.hpp"

#include <cstdio>
#include <string>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

using namespace dc;

static bool has(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

int main() {
  std::printf("=== ENC-617b/c/d compute-WGSL kernel builders ===\n");

  // ----- 617b aggregate: parallel reduction with per-op reducers -----------
  {
    std::string sum = buildAggregateKernelWgsl(GpuAggOp::Sum);
    check(has(sum, "@binding(0)") && has(sum, "keyc : array<i32>"),
          "aggregate: key binding 0 is i32");
    check(has(sum, "array<atomic<u32>>"), "aggregate: u32 atomic accumulator");
    check(has(sum, "atomicAdd(&outAcc"), "aggregate sum: atomicAdd on outAcc");
    check(has(sum, "atomicAdd(&outCount"), "aggregate: count always maintained");
    check(has(sum, "arrayLength(&measure)"),
          "aggregate: measure binding kept alive");

    std::string mn = buildAggregateKernelWgsl(GpuAggOp::Min);
    check(has(mn, "atomicMin(&outAcc"), "aggregate min: atomicMin");
    std::string mx = buildAggregateKernelWgsl(GpuAggOp::Max);
    check(has(mx, "atomicMax(&outAcc"), "aggregate max: atomicMax");
    std::string cnt = buildAggregateKernelWgsl(GpuAggOp::Count);
    // Count touches outAcc (atomicAdd 0u) so its binding survives pruning.
    check(has(cnt, "atomicAdd(&outAcc[g], 0u)"),
          "aggregate count: outAcc kept alive");
  }

  // ----- 617b bin: atomic-u32 histogram ------------------------------------
  {
    std::string bin = buildBinHistogramKernelWgsl();
    check(has(bin, "hist : array<atomic<u32>>"), "bin: u32 atomic histogram");
    check(has(bin, "atomicAdd(&hist["), "bin: atomicAdd into the bin");
    // The floor/clamp must mirror BinTransform exactly.
    check(has(bin, "floor((v - params.firstEdge) / params.step)"),
          "bin: floor((v-firstEdge)/step)");
    check(has(bin, "max(0, min(b, i32(params.binCount) - 1))"),
          "bin: clamp to [0, count-1]");
    check(has(bin, "v == v") && has(bin, "3.4028235e38f"),
          "bin: finite guard (nan/inf -> bin 0)");
  }

  // ----- 617c stack: work-efficient prefix-sum scan ------------------------
  {
    std::string scan = buildScanKernelWgsl(256u);
    check(has(scan, "var<workgroup> temp : array<f32, 512>"),
          "scan: shared array sized 2*wg");
    check(has(scan, "@workgroup_size(256)"), "scan: workgroup size 256");
    check(has(scan, "workgroupBarrier()"), "scan: barriers between phases");
    // Inclusive band: y0 = exclusive prefix, y1 = y0 + value.
    check(has(scan, "y0[ai] = temp[ai]") && has(scan, "y1[ai] = temp[ai] + value[ai]"),
          "scan: y0=exclusive, y1=y0+value");
    check(has(scan, "select(0.0f, value[ai], ai < n)"),
          "scan: 0-pad the tail past rowCount");
  }

  // ----- 617d KDE: splat-accumulate ----------------------------------------
  {
    std::string kde = buildKdeSplatKernelWgsl();
    check(has(kde, "grid : array<atomic<u32>>"),
          "kde: fixed-point u32 atomic grid");
    check(has(kde, "exp(-r2 * params.invTwoSigmaSq)"),
          "kde: gaussian kernel weight");
    check(has(kde, "atomicAdd(&grid[idx]"), "kde: atomicAdd into the cell");
    check(has(kde, "round(w * params.scale)"), "kde: fixed-point quantization");
    // The window iterates [-radius, +radius]^2 with in-bounds guards.
    check(has(kde, "dj : i32 = -r; dj <= r") && has(kde, "di : i32 = -r; di <= r"),
          "kde: (2*radius+1)^2 window");
  }

  std::printf("\n%d passed, %d failed\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
