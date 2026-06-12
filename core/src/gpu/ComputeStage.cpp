// ENC-617a — ComputeStage implementation. See dc/gpu/ComputeStage.hpp.
//
// The generalization of the ENC-591 spike: instead of one hard-coded `data[i]*=2`
// kernel over one buffer, this uploads N input columns + an output column, builds
// the pipeline from a GENERATED kernel (ExprWgsl), dispatches, and reads back —
// the column-oriented reuse of DawnDevice's ENC-590/591 compute API.
#include "dc/gpu/ComputeStage.hpp"

#include "dc/transform/ExprWgsl.hpp"

#include <cstring>

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

}  // namespace dc
