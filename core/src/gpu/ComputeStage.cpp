// ENC-617a — ComputeStage implementation. See dc/gpu/ComputeStage.hpp.
//
// The generalization of the ENC-591 spike: instead of one hard-coded `data[i]*=2`
// kernel over one buffer, this uploads N input columns + an output column, builds
// the pipeline from a GENERATED kernel (ExprWgsl), dispatches, and reads back —
// the column-oriented reuse of DawnDevice's ENC-590/591 compute API.
#include "dc/gpu/ComputeStage.hpp"

#include "dc/transform/ExprWgsl.hpp"

#include <algorithm>
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

// ===========================================================================
// ENC-619 — the WGSL escape hatch: general customCompute dispatch + the two
// shipped reference kernels (FFT/STFT, marching-squares).
// ===========================================================================

bool ComputeStage::runCustomCompute(
    const CustomComputeSpec& spec,
    const std::vector<std::vector<float>>& inputs,
    std::vector<std::vector<float>>& outputs,
    std::vector<std::uint32_t>& outCounts) {
  if (inputs.size() != spec.inputs.size()) return false;

  // Sandbox gate FIRST (no partial render): a violation rejects before any GPU
  // resource is touched. Input element counts size the input bindings.
  std::vector<std::uint32_t> inElems(spec.inputs.size(), 0u);
  for (std::size_t k = 0; k < inputs.size(); ++k)
    inElems[k] = static_cast<std::uint32_t>(inputs[k].size());
  if (!validateCustomCompute(spec, inElems).ok()) return false;

  // Assemble the binding list in ASCENDING @binding order (dispatchCompute binds
  // the array sequentially to bindings 0,1,2,…, so the device buffer order must
  // match the kernel's @binding(N) declarations). A variable-cardinality output
  // carries an extra atomic-counter buffer immediately after it (the kernel's next
  // @binding); the spec's binding indices already account for that ordering.
  struct Slot {
    std::uint32_t binding;
    BufferHandle buf;
  };
  std::vector<Slot> slots;

  // Inputs: upload read-only storage buffers.
  for (std::size_t k = 0; k < spec.inputs.size(); ++k) {
    const auto& col = inputs[k];
    const std::size_t bytes = col.size() * sizeof(float);
    BufferHandle h =
        device_.createStorageBuffer(bytes ? bytes : 4, col.data(), bytes);
    if (!h.valid()) return false;
    slots.push_back({spec.inputs[k].binding, h});
  }

  // Outputs: allocate zero-initialized buffers at the declared cap. A
  // variable-cardinality output also gets a zeroed u32 counter buffer at the
  // binding immediately following it.
  outputs.assign(spec.outputs.size(), {});
  outCounts.assign(spec.outputs.size(), 0u);
  // Cache the output + counter HANDLES directly (handles are stable identities,
  // independent of the slot vector's order) so readback is robust to the sort.
  std::vector<BufferHandle> outHandle(spec.outputs.size());
  std::vector<BufferHandle> cntHandle(spec.outputs.size());  // null if fixed-card
  for (std::size_t k = 0; k < spec.outputs.size(); ++k) {
    const auto& o = spec.outputs[k];
    const std::size_t bytes =
        static_cast<std::size_t>(o.capElements) * ccDTypeBytes(o.dtype);
    BufferHandle h =
        device_.createStorageBuffer(bytes ? bytes : 4, nullptr, 0);
    if (!h.valid()) return false;
    outHandle[k] = h;
    slots.push_back({o.binding, h});
    outputs[k].assign(o.capElements, 0.0f);
    if (o.variableCardinality) {
      const std::uint32_t zero = 0u;
      BufferHandle cnt =
          device_.createStorageBuffer(sizeof(std::uint32_t), &zero, sizeof(zero));
      if (!cnt.valid()) return false;
      cntHandle[k] = cnt;
      slots.push_back({o.binding + 1u, cnt});
    }
  }

  std::sort(slots.begin(), slots.end(),
            [](const Slot& a, const Slot& b) { return a.binding < b.binding; });
  std::vector<BufferHandle> bindings;
  bindings.reserve(slots.size());
  for (const auto& s : slots) bindings.push_back(s.buf);

  ComputePipelineHandle pipe =
      device_.createComputePipeline(spec.wgsl.c_str(), spec.entryPoint.c_str());
  if (!pipe.valid()) return false;  // Tint rejected -> lastDeviceError() set.

  if (!device_.dispatchCompute(pipe, bindings, spec.dispatchX, spec.dispatchY,
                               spec.dispatchZ)) {
    return false;
  }

  // Read each output back; for a variable-cardinality output, read its atomic
  // counter and compact to the live prefix (min(count, cap) elements).
  for (std::size_t k = 0; k < spec.outputs.size(); ++k) {
    const auto& o = spec.outputs[k];
    if (cntHandle[k].valid()) {
      std::uint32_t count = 0;
      if (!device_.readBuffer(cntHandle[k], 0, sizeof(std::uint32_t),
                              reinterpret_cast<std::uint8_t*>(&count))) {
        return false;
      }
      outCounts[k] = count;
      const std::uint32_t live = std::min(count, o.capElements);
      outputs[k].assign(live, 0.0f);
      if (live == 0) continue;
    }
    if (outputs[k].empty()) continue;
    if (!device_.readBuffer(
            outHandle[k], 0, outputs[k].size() * sizeof(float),
            reinterpret_cast<std::uint8_t*>(outputs[k].data()))) {
      return false;
    }
  }
  return true;
}

bool ComputeStage::runStft(const std::vector<float>& samples,
                           std::uint32_t fftSize, std::uint32_t hop,
                           std::vector<float>& mag, std::uint32_t& frames,
                           std::uint32_t& bins) {
  frames = 0;
  bins = fftSize / 2u + 1u;
  mag.clear();
  if (fftSize == 0 || hop == 0 || samples.size() < fftSize) return false;
  frames = static_cast<std::uint32_t>((samples.size() - fftSize) / hop) + 1u;

  struct {
    std::uint32_t fftSize;
    std::uint32_t hop;
    std::uint32_t frames;
    std::uint32_t bins;
  } params{fftSize, hop, frames, bins};

  const std::size_t magCount = static_cast<std::size_t>(frames) * bins;
  mag.assign(magCount, 0.0f);

  BufferHandle sampBuf = device_.createStorageBuffer(
      samples.size() * sizeof(float), samples.data(),
      samples.size() * sizeof(float));
  BufferHandle magBuf =
      device_.createStorageBuffer(magCount * sizeof(float), nullptr, 0);
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), &params, sizeof(params));
  if (!sampBuf.valid() || !magBuf.valid() || !paramBuf.valid()) return false;

  const std::string wgsl = buildStftKernelWgsl();
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  // @workgroup_size(8,8,1) -> ceil(frames/8) x ceil(bins/8) workgroups.
  const std::uint32_t gx = (frames + 7u) / 8u;
  const std::uint32_t gy = (bins + 7u) / 8u;
  if (!device_.dispatchCompute(pipe, {sampBuf, magBuf, paramBuf}, gx, gy, 1u))
    return false;

  return device_.readBuffer(magBuf, 0, magCount * sizeof(float),
                            reinterpret_cast<std::uint8_t*>(mag.data()));
}

bool ComputeStage::runMarchingSquares(const std::vector<float>& field,
                                      std::uint32_t gridW, std::uint32_t gridH,
                                      float iso, std::uint32_t capSegments,
                                      std::vector<float>& segs,
                                      std::uint32_t& count) {
  count = 0;
  segs.clear();
  if (gridW < 2 || gridH < 2 || capSegments == 0) return false;
  if (field.size() < static_cast<std::size_t>(gridW) * gridH) return false;

  struct {
    std::uint32_t gridW;
    std::uint32_t gridH;
    std::uint32_t capSegs;
    float iso;
  } params{gridW, gridH, capSegments, iso};

  // Bounded output: 4 f32 per segment + a u32 atomic counter.
  std::vector<float> segBuf(static_cast<std::size_t>(capSegments) * 4u, 0.0f);
  const std::uint32_t zero = 0u;

  BufferHandle fieldBuf = device_.createStorageBuffer(
      field.size() * sizeof(float), field.data(), field.size() * sizeof(float));
  BufferHandle segsBuf = device_.createStorageBuffer(
      segBuf.size() * sizeof(float), segBuf.data(), segBuf.size() * sizeof(float));
  BufferHandle cntBuf =
      device_.createStorageBuffer(sizeof(std::uint32_t), &zero, sizeof(zero));
  BufferHandle paramBuf =
      device_.createStorageBuffer(sizeof(params), &params, sizeof(params));
  if (!fieldBuf.valid() || !segsBuf.valid() || !cntBuf.valid() ||
      !paramBuf.valid()) {
    return false;
  }

  const std::string wgsl = buildMarchingSquaresKernelWgsl();
  ComputePipelineHandle pipe =
      device_.createComputePipeline(wgsl.c_str(), "main");
  if (!pipe.valid()) return false;

  // One invocation per cell; @workgroup_size(8,8,1).
  const std::uint32_t gx = ((gridW - 1u) + 7u) / 8u;
  const std::uint32_t gy = ((gridH - 1u) + 7u) / 8u;
  if (!device_.dispatchCompute(pipe, {fieldBuf, segsBuf, cntBuf, paramBuf}, gx,
                               gy, 1u)) {
    return false;
  }

  // Read the counter, then compact: the live count is min(count, cap) segments.
  if (!device_.readBuffer(cntBuf, 0, sizeof(std::uint32_t),
                          reinterpret_cast<std::uint8_t*>(&count))) {
    return false;
  }
  const std::uint32_t live = std::min(count, capSegments);
  segs.assign(static_cast<std::size_t>(live) * 4u, 0.0f);
  if (live == 0) return true;
  return device_.readBuffer(segsBuf, 0, segs.size() * sizeof(float),
                            reinterpret_cast<std::uint8_t*>(segs.data()));
}

}  // namespace dc
