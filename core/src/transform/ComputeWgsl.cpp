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

}  // namespace dc
