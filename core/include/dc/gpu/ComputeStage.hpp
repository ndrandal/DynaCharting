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
