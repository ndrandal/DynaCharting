// ENC-619 (Epic ENC-619, the FINAL epic) — the WGSL ESCAPE HATCH.
//
// WHAT THIS IS (RESEARCH §5.3 + §7.4 tier 3)
// ------------------------------------------
// The custom WGSL compute node — the manifest-declared transform that carries a
// raw WGSL @compute kernel + typed storage IO bindings, uniforms, a dispatch and
// a recompute policy. It is the "shader-delivery vehicle" the grammar opens for
// exactly the four walls inexpressible in {filter,bin,aggregate,stack,window,sort}
// — FFT/STFT, 2D KDE field, marching-squares iso-extraction, bespoke per-cell math
// (§5.3 "what it buys"). It is a STATELESS column→column COMPUTE function inside
// the data path (§5.3 "what it cannot buy": no event capture, no cross-frame state,
// no pixel-hit routing, no 3D occluded raster).
//
// THE SANDBOX CONTRACT (§5.3, enforced at LOAD, fail-fast)
// --------------------------------------------------------
// I/O ONLY through declared storage-buffer bindings (no globals, no host calls);
// no dynamic alloc / recursion / unbounded loops (WGSL forbids these natively);
// static limits enforced at load:
//   * workgroup_size ≤ 256 (the WebGPU per-dim cap), and the product ≤ 256
//   * shared (workgroup) memory ≤ 16 KiB
//   * ≤ 8 storage buffers (inputs + outputs combined)
//   * dispatch ≤ 65535 per dim (the WebGPU maxComputeWorkgroupsPerDimension floor)
//   * each binding ≤ 128 MiB
//   * scalar dtypes f32 / f16 / i32 / u32 ONLY (epoch-ms pre-normalized to f32)
// A kernel that exceeds ANY limit is REJECTED at load (`validate()` returns a
// non-Ok status + a localized message) — NO partial render. The WGSL itself is
// Tint-validated at PIPELINE CREATION (dc_gpu / DawnDevice's error scope): a Tint
// rejection fails the dispatch loudly via lastDeviceError(), mirroring frameFailed.
//
// VARIABLE-CARDINALITY OUTPUT (contour / voronoi / hull)
// ------------------------------------------------------
// When the output count is data-dependent (marching-squares produces 0..2 segments
// per cell), the engine owns a MAX-BOUNDED output buffer + an atomic counter; the
// kernel atomicAdds to claim a slot and writes there; the host reads the counter +
// compacts to the live prefix. The manifest declares the cap (`capElements`). This
// is the §7.2 "topology extraction (variable cardinality)" path made concrete.
//
// WHY THIS HEADER LIVES IN `dc` (pure C++17, NO Dawn)
// ---------------------------------------------------
// Exactly like ExprWgsl / ComputeWgsl: the SPEC parse, the sandbox-limit checks,
// and the two reference WGSL builders (FFT/STFT, marching-squares) are pure string
// + struct logic — no GPU resource. That makes the WHOLE sandbox contract + the
// kernel codegen unit-testable WITHOUT a GPU (a kernel exceeding a limit is
// REJECTED in a fast logic test). The compute STAGE that uploads/dispatches/reads
// (ComputeStage::runCustomCompute / runStft / runMarchingSquares) lives in dc_gpu.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// The scalar element dtypes a custom-compute binding may carry. f64 is absent by
// design (the f32-only contract, §5.3: epoch-ms is pre-normalized to relative f32).
// ---------------------------------------------------------------------------
enum class CcDType : std::uint8_t { F32, F16, I32, U32 };

// Byte width of one element of a CcDType (f16 = 2 bytes; the rest = 4).
inline constexpr std::uint32_t ccDTypeBytes(CcDType dt) {
  return dt == CcDType::F16 ? 2u : 4u;
}

// The WGSL scalar-type spelling for a CcDType (for the binding's array<...> decl).
const char* ccDTypeWgsl(CcDType dt);

// Parse a dtype string ("f32"/"f16"/"i32"/"u32") -> CcDType. Returns false on any
// other string (in particular "f64" — rejected by the f32-only contract).
bool parseCcDType(const std::string& s, CcDType& out);

// ---------------------------------------------------------------------------
// One declared storage binding (an input the kernel reads, or an output it writes).
// I/O happens ONLY through these — the sandbox's single seam.
// ---------------------------------------------------------------------------
enum class CcAccess : std::uint8_t { Read, Write };

struct CcBinding {
  std::uint32_t binding{0};   // @group(0) @binding(N)
  std::string name;           // the WGSL var name AND (for inputs) the source column
  std::string column;         // inputs: the source column id (in.field); outputs: ""
  CcDType dtype{CcDType::F32};
  CcAccess access{CcAccess::Read};
  // The declared element COUNT of this binding's array. For a fixed-cardinality
  // output it is exact; for a VARIABLE-cardinality output it is the CAP (the engine
  // bounds the buffer at this many elements + an atomic counter). 0 for an input
  // (its length is the source column's row count, supplied at dispatch).
  std::uint32_t capElements{0};
  // True iff this output's cardinality is data-dependent (the atomic-counter +
  // compaction path). Only meaningful for an output (access == Write).
  bool variableCardinality{false};
};

// ---------------------------------------------------------------------------
// The recompute cadence policy (§5.3 `recomputePolicy`). Mirrors the streaming
// scheduler's class vocabulary loosely; for the escape hatch v0 we carry the hop
// (recompute every `hop` new rows) and a coarse class. The engine reads this to
// decide WHEN to re-dispatch; the sandbox does not need it to validate the kernel.
// ---------------------------------------------------------------------------
struct CcRecomputePolicy {
  std::uint32_t hop{0};       // recompute every `hop` appended rows (0 => perFrame)
  bool perFrame{true};        // recompute every frame (the conservative default)
};

// ---------------------------------------------------------------------------
// CustomComputeSpec — the fully-parsed custom-compute node. The control-plane
// object the validator builds from JSON and the sandbox `validate()`s; the GPU
// stage consumes it to upload/dispatch/read.
// ---------------------------------------------------------------------------
struct CustomComputeSpec {
  std::string id;                    // the transform node id
  std::string wgsl;                  // the raw @compute kernel source
  std::string entryPoint{"main"};    // the @compute fn name
  std::uint32_t workgroupX{64};      // @workgroup_size(x,y,z) — parsed from the WGSL
  std::uint32_t workgroupY{1};
  std::uint32_t workgroupZ{1};
  std::uint32_t sharedBytes{0};      // declared var<workgroup> bytes (parsed)
  std::vector<CcBinding> inputs;
  std::vector<CcBinding> outputs;
  // The dispatch grid (workgroup counts per dim). A value may be a literal or a
  // symbolic token the engine resolves from input lengths ("frames","bins",…); the
  // spec carries the RESOLVED literal counts the host computes (the validator
  // resolves the symbolic dispatch to literals before sandbox-checking the cap).
  std::uint32_t dispatchX{1};
  std::uint32_t dispatchY{1};
  std::uint32_t dispatchZ{1};
  CcRecomputePolicy recompute;

  // Total declared storage bindings (inputs + outputs) — the ≤8 limit operand.
  std::size_t bindingCount() const { return inputs.size() + outputs.size(); }
};

// ---------------------------------------------------------------------------
// The sandbox static limits (§5.3). Public so a test asserts a kernel that
// exceeds EACH one is rejected, and so the validator emits them in messages.
// ---------------------------------------------------------------------------
struct CcLimits {
  std::uint32_t maxWorkgroupPerDim{256};   // each of x/y/z ≤ this
  std::uint32_t maxWorkgroupProduct{256};  // x*y*z ≤ this (the invocation cap)
  std::uint32_t maxSharedBytes{16384};     // 16 KiB workgroup memory
  std::size_t maxStorageBuffers{8};        // inputs + outputs combined
  std::uint32_t maxDispatchPerDim{65535};  // each of x/y/z ≤ this
  std::uint64_t maxBindingBytes{128ull * 1024 * 1024};  // 128 MiB / binding
};

// The outcome of a sandbox validation. Ok carries an empty message; every reject
// carries a localized message naming the violated limit + the offending value.
enum class CcStatus : std::uint8_t {
  Ok,
  BadSpec,             // structurally malformed (no kernel / no entry / no IO)
  WorkgroupTooLarge,   // a workgroup dim or the product exceeds the cap
  SharedTooLarge,      // var<workgroup> bytes exceed 16 KiB
  TooManyBuffers,      // > 8 storage bindings
  DispatchTooLarge,    // a dispatch dim exceeds 65535
  BindingTooLarge,     // a binding's byte size exceeds 128 MiB
  BadDType,            // a binding dtype is not f32/f16/i32/u32 (e.g. f64)
  DuplicateBinding,    // two bindings share a @binding index
};

const char* toString(CcStatus s);

struct CcValidation {
  CcStatus status{CcStatus::Ok};
  std::string message;
  bool ok() const { return status == CcStatus::Ok; }
};

// ---------------------------------------------------------------------------
// validateCustomCompute — the sandbox gate. Enforces EVERY §5.3 static limit over
// `spec`, given the per-input element counts `inputElements` (parallel to
// spec.inputs; used to size an input binding's bytes) and the `limits`. Returns Ok
// or the FIRST violated limit with a localized message. Pure / data-free over the
// kernel; the WGSL string itself is Tint-validated later at pipeline creation.
// ---------------------------------------------------------------------------
CcValidation validateCustomCompute(const CustomComputeSpec& spec,
                                   const std::vector<std::uint32_t>& inputElements,
                                   const CcLimits& limits = {});

// ---------------------------------------------------------------------------
// parseWorkgroupSize — extract the @workgroup_size(x[,y[,z]]) literal counts from a
// WGSL kernel string (the values the sandbox checks against maxWorkgroupPerDim /
// product). Returns false if no @workgroup_size attribute is found (a kernel with
// none is malformed for our dispatch contract). Symbolic override_constant sizes
// are NOT accepted (the contract requires a static, checkable size).
// ---------------------------------------------------------------------------
bool parseWorkgroupSize(const std::string& wgsl, std::uint32_t& x,
                        std::uint32_t& y, std::uint32_t& z);

// ===========================================================================
// ENC-619 — CPU REFERENCE implementations of the two shipped escape-hatch kernels.
// These mirror the WGSL builders (ComputeWgsl.hpp: buildStftKernelWgsl /
// buildMarchingSquaresKernelWgsl) BIT-FOR-BIT in f32 math (same Hann window, same
// direct DFT, same marching-squares case table + saddle disambiguation), so the
// Dawn tests assert GPU==CPU within fp tolerance. They are also independently
// unit-testable WITHOUT a GPU (the fast logic test asserts correctness on a known
// signal / known field).
// ===========================================================================

// referenceStft — windowed short-time FFT magnitude over `samples` (a raw signal
// column). For each frame f (start = f*hop) and bin k in [0, fftSize/2], computes
// |X[k]| / fftSize of the Hann-windowed length-`fftSize` slice. The output is
// row-major frames × bins, bins = fftSize/2 + 1. `frames` is the number of full
// hops that fit: frames = samples.size() >= fftSize ? (samples.size()-fftSize)/hop
// + 1 : 0. Returns the magnitude grid (frames*bins f32); also writes `frames` and
// `bins` out. Uses f32 accumulation to match the WGSL exactly.
std::vector<float> referenceStft(const std::vector<float>& samples,
                                 std::uint32_t fftSize, std::uint32_t hop,
                                 std::uint32_t& frames, std::uint32_t& bins);

// One iso-line segment in GRID space (the variable-cardinality output element).
struct IsoSegment {
  float x0, y0, x1, y1;
};

// referenceMarchingSquares — per-cell iso-line extraction from a scalar `field`
// (gridW x gridH, row-major) at level `iso`. Returns the list of segments in the
// SAME per-cell scan order (gy outer, gx inner) the GPU produces, so a sorted
// comparison (or a set comparison) matches; the GPU's atomic ordering is arbitrary
// across cells, so the Dawn test sorts both sides. Same case table + saddle
// (center-average) disambiguation as the WGSL.
std::vector<IsoSegment> referenceMarchingSquares(const std::vector<float>& field,
                                                 std::uint32_t gridW,
                                                 std::uint32_t gridH, float iso);

}  // namespace dc
