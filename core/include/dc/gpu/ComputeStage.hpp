// ENC-617a (Epic ENC-617) — ComputeStage: a reusable transform-DAG COMPUTE
// stage that runs a per-row expression as a WebGPU compute pass (RESEARCH §5.1:
// the GPU execution target for the transform DAG; §5.3: filter/formula's GPU
// fast path). This is the GENERALIZATION of the ENC-591 de-risk spike's one-off
// `data[i] *= 2` round-trip into a column-oriented, reusable layer.
//
// WHAT IT DOES
// ------------
// Given the input columns of a transform (read as f32 — the CPU evaluator's
// f32-domain doubles, narrowed) and a WGSL @compute kernel generated from the
// expression AST (ExprWgsl.hpp), the stage:
//   1. uploads each input column to a read-only storage buffer (createStorageBuffer),
//   2. allocates the output storage buffer,
//   3. compiles + builds the compute pipeline (createComputePipeline) and
//      dispatches ceil(N/64) workgroups (dispatchCompute) — the bindings are
//      col0..col{C-1} then the output, in slot order, exactly as the kernel
//      declares them,
//   4. reads the output buffer back (readBuffer / the map-pump).
// Built ENTIRELY on DawnDevice's ENC-590/591 compute API; it adds NO new device
// entry points — it is the column-oriented USE of that API.
//
// TWO FAST PATHS (the only ones this sub-PR ships; aggregate/scan/KDE are 617b–d)
//   * runFormula  — a Num expression -> an f32 output column (per-row).
//   * runFilter   — a Bool predicate -> the kept row indices, by reading back a
//                   per-row 0/1 mask and compacting CPU-side (the simple-mask
//                   output of RESEARCH §5.1). The survivor set is BYTE-IDENTICAL
//                   to the CPU FilterTransform's evalBool pass.
//
// C++20 (Dawn): lives in dc_gpu (it #includes DawnDevice). The codegen it
// consumes (ExprWgsl) is pure dc and GPU-free; this stage is the GPU half.
#pragma once

#include "dc/gpu/DawnDevice.hpp"
#include "dc/transform/ComputeWgsl.hpp"
#include "dc/transform/Expr.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// ComputeStage — a thin, reusable wrapper around a DawnDevice that runs a
// per-row expression kernel over a set of f32 input columns. Holds no state
// between calls beyond the device reference; each run() owns its buffers.
// ---------------------------------------------------------------------------
class ComputeStage {
 public:
  // The device MUST already be init()'d. The stage borrows it (does not own it).
  explicit ComputeStage(DawnDevice& device) : device_(device) {}

  // Workgroup size baked into the generated kernels (ENC-591 spike value; <= the
  // WebGPU 256 cap). Public so a caller can match its dispatch math / tests.
  static constexpr std::uint32_t kWorkgroupSize = 64;

  // runFormula — execute a formula's compiled Num expression on the GPU.
  //   columns : the input columns IN SLOT ORDER (column k -> binding k). Each is
  //             rowCount f32 values (the CPU path's double-per-row, narrowed).
  //   expr    : a compiled NUM expression (formula's typing gate guarantees Num).
  //   out     : receives rowCount f32 results (out[i] == expr(row i)).
  // Returns false on any device failure (pipeline build / dispatch / readback) or
  // a ragged `columns` (every column must have the same length). On success `out`
  // is byte-identical to FormulaTransform's per-row static_cast<float>(evalNum).
  bool runFormula(const std::vector<std::vector<float>>& columns,
                  const CompiledExpr& expr, std::vector<float>& out);

  // runFilter — execute a filter's compiled Bool predicate on the GPU and return
  // the SURVIVING row indices (ascending), compacted CPU-side from the GPU mask.
  //   columns  : input columns in slot order (as runFormula).
  //   expr     : a compiled BOOL expression (filter's typing gate guarantees Bool).
  //   keepRows : receives the indices i for which the predicate is true, ascending.
  // The kept set is byte-identical to FilterTransform's evalBool survivor pass.
  // Returns false on any device failure or ragged input.
  bool runFilter(const std::vector<std::vector<float>>& columns,
                 const CompiledExpr& expr, std::vector<std::uint32_t>& keepRows);

  // ===== ENC-617b — GPU aggregate (parallel reduction) + bin (atomic-u32) =====

  // runAggregate — GPU parallel reduction over a single integer GROUP-KEY column
  // (keys are the output group index in [0, nGroups)) + one f32 MEASURE column of
  // NON-NEGATIVE INTEGER-valued samples. `op` selects the reducer.
  //   keys     : per-row group index (0..nGroups-1), one per row.
  //   measure  : per-row measure value (ignored for Count; non-negative-int for
  //              Sum/Mean/Min/Max so the u32 reduction is exact).
  //   nGroups  : the number of output groups.
  //   out      : receives nGroups results (Sum/Min/Max/Mean -> f32 value;
  //              Count -> the integer count as f32). BYTE-IDENTICAL to the CPU
  //              AggregateTransform when groups are dense integer keys.
  // Returns false on device failure or a ragged keys/measure length mismatch.
  bool runAggregate(const std::vector<std::int32_t>& keys,
                    const std::vector<float>& measure, GpuAggOp op,
                    std::uint32_t nGroups, std::vector<float>& out);

  // runBin — GPU atomic-u32 HISTOGRAM. Maps each value to a bin index (the same
  // floor/clamp BinTransform uses) and atomicAdds the per-bin count.
  //   values     : the numeric column to bin (one per row).
  //   firstEdge  : the first bin's lower edge (BinSpec.firstEdge).
  //   step       : the bin width (BinSpec.step, > 0).
  //   binCount   : number of bins (BinSpec.count).
  //   counts     : receives binCount per-bin counts (u32). Byte-identical to a
  //                CPU bin -> aggregate(count) over the same spec.
  // Returns false on device failure or an invalid spec (step <= 0, binCount 0).
  bool runBin(const std::vector<float>& values, float firstEdge, float step,
              std::uint32_t binCount, std::vector<std::uint32_t>& counts);

  // ===== ENC-617c — GPU stack via a prefix-sum (Blelloch) scan ==============

  // runStackScan — GPU work-efficient inclusive prefix-sum scan of one f32 value
  // column into the cumulative stack band: y0[i] = sum_{j<i} value[j], y1[i] =
  // y0[i] + value[i] (StackOffset::Zero, single group). Single-workgroup fast
  // path: rowCount must be <= 2*kScanWorkgroupSize.
  //   values : the per-row measure column (input stacking order).
  //   y0,y1  : receive the per-row band (rowCount each). Byte-identical to the
  //            CPU StackTransform::Zero running sum when the partial sums are
  //            f32-exact (e.g. integer-valued measures).
  // Returns false on device failure or rowCount > 2*kScanWorkgroupSize.
  bool runStackScan(const std::vector<float>& values, std::vector<float>& y0,
                    std::vector<float>& y1);

  // The single-workgroup scan size (rows <= 2*this are handled in one dispatch).
  static constexpr std::uint32_t kScanWorkgroupSize = 256;

  // ===== ENC-617d — 2D KDE splat-accumulate ================================

  // KdeGrid — the parameters of the density grid the points splat into.
  struct KdeGrid {
    std::uint32_t width{0};   // grid columns
    std::uint32_t height{0};  // grid rows
    float x0{0.0f};           // world-space lower-left x of the grid
    float y0{0.0f};           // world-space lower-left y of the grid
    float cellW{1.0f};        // world width of one cell
    float cellH{1.0f};        // world height of one cell
    float sigma{1.0f};        // gaussian bandwidth, in CELL units
    std::uint32_t radius{2};  // splat half-window, in cells
  };

  // runKde — 2D KDE splat-accumulate. Scatters each point into `grid`,
  // accumulating a gaussian kernel over a (2*radius+1)^2 cell window, and reads
  // back the density field (width*height f32, row-major). Float atomics are
  // absent, so the GPU accumulates a fixed-point u32 and the host divides by
  // kKdeFixedScale — matching the CPU reference's same quantization.
  //   px,py   : the point coordinates (same length).
  //   grid    : the density-grid parameters.
  //   density : receives width*height f32 density values (row-major; [j*W+i]).
  // Returns false on device failure or a ragged px/py / empty grid.
  bool runKde(const std::vector<float>& px, const std::vector<float>& py,
              const KdeGrid& grid, std::vector<float>& density);

 private:
  // Shared core: upload `columns` + an output buffer of `outBytes`, compile
  // `wgsl`, dispatch ceil(N/64) groups, read the output back into `outBytes` of
  // `outData`. Returns false on any failure / ragged input. `rowCount` is the
  // common column length (0 columns => caller must pass it explicitly is N/A here
  // since filter/formula always have >=1 input column on this path).
  bool dispatchKernel(const std::vector<std::vector<float>>& columns,
                      const std::string& wgsl, std::size_t rowCount,
                      void* outData, std::size_t outBytes);

  DawnDevice& device_;
};

}  // namespace dc
