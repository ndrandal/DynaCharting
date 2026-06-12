// ENC-617b/c/d (Epic ENC-617) — WGSL kernel builders for the GPU compute fast
// path of the non-expression transforms (RESEARCH §5.1):
//   * aggregate -> GPU PARALLEL REDUCTION (sum/mean/min/max/count)
//   * bin       -> GPU ATOMIC-u32 HISTOGRAM (WebGPU has integer atomics; no float
//                  atomics, so counts accumulate in u32)
//   * stack     -> GPU DECOUPLED-LOOKBACK / work-efficient PREFIX-SUM SCAN
//   * kde       -> a 2D SPLAT-ACCUMULATE compute pass (the prime GPU density case)
//
// WHY THIS LIVES IN `dc` (pure C++17, NO Dawn)
// --------------------------------------------
// Exactly like ExprWgsl.hpp, these are STRING generators over a tiny parameter
// set — they build no GPU resource. Keeping them in `dc` makes the emitted WGSL
// unit-testable WITHOUT a GPU (assert the kernel string in a fast logic test);
// the compute STAGE that uploads/dispatches/reads them lives in dc_gpu
// (ComputeStage, which #includes Dawn).
//
// THE BIND-GROUP KEEP-ALIVE LESSON (ENC-617a)
// -------------------------------------------
// Dawn's AUTO bind-group layout PRUNES any @group(0) binding the WGSL does not
// statically reference, which then mismatches a bind group that binds ALL the
// buffers ComputeStage uploads -> CreateBindGroup fails -> the dispatch silently
// writes nothing (all-zeros). Every kernel below therefore references EVERY
// declared binding (the params buffer in particular gets an `arrayLength`/field
// touch even when a branch would not otherwise read it).
#pragma once

#include <cstdint>
#include <string>

namespace dc {

// The reducer kind for the GPU aggregate fast path. Mirrors AggOp's streaming
// reducers; Median (class-2) stays CPU-only and is intentionally absent.
enum class GpuAggOp : std::uint8_t { Sum, Mean, Min, Max, Count };

// ---------------------------------------------------------------------------
// buildAggregateKernelWgsl — GPU aggregate via parallel reduction over a single
// i32 GROUP-KEY column whose values are the output GROUP INDEX in [0, nGroups)
// (the realistic large-N case: a `bin` index feeding `aggregate`), plus one f32
// MEASURE column of NON-NEGATIVE INTEGER-valued samples (so the reduction is
// exact in u32 and BYTE-IDENTICAL to the CPU AggregateTransform's f64 path).
//
// Bindings @group(0): 0=key(array<i32>), 1=measure(array<f32>),
//   2=outAcc(atomic<u32> per group: the sum/min/max as a u32), 3=outCount
//   (atomic<u32> per group), 4=params(u32: [rowCount, nGroups]).
// Min/Max seed outAcc with the identity (0xffffffff / 0) — the caller pre-fills
// it. The host converts outAcc -> the f32/i32 output (Sum/Min/Max -> value;
// Mean -> outAcc/outCount; Count -> outCount).
std::string buildAggregateKernelWgsl(GpuAggOp op);

// ---------------------------------------------------------------------------
// buildBinHistogramKernelWgsl — GPU `bin` as an ATOMIC-u32 HISTOGRAM. Each row's
// value is mapped to a bin index b = clamp(floor((v-firstEdge)/step), 0, count-1)
// (BYTE-IDENTICAL to BinTransform::evaluate's per-row assignment), then
// atomicAdd(&hist[b], 1u). The per-bin COUNT is exactly a downstream
// `bin -> aggregate(count)`; this fuses the two into one atomic pass.
//
// Bindings @group(0): 0=value(array<f32>), 1=hist(atomic<u32> per bin),
//   2=params(struct{rowCount:u32, binCount:u32, firstEdge:f32, step:f32}).
std::string buildBinHistogramKernelWgsl();

// ---------------------------------------------------------------------------
// buildScanKernelWgsl — a single-workgroup work-efficient (Blelloch) inclusive
// prefix-sum SCAN that turns a per-row f32 `value` column into the cumulative
// band y1[i] = sum_{j<=i} value[j] and baseline y0[i] = y1[i]-value[i] WITHIN a
// group (the `stack` band). Decoupled-lookback is the multi-workgroup
// generalization; for the bounded row counts this fast path targets a single
// workgroup (<= 2*workgroupSize rows) is exact and BYTE-IDENTICAL to the CPU
// StackTransform::Zero running sum. `workgroupSize` rows-per-half are processed
// (so up to 2*workgroupSize rows); the caller dispatches ONE workgroup.
//
// Bindings @group(0): 0=value(array<f32>), 1=y0(array<f32>), 2=y1(array<f32>),
//   3=params(u32: [rowCount]).
std::string buildScanKernelWgsl(std::uint32_t workgroupSize);

// ---------------------------------------------------------------------------
// buildKdeSplatKernelWgsl — a 2D KDE SPLAT-ACCUMULATE pass. Each point (x,y) is
// splat into a GRID (gridW x gridH) accumulating a gaussian kernel exp(-r^2 /
// (2 sigma^2)) over a (2*radius+1)^2 cell window around its cell. Float atomics
// are absent, so the density is accumulated as a FIXED-POINT u32
// (round(weight * kScale)) via atomicAdd; the host divides by kScale to recover
// the f32 density field (which a SequentialColorScale then tone-maps to color).
// The grid mapping (cell of a point, the gaussian weight) is computed identically
// on the CPU reference, so GPU==CPU after the fixed-point quantization.
//
// Bindings @group(0): 0=px(array<f32>), 1=py(array<f32>),
//   2=grid(atomic<u32> per cell, gridW*gridH), 3=params(struct{pointCount:u32,
//   gridW:u32, gridH:u32, radius:u32, x0:f32, y0:f32, invCellW:f32, invCellH:f32,
//   invTwoSigmaSq:f32, scale:f32}).
std::string buildKdeSplatKernelWgsl();

// The fixed-point scale the KDE splat accumulates in (weight*kKdeFixedScale,
// rounded). Public so the host divides the read-back u32 grid by the same value.
constexpr float kKdeFixedScale = 65536.0f;

}  // namespace dc
