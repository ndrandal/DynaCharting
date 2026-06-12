// ENC-617a (Epic ENC-617) — AST -> WGSL codegen: the GPU backend of the
// expression DSL (RESEARCH §5.3: "the same AST compiles to EITHER a WASM-CPU
// tight loop OR a generated WGSL body, so filter/formula get the GPU fast path
// for free").
//
// WHAT THIS IS
// ------------
// A SECOND backend over the SAME `ExprNode` AST that `Expr.cpp`'s CPU evaluator
// walks. Where `evalNum`/`evalBool` interpret the tree per row on the CPU, this
// pass EMITS a WGSL expression string from the tree, plus a complete `@compute`
// kernel that runs that expression over every row of a set of storage-buffer
// columns. filter (a Bool predicate -> a per-row mask) and formula (a Num
// expression -> a derived column) both compile through here.
//
// THE WEBGPU CONTRACT (RESEARCH §5.1)
// -----------------------------------
//   * f32 ONLY — WebGPU has no f64. Every column ref is an `array<f32>` load and
//     every literal is emitted with an `f` suffix. The CPU path evaluates in
//     double then narrows to f32 at the formula sink (`static_cast<float>`), so a
//     formula whose ops are exactly reproducible in f32 (arithmetic, min/max,
//     abs, floor/ceil/round, comparisons, …) is BYTE-IDENTICAL on both paths.
//     Epoch-ms timestamps are pre-normalized upstream (never fed as f32 here).
//   * workgroup_size <= 256 — we emit @workgroup_size(64) (the ENC-591 spike
//     value), well under the cap; the kernel bounds-guards on arrayLength so the
//     dispatch can round the workgroup count up.
//   * the ~25 math fns map to WGSL intrinsics (abs/sqrt/exp/log/log2/sin/…); the
//     few without a direct intrinsic (cbrt, log10, sign-as-int, isnan/isfinite,
//     N-ary min/max) are emitted as exact WGSL equivalents (see emitCall).
//
// PURE `dc` (C++17, NO Dawn): this is string generation over the AST — it builds
// no GPU resource. The compute STAGE that uploads/dispatches/reads the generated
// kernel lives in dc_gpu (ComputeStage, which #includes Dawn). Keeping codegen
// here makes it unit-testable WITHOUT a GPU: assert the emitted WGSL string.
#pragma once

#include "dc/transform/Expr.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// emitExprWgsl — emit the WGSL expression text for `node` (the GPU mirror of
// evalNum/evalBool). Column refs become `col{slot}[i]` loads (the i-th element of
// the slot's storage buffer); a Num node yields an f32-typed WGSL sub-expression,
// a Bool node a bool-typed one (matching the AST's `kind`). Fully parenthesized,
// so it is precedence-safe to embed anywhere. No surrounding kernel — just the
// expression. `i` is the row-index identifier the caller's kernel defines.
std::string emitExprWgsl(const ExprNode& node);

// ---------------------------------------------------------------------------
// Full @compute kernel builders. Both bind their input columns as
// `array<f32>` storage buffers at @group(0) @binding(0..C-1) IN SLOT ORDER (so
// binding k == column slot k == the k-th input column the compute stage uploads),
// plus their output at @binding(C). They @workgroup_size(64) and bounds-guard on
// the row count. The emitted WGSL is what ComputeStage compiles + dispatches.
// ---------------------------------------------------------------------------

// buildFormulaKernelWgsl — a per-row Num expression -> a derived f32 column.
//   inputs : columns 0..numColumns-1  (array<f32>, read-only)
//   output : binding numColumns        (array<f32>, write)
// out[i] = <expr>(row i). `expr.root` MUST be a Num expression (formula's typing
// gate guarantees it). Mirrors FormulaTransform::evaluate's per-row evalNum.
std::string buildFormulaKernelWgsl(const CompiledExpr& expr,
                                   std::uint32_t numColumns);

// buildFilterMaskKernelWgsl — a per-row Bool predicate -> a per-row 0/1 mask.
//   inputs : columns 0..numColumns-1  (array<f32>, read-only)
//   output : binding numColumns        (array<u32>, write) — mask[i] = 1 kept / 0
// `expr.root` MUST be a Bool expression (filter's typing gate guarantees it).
// The compute stage reads the mask back and compacts survivors CPU-side (the
// simple-mask output of RESEARCH §5.1; prefix-sum compaction is a later sub-PR),
// so the kept set is BYTE-IDENTICAL to FilterTransform::evaluate's evalBool pass.
std::string buildFilterMaskKernelWgsl(const CompiledExpr& expr,
                                      std::uint32_t numColumns);

}  // namespace dc
