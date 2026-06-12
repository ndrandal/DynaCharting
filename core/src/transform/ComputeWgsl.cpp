// ENC-617b/c/d — WGSL kernel builders for the non-expression GPU transforms.
// See dc/transform/ComputeWgsl.hpp for the contract + the keep-alive lesson.
#include "dc/transform/ComputeWgsl.hpp"

namespace dc {

std::string buildAggregateKernelWgsl(GpuAggOp op) {
  // Bindings: 0 key (i32), 1 measure (f32), 2 outAcc (atomic u32), 3 outCount
  // (atomic u32), 4 params (u32: [rowCount, nGroups]).
  std::string s;
  s += "@group(0) @binding(0) var<storage, read> keyc : array<i32>;\n";
  s += "@group(0) @binding(1) var<storage, read> measure : array<f32>;\n";
  s += "@group(0) @binding(2) var<storage, read_write> outAcc : "
       "array<atomic<u32>>;\n";
  s += "@group(0) @binding(3) var<storage, read_write> outCount : "
       "array<atomic<u32>>;\n";
  s += "@group(0) @binding(4) var<storage, read> params : array<u32>;\n";
  s += "\n@compute @workgroup_size(64)\n";
  s += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  s += "  let i = gid.x;\n";
  s += "  let rowCount = params[0];\n";
  // Keep-alive: touch binding 1 length so an op that never reads `measure`
  // (Count) still retains the binding in the auto layout.
  s += "  _ = arrayLength(&measure);\n";
  s += "  if (i >= rowCount) { return; }\n";
  s += "  let g = u32(keyc[i]);\n";
  // count is always maintained (Mean needs it; Count returns it).
  s += "  atomicAdd(&outCount[g], 1u);\n";
  switch (op) {
    case GpuAggOp::Count:
      // Touch outAcc so its binding is retained even though Count ignores it.
      s += "  atomicAdd(&outAcc[g], 0u);\n";
      break;
    case GpuAggOp::Sum:
    case GpuAggOp::Mean:
      // Non-negative integer-valued measure -> exact u32 accumulation.
      s += "  atomicAdd(&outAcc[g], u32(measure[i]));\n";
      break;
    case GpuAggOp::Min:
      s += "  atomicMin(&outAcc[g], u32(measure[i]));\n";
      break;
    case GpuAggOp::Max:
      s += "  atomicMax(&outAcc[g], u32(measure[i]));\n";
      break;
  }
  s += "}\n";
  return s;
}

std::string buildBinHistogramKernelWgsl() {
  // params: struct{ rowCount:u32, binCount:u32, firstEdge:f32, step:f32 }.
  std::string s;
  s += "struct Params {\n";
  s += "  rowCount : u32,\n";
  s += "  binCount : u32,\n";
  s += "  firstEdge : f32,\n";
  s += "  step : f32,\n";
  s += "};\n";
  s += "@group(0) @binding(0) var<storage, read> value : array<f32>;\n";
  s += "@group(0) @binding(1) var<storage, read_write> hist : "
       "array<atomic<u32>>;\n";
  s += "@group(0) @binding(2) var<storage, read> params : Params;\n";
  s += "\n@compute @workgroup_size(64)\n";
  s += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  s += "  let i = gid.x;\n";
  s += "  if (i >= params.rowCount) { return; }\n";
  s += "  let v = value[i];\n";
  // Byte-identical to BinTransform: b = clamp(floor((v-firstEdge)/step), 0,
  // count-1); a non-finite value lands in bin 0 (CPU's std::isfinite guard).
  s += "  var b : i32 = 0;\n";
  // finite(v): rules out NaN (v==v) and +/-inf (inf-inf == NaN). Avoids an
  // f32 max literal — Tint rejects 3.4028235e38 as "cannot be represented as
  // f32" because it rounds up past FLT_MAX to inf at compile time.
  s += "  if (v == v && (v - v) == 0.0) {\n";
  s += "    b = i32(floor((v - params.firstEdge) / params.step));\n";
  s += "    b = max(0, min(b, i32(params.binCount) - 1));\n";
  s += "  }\n";
  s += "  atomicAdd(&hist[u32(b)], 1u);\n";
  s += "}\n";
  return s;
}

std::string buildScanKernelWgsl(std::uint32_t workgroupSize) {
  // A single-workgroup work-efficient (Blelloch) inclusive scan. `n2` =
  // 2*workgroupSize is the padded power-of-two array length the shared buffer
  // holds; rows beyond rowCount are padded with 0 so the scan is exact.
  const std::uint32_t n2 = workgroupSize * 2u;
  const std::string N2 = std::to_string(n2);
  const std::string WG = std::to_string(workgroupSize);
  std::string s;
  s += "@group(0) @binding(0) var<storage, read> value : array<f32>;\n";
  s += "@group(0) @binding(1) var<storage, read_write> y0 : array<f32>;\n";
  s += "@group(0) @binding(2) var<storage, read_write> y1 : array<f32>;\n";
  s += "@group(0) @binding(3) var<storage, read> params : array<u32>;\n";
  s += "\nvar<workgroup> temp : array<f32, " + N2 + ">;\n";
  s += "\n@compute @workgroup_size(" + WG + ")\n";
  s += "fn main(@builtin(local_invocation_id) lid : vec3<u32>) {\n";
  s += "  let t = lid.x;\n";
  s += "  let n = params[0];\n";
  s += "  let ai = t;\n";
  s += "  let bi = t + " + WG + "u;\n";
  // Load 2 elements per thread into shared memory (0-pad the tail).
  s += "  temp[ai] = select(0.0f, value[ai], ai < n);\n";
  s += "  temp[bi] = select(0.0f, value[bi], bi < n);\n";
  // Up-sweep (reduce) phase.
  s += "  var offset : u32 = 1u;\n";
  s += "  for (var d : u32 = " + N2 + "u >> 1u; d > 0u; d = d >> 1u) {\n";
  s += "    workgroupBarrier();\n";
  s += "    if (t < d) {\n";
  s += "      let aii = offset * (2u * t + 1u) - 1u;\n";
  s += "      let bii = offset * (2u * t + 2u) - 1u;\n";
  s += "      temp[bii] = temp[bii] + temp[aii];\n";
  s += "    }\n";
  s += "    offset = offset * 2u;\n";
  s += "  }\n";
  // Save the total then clear the last element for the down-sweep.
  s += "  workgroupBarrier();\n";
  s += "  if (t == 0u) { temp[" + N2 + "u - 1u] = 0.0f; }\n";
  // Down-sweep phase.
  s += "  for (var d : u32 = 1u; d < " + N2 + "u; d = d * 2u) {\n";
  s += "    offset = offset >> 1u;\n";
  s += "    workgroupBarrier();\n";
  s += "    if (t < d) {\n";
  s += "      let aii = offset * (2u * t + 1u) - 1u;\n";
  s += "      let bii = offset * (2u * t + 2u) - 1u;\n";
  s += "      let tmp = temp[aii];\n";
  s += "      temp[aii] = temp[bii];\n";
  s += "      temp[bii] = temp[bii] + tmp;\n";
  s += "    }\n";
  s += "  }\n";
  s += "  workgroupBarrier();\n";
  // temp now holds the EXCLUSIVE prefix sum -> that is y0; y1 = y0 + value.
  s += "  if (ai < n) { y0[ai] = temp[ai]; y1[ai] = temp[ai] + value[ai]; }\n";
  s += "  if (bi < n) { y0[bi] = temp[bi]; y1[bi] = temp[bi] + value[bi]; }\n";
  s += "}\n";
  return s;
}

std::string buildKdeSplatKernelWgsl() {
  // params: struct{ pointCount,gridW,gridH,radius:u32; x0,y0,invCellW,invCellH,
  //   invTwoSigmaSq,scale:f32 }.
  std::string s;
  s += "struct Params {\n";
  s += "  pointCount : u32,\n";
  s += "  gridW : u32,\n";
  s += "  gridH : u32,\n";
  s += "  radius : u32,\n";
  s += "  x0 : f32,\n";
  s += "  y0 : f32,\n";
  s += "  invCellW : f32,\n";
  s += "  invCellH : f32,\n";
  s += "  invTwoSigmaSq : f32,\n";
  s += "  scale : f32,\n";
  s += "};\n";
  s += "@group(0) @binding(0) var<storage, read> px : array<f32>;\n";
  s += "@group(0) @binding(1) var<storage, read> py : array<f32>;\n";
  s += "@group(0) @binding(2) var<storage, read_write> grid : "
       "array<atomic<u32>>;\n";
  s += "@group(0) @binding(3) var<storage, read> params : Params;\n";
  s += "\n@compute @workgroup_size(64)\n";
  s += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  s += "  let i = gid.x;\n";
  s += "  if (i >= params.pointCount) { return; }\n";
  // The point's continuous cell coordinate (matches the CPU reference exactly).
  s += "  let cx = (px[i] - params.x0) * params.invCellW;\n";
  s += "  let cy = (py[i] - params.y0) * params.invCellH;\n";
  s += "  let ci = i32(floor(cx));\n";
  s += "  let cj = i32(floor(cy));\n";
  s += "  let r = i32(params.radius);\n";
  s += "  for (var dj : i32 = -r; dj <= r; dj = dj + 1) {\n";
  s += "    for (var di : i32 = -r; di <= r; di = di + 1) {\n";
  s += "      let gx = ci + di;\n";
  s += "      let gy = cj + dj;\n";
  s += "      if (gx < 0 || gy < 0 || gx >= i32(params.gridW) || "
       "gy >= i32(params.gridH)) { continue; }\n";
  // Distance from the point to the CELL CENTER, in cell units.
  s += "      let ddx = (f32(gx) + 0.5f) - cx;\n";
  s += "      let ddy = (f32(gy) + 0.5f) - cy;\n";
  s += "      let r2 = ddx * ddx + ddy * ddy;\n";
  s += "      let w = exp(-r2 * params.invTwoSigmaSq);\n";
  // Fixed-point accumulation (no float atomics): round(w*scale) added as u32.
  s += "      let q = u32(round(w * params.scale));\n";
  s += "      let idx = u32(gy) * params.gridW + u32(gx);\n";
  s += "      atomicAdd(&grid[idx], q);\n";
  s += "    }\n";
  s += "  }\n";
  s += "}\n";
  return s;
}

// ===========================================================================
// ENC-619 — the escape-hatch reference kernels (FFT/STFT, marching-squares).
// ===========================================================================

std::string buildStftKernelWgsl() {
  // params: struct{ fftSize:u32, hop:u32, frames:u32, bins:u32 }. One invocation
  // per (frame f = gid.x, bin k = gid.y); direct DFT of the Hann-windowed slice.
  std::string s;
  s += "struct Params {\n";
  s += "  fftSize : u32,\n";
  s += "  hop : u32,\n";
  s += "  frames : u32,\n";
  s += "  bins : u32,\n";
  s += "};\n";
  s += "@group(0) @binding(0) var<storage, read> sample : array<f32>;\n";
  s += "@group(0) @binding(1) var<storage, read_write> mag : array<f32>;\n";
  s += "@group(0) @binding(2) var<storage, read> params : Params;\n";
  s += "\n@compute @workgroup_size(8, 8, 1)\n";
  s += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  s += "  let f = gid.x;\n";
  s += "  let k = gid.y;\n";
  // Keep-alive: touch the sample-array length so the binding survives auto-layout.
  s += "  _ = arrayLength(&sample);\n";
  s += "  if (f >= params.frames || k >= params.bins) { return; }\n";
  s += "  let N = params.fftSize;\n";
  s += "  let base = f * params.hop;\n";
  s += "  let twoPi = 6.28318530717958647692528676655900577f;\n";
  s += "  let wk = -twoPi * f32(k) / f32(N);\n";
  s += "  var re : f32 = 0.0f;\n";
  s += "  var im : f32 = 0.0f;\n";
  s += "  for (var n : u32 = 0u; n < N; n = n + 1u) {\n";
  // Hann window (denominator N-1, matching the CPU reference).
  s += "    let hann = 0.5f * (1.0f - cos(twoPi * f32(n) / f32(N - 1u)));\n";
  s += "    let sn = sample[base + n] * hann;\n";
  s += "    let ang = wk * f32(n);\n";
  s += "    re = re + sn * cos(ang);\n";
  s += "    im = im + sn * sin(ang);\n";
  s += "  }\n";
  s += "  let m = sqrt(re * re + im * im) / f32(N);\n";
  s += "  mag[f * params.bins + k] = m;\n";
  s += "}\n";
  return s;
}

std::string buildMarchingSquaresKernelWgsl() {
  // params: struct{ gridW:u32, gridH:u32, capSegs:u32, iso:f32 }. One invocation
  // per cell; emits 0..2 iso-line segments into the bounded `segs` buffer, claiming
  // slots via an atomic counter (the variable-cardinality path).
  std::string s;
  s += "struct Params {\n";
  s += "  gridW : u32,\n";
  s += "  gridH : u32,\n";
  s += "  capSegs : u32,\n";
  s += "  iso : f32,\n";
  s += "};\n";
  s += "@group(0) @binding(0) var<storage, read> field : array<f32>;\n";
  s += "@group(0) @binding(1) var<storage, read_write> segs : array<f32>;\n";
  s += "@group(0) @binding(2) var<storage, read_write> segCount : atomic<u32>;\n";
  s += "@group(0) @binding(3) var<storage, read> params : Params;\n";
  // Linear iso-crossing on an edge between two corner values a,b at unit spacing.
  s += "\nfn lerpT(a : f32, b : f32, iso : f32) -> f32 {\n";
  s += "  let d = b - a;\n";
  // Guard a degenerate edge (a==b): the case table never asks for a crossing on a
  // non-straddling edge, but keep it finite (return 0.5) to match the CPU guard.
  s += "  if (d == 0.0f) { return 0.5f; }\n";
  s += "  return (iso - a) / d;\n";
  s += "}\n";
  // Claim a slot + write one segment (x0,y0)->(x1,y1); drop on overflow.
  s += "fn emit(x0 : f32, y0 : f32, x1 : f32, y1 : f32, cap : u32) {\n";
  s += "  let slot = atomicAdd(&segCount, 1u);\n";
  s += "  if (slot >= cap) { return; }\n";
  s += "  let o = slot * 4u;\n";
  s += "  segs[o + 0u] = x0;\n";
  s += "  segs[o + 1u] = y0;\n";
  s += "  segs[o + 2u] = x1;\n";
  s += "  segs[o + 3u] = y1;\n";
  s += "}\n";
  s += "\n@compute @workgroup_size(8, 8, 1)\n";
  s += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  s += "  let gx = gid.x;\n";
  s += "  let gy = gid.y;\n";
  s += "  _ = arrayLength(&segs);\n";
  s += "  if (gx + 1u >= params.gridW || gy + 1u >= params.gridH) { return; }\n";
  s += "  let W = params.gridW;\n";
  s += "  let iso = params.iso;\n";
  // Corners: bl=(gx,gy) tl=(gx,gy+1) tr=(gx+1,gy+1) br=(gx+1,gy). Field is
  // row-major [row*W + col].
  s += "  let bl = field[gy * W + gx];\n";
  s += "  let br = field[gy * W + (gx + 1u)];\n";
  s += "  let tl = field[(gy + 1u) * W + gx];\n";
  s += "  let tr = field[(gy + 1u) * W + (gx + 1u)];\n";
  s += "  let fx = f32(gx);\n";
  s += "  let fy = f32(gy);\n";
  // 4-bit case index: bit0=bl bit1=br bit2=tr bit3=tl (CCW from bottom-left).
  s += "  var c : u32 = 0u;\n";
  s += "  if (bl >= iso) { c = c | 1u; }\n";
  s += "  if (br >= iso) { c = c | 2u; }\n";
  s += "  if (tr >= iso) { c = c | 4u; }\n";
  s += "  if (tl >= iso) { c = c | 8u; }\n";
  // Edge crossing points (only valid when that edge straddles iso):
  //   B(bottom): bl-br ; R(right): br-tr ; T(top): tl-tr ; L(left): bl-tl
  s += "  let bX = fx + lerpT(bl, br, iso);\n";
  s += "  let bY = fy;\n";
  s += "  let rX = fx + 1.0f;\n";
  s += "  let rY = fy + lerpT(br, tr, iso);\n";
  s += "  let tX = fx + lerpT(tl, tr, iso);\n";
  s += "  let tY = fy + 1.0f;\n";
  s += "  let lX = fx;\n";
  s += "  let lY = fy + lerpT(bl, tl, iso);\n";
  s += "  let cap = params.capSegs;\n";
  // The 16 cases (1 and 14, 2 and 13, ... are complementary -> same segment). The
  // two saddles (5,10) disambiguate via the cell-center average vs iso, matching
  // the CPU reference.
  s += "  switch (c) {\n";
  s += "    case 0u, 15u: {}\n";
  s += "    case 1u, 14u: { emit(lX, lY, bX, bY, cap); }\n";
  s += "    case 2u, 13u: { emit(bX, bY, rX, rY, cap); }\n";
  s += "    case 3u, 12u: { emit(lX, lY, rX, rY, cap); }\n";
  s += "    case 4u, 11u: { emit(rX, rY, tX, tY, cap); }\n";
  s += "    case 6u, 9u:  { emit(bX, bY, tX, tY, cap); }\n";
  s += "    case 7u, 8u:  { emit(lX, lY, tX, tY, cap); }\n";
  s += "    case 5u: {\n";  // bl,tr inside; br,tl outside
  s += "      let center = (bl + br + tr + tl) * 0.25f;\n";
  s += "      if (center >= iso) {\n";
  s += "        emit(lX, lY, tX, tY, cap); emit(bX, bY, rX, rY, cap);\n";
  s += "      } else {\n";
  s += "        emit(lX, lY, bX, bY, cap); emit(tX, tY, rX, rY, cap);\n";
  s += "      }\n";
  s += "    }\n";
  s += "    case 10u: {\n";  // br,tl inside; bl,tr outside
  s += "      let center = (bl + br + tr + tl) * 0.25f;\n";
  s += "      if (center >= iso) {\n";
  s += "        emit(lX, lY, bX, bY, cap); emit(tX, tY, rX, rY, cap);\n";
  s += "      } else {\n";
  s += "        emit(lX, lY, tX, tY, cap); emit(bX, bY, rX, rY, cap);\n";
  s += "      }\n";
  s += "    }\n";
  s += "    default: {}\n";
  s += "  }\n";
  s += "}\n";
  return s;
}

}  // namespace dc
