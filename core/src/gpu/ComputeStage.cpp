// ENC-617a — ComputeStage implementation. See dc/gpu/ComputeStage.hpp.
//
// The generalization of the ENC-591 spike: instead of one hard-coded `data[i]*=2`
// kernel over one buffer, this uploads N input columns + an output column, builds
// the pipeline from a GENERATED kernel (ExprWgsl), dispatches, and reads back —
// the column-oriented reuse of DawnDevice's ENC-590/591 compute API.
#include "dc/gpu/ComputeStage.hpp"

#include "dc/transform/ExprWgsl.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace dc {

bool ComputeStage::dispatchKernel(const std::vector<std::vector<float>>& columns,
                                  const std::string& wgsl, std::size_t rowCount,
                                  void* outData, std::size_t outBytes) {
  // Every input column must have the same length (the row count).
  for (const auto& c : columns) {
    if (c.size() != rowCount) return false;
  }
  if (rowCount == 0) return true;  // nothing to dispatch; output stays as-is.

  // --- 1. Upload each input column to a read-only storage buffer. -----------
  // Slot order == binding order == the kernel's col0..col{C-1} declarations.
  std::vector<BufferHandle> bindings;
  bindings.reserve(columns.size() + 1);
  for (const auto& col : columns) {
    const std::size_t bytes = col.size() * sizeof(float);
    BufferHandle h = device_.createStorageBuffer(bytes, col.data(), bytes);
    if (!h.valid()) return false;
    bindings.push_back(h);
  }

  // --- 2. Allocate the output storage buffer (zero-initialized). ------------
  BufferHandle outBuf = device_.createStorageBuffer(outBytes, nullptr, 0);
  if (!outBuf.valid()) return false;
  bindings.push_back(outBuf);

  // --- 3. Build the compute pipeline + dispatch ceil(N / 64) workgroups. ----
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  const std::uint32_t n = static_cast<std::uint32_t>(rowCount);
  const std::uint32_t groups = (n + kWorkgroupSize - 1) / kWorkgroupSize;
  if (!device_.dispatchCompute(pipe, bindings, groups)) return false;

  // --- 4. Read the output buffer back via the existing map-pump readback. ---
  return device_.readBuffer(outBuf, 0, outBytes,
                            reinterpret_cast<std::uint8_t*>(outData));
}

bool ComputeStage::runFormula(const std::vector<std::vector<float>>& columns,
                              const CompiledExpr& expr,
                              std::vector<float>& out) {
  if (!expr.valid() || expr.resultKind != ExprKind::Num) return false;
  const std::size_t rowCount = columns.empty() ? 0 : columns.front().size();

  const std::string wgsl = buildFormulaKernelWgsl(
      expr, static_cast<std::uint32_t>(columns.size()));

  out.assign(rowCount, 0.0f);
  if (rowCount == 0) return true;
  return dispatchKernel(columns, wgsl, rowCount, out.data(),
                        out.size() * sizeof(float));
}

bool ComputeStage::runFilter(const std::vector<std::vector<float>>& columns,
                             const CompiledExpr& expr,
                             std::vector<std::uint32_t>& keepRows) {
  if (!expr.valid() || expr.resultKind != ExprKind::Bool) return false;
  const std::size_t rowCount = columns.empty() ? 0 : columns.front().size();

  const std::string wgsl = buildFilterMaskKernelWgsl(
      expr, static_cast<std::uint32_t>(columns.size()));

  // The GPU writes a per-row 0/1 mask; we compact survivors CPU-side (the simple-
  // mask output of RESEARCH §5.1; prefix-sum compaction is a later sub-PR).
  std::vector<std::uint32_t> mask(rowCount, 0u);
  keepRows.clear();
  if (rowCount == 0) return true;
  if (!dispatchKernel(columns, wgsl, rowCount, mask.data(),
                      mask.size() * sizeof(std::uint32_t))) {
    return false;
  }
  for (std::uint32_t i = 0; i < rowCount; ++i) {
    if (mask[i] != 0u) keepRows.push_back(i);
  }
  return true;
}

// ===========================================================================
// ENC-617b — GPU aggregate (parallel reduction) + bin (atomic-u32 histogram)
// ===========================================================================

bool ComputeStage::runAggregate(const std::vector<std::int32_t>& keys,
                                const std::vector<float>& measure, GpuAggOp op,
                                std::uint32_t nGroups, std::vector<float>& out) {
  const std::size_t rowCount = keys.size();
  out.assign(nGroups, 0.0f);
  if (nGroups == 0) return true;
  if (measure.size() != rowCount) return false;
  if (rowCount == 0) return true;

  // Seed the accumulator: identity per reducer. Min -> 0xffffffff (u32 max),
  // Max/Sum/Mean/Count -> 0. (Min/Max compare on u32; the measure is a
  // non-negative integer so its u32 ordering matches its numeric ordering.)
  const std::uint32_t accSeed =
      (op == GpuAggOp::Min) ? 0xffffffffu : 0u;
  std::vector<std::uint32_t> acc(nGroups, accSeed);
  std::vector<std::uint32_t> count(nGroups, 0u);
  const std::uint32_t params[2] = {static_cast<std::uint32_t>(rowCount),
                                   nGroups};

  // Bindings in slot order: 0 key, 1 measure, 2 acc, 3 count, 4 params.
  BufferHandle keyBuf = device_.createStorageBuffer(
      keys.size() * sizeof(std::int32_t), keys.data(),
      keys.size() * sizeof(std::int32_t));
  BufferHandle measBuf = device_.createStorageBuffer(
      measure.size() * sizeof(float), measure.data(),
      measure.size() * sizeof(float));
  BufferHandle accBuf = device_.createStorageBuffer(
      acc.size() * sizeof(std::uint32_t), acc.data(),
      acc.size() * sizeof(std::uint32_t));
  BufferHandle countBuf = device_.createStorageBuffer(
      count.size() * sizeof(std::uint32_t), count.data(),
      count.size() * sizeof(std::uint32_t));
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), params, sizeof(params));
  if (!keyBuf.valid() || !measBuf.valid() || !accBuf.valid() ||
      !countBuf.valid() || !paramBuf.valid()) {
    return false;
  }

  const std::string wgsl = buildAggregateKernelWgsl(op);
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  const std::uint32_t n = static_cast<std::uint32_t>(rowCount);
  const std::uint32_t groups = (n + kWorkgroupSize - 1) / kWorkgroupSize;
  if (!device_.dispatchCompute(pipe, {keyBuf, measBuf, accBuf, countBuf, paramBuf},
                               groups)) {
    return false;
  }

  if (!device_.readBuffer(accBuf, 0, acc.size() * sizeof(std::uint32_t),
                          reinterpret_cast<std::uint8_t*>(acc.data()))) {
    return false;
  }
  if (!device_.readBuffer(countBuf, 0, count.size() * sizeof(std::uint32_t),
                          reinterpret_cast<std::uint8_t*>(count.data()))) {
    return false;
  }

  // Resolve the reducer's final value per group (mirrors AggregateTransform).
  for (std::uint32_t g = 0; g < nGroups; ++g) {
    switch (op) {
      case GpuAggOp::Count:
        out[g] = static_cast<float>(count[g]);
        break;
      case GpuAggOp::Sum:
        out[g] = static_cast<float>(acc[g]);
        break;
      case GpuAggOp::Mean:
        out[g] = count[g] > 0u
                     ? static_cast<float>(static_cast<double>(acc[g]) /
                                          static_cast<double>(count[g]))
                     : 0.0f;
        break;
      case GpuAggOp::Min:
        out[g] = count[g] > 0u ? static_cast<float>(acc[g]) : 0.0f;
        break;
      case GpuAggOp::Max:
        out[g] = count[g] > 0u ? static_cast<float>(acc[g]) : 0.0f;
        break;
    }
  }
  return true;
}

bool ComputeStage::runBin(const std::vector<float>& values, float firstEdge,
                          float step, std::uint32_t binCount,
                          std::vector<std::uint32_t>& counts) {
  counts.assign(binCount, 0u);
  if (binCount == 0 || !(step > 0.0f)) return false;
  if (values.empty()) return true;

  // params: struct{ rowCount:u32, binCount:u32, firstEdge:f32, step:f32 }.
  struct {
    std::uint32_t rowCount;
    std::uint32_t binCount;
    float firstEdge;
    float step;
  } params{static_cast<std::uint32_t>(values.size()), binCount, firstEdge, step};

  BufferHandle valBuf = device_.createStorageBuffer(
      values.size() * sizeof(float), values.data(),
      values.size() * sizeof(float));
  BufferHandle histBuf = device_.createStorageBuffer(
      counts.size() * sizeof(std::uint32_t), counts.data(),
      counts.size() * sizeof(std::uint32_t));
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), &params, sizeof(params));
  if (!valBuf.valid() || !histBuf.valid() || !paramBuf.valid()) return false;

  const std::string wgsl = buildBinHistogramKernelWgsl();
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  const std::uint32_t n = static_cast<std::uint32_t>(values.size());
  const std::uint32_t groups = (n + kWorkgroupSize - 1) / kWorkgroupSize;
  if (!device_.dispatchCompute(pipe, {valBuf, histBuf, paramBuf}, groups)) {
    return false;
  }
  return device_.readBuffer(histBuf, 0, counts.size() * sizeof(std::uint32_t),
                            reinterpret_cast<std::uint8_t*>(counts.data()));
}

// ===========================================================================
// ENC-617c — GPU stack via a single-workgroup work-efficient prefix-sum scan
// ===========================================================================

bool ComputeStage::runStackScan(const std::vector<float>& values,
                                std::vector<float>& y0, std::vector<float>& y1) {
  const std::size_t rowCount = values.size();
  y0.assign(rowCount, 0.0f);
  y1.assign(rowCount, 0.0f);
  if (rowCount == 0) return true;
  if (rowCount > 2u * kScanWorkgroupSize) return false;

  const std::uint32_t params[1] = {static_cast<std::uint32_t>(rowCount)};

  BufferHandle valBuf = device_.createStorageBuffer(
      values.size() * sizeof(float), values.data(),
      values.size() * sizeof(float));
  BufferHandle y0Buf =
      device_.createStorageBuffer(rowCount * sizeof(float), nullptr, 0);
  BufferHandle y1Buf =
      device_.createStorageBuffer(rowCount * sizeof(float), nullptr, 0);
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), params, sizeof(params));
  if (!valBuf.valid() || !y0Buf.valid() || !y1Buf.valid() || !paramBuf.valid()) {
    return false;
  }

  const std::string wgsl = buildScanKernelWgsl(kScanWorkgroupSize);
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  // Single workgroup: the scan processes up to 2*kScanWorkgroupSize rows.
  if (!device_.dispatchCompute(pipe, {valBuf, y0Buf, y1Buf, paramBuf}, 1u)) {
    return false;
  }
  if (!device_.readBuffer(y0Buf, 0, rowCount * sizeof(float),
                          reinterpret_cast<std::uint8_t*>(y0.data()))) {
    return false;
  }
  return device_.readBuffer(y1Buf, 0, rowCount * sizeof(float),
                            reinterpret_cast<std::uint8_t*>(y1.data()));
}

// ===========================================================================
// ENC-617d — 2D KDE splat-accumulate
// ===========================================================================

bool ComputeStage::runKde(const std::vector<float>& px,
                          const std::vector<float>& py, const KdeGrid& grid,
                          std::vector<float>& density) {
  const std::size_t pointCount = px.size();
  const std::size_t cells =
      static_cast<std::size_t>(grid.width) * grid.height;
  density.assign(cells, 0.0f);
  if (grid.width == 0 || grid.height == 0) return false;
  if (py.size() != pointCount) return false;
  if (pointCount == 0) return true;
  if (!(grid.cellW > 0.0f) || !(grid.cellH > 0.0f) || !(grid.sigma > 0.0f)) {
    return false;
  }

  // params: struct{ pointCount,gridW,gridH,radius:u32; x0,y0,invCellW,invCellH,
  //   invTwoSigmaSq,scale:f32 }.
  struct {
    std::uint32_t pointCount;
    std::uint32_t gridW;
    std::uint32_t gridH;
    std::uint32_t radius;
    float x0;
    float y0;
    float invCellW;
    float invCellH;
    float invTwoSigmaSq;
    float scale;
  } params{static_cast<std::uint32_t>(pointCount),
           grid.width,
           grid.height,
           grid.radius,
           grid.x0,
           grid.y0,
           1.0f / grid.cellW,
           1.0f / grid.cellH,
           1.0f / (2.0f * grid.sigma * grid.sigma),
           kKdeFixedScale};

  std::vector<std::uint32_t> gridU(cells, 0u);
  BufferHandle pxBuf = device_.createStorageBuffer(
      px.size() * sizeof(float), px.data(), px.size() * sizeof(float));
  BufferHandle pyBuf = device_.createStorageBuffer(
      py.size() * sizeof(float), py.data(), py.size() * sizeof(float));
  BufferHandle gridBuf = device_.createStorageBuffer(
      gridU.size() * sizeof(std::uint32_t), gridU.data(),
      gridU.size() * sizeof(std::uint32_t));
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), &params, sizeof(params));
  if (!pxBuf.valid() || !pyBuf.valid() || !gridBuf.valid() || !paramBuf.valid()) {
    return false;
  }

  const std::string wgsl = buildKdeSplatKernelWgsl();
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  const std::uint32_t n = static_cast<std::uint32_t>(pointCount);
  const std::uint32_t groups = (n + kWorkgroupSize - 1) / kWorkgroupSize;
  if (!device_.dispatchCompute(pipe, {pxBuf, pyBuf, gridBuf, paramBuf}, groups)) {
    return false;
  }
  if (!device_.readBuffer(gridBuf, 0, gridU.size() * sizeof(std::uint32_t),
                          reinterpret_cast<std::uint8_t*>(gridU.data()))) {
    return false;
  }
  // Recover the f32 density field from the fixed-point accumulation.
  for (std::size_t k = 0; k < cells; ++k) {
    density[k] = static_cast<float>(gridU[k]) / kKdeFixedScale;
  }
  return true;
}

}  // namespace dc
