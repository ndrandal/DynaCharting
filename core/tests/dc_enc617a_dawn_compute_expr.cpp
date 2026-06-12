// ENC-617a (Epic ENC-617) — GPU filter + formula on Dawn: BYTE-IDENTICAL to the
// CPU evaluator (native / lavapipe).
//
// The functional proof that the AST->WGSL codegen (ExprWgsl) + the reusable
// ComputeStage produce results IDENTICAL to the CPU Expression DSL evaluator
// (Expr.cpp's evalNum/evalBool) — the §5.3 promise that "the same AST compiles
// to either a WASM-CPU tight loop or a generated WGSL body, so filter/formula
// get the GPU fast path for free." Builds on the ENC-591 compute round-trip.
//
//   * formula : compile a NUM expression, run it on the GPU (runFormula), and
//     assert every output f32 is BIT-EQUAL to static_cast<float>(evalNum) over
//     the same row (the FormulaTransform sink). The expressions are chosen to be
//     exactly reproducible in f32 (arithmetic / min/max / abs / floor / clamp /
//     comparisons-in-a-ternary) so an EXACT compare is the correct bar — the
//     same discipline as the ENC-591 *2.0 lossless-scale round-trip.
//   * filter  : compile a BOOL predicate, run it on the GPU (runFilter), and
//     assert the surviving row indices EXACTLY equal the CPU evalBool survivor
//     set (the FilterTransform compaction).
//
// f32 ONLY (WebGPU has no f64): the CPU reference reads each column's f32 value
// widened to double — exactly the bytes the GPU loads — so the two paths agree
// bit-for-bit on these ops.
//
// On this headless box force lavapipe if no HW adapter:
//   VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json
#include "dc/gpu/ComputeStage.hpp"
#include "dc/transform/Expr.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace dc;

static int failures = 0;
static void check(bool c, const char* msg) {
  if (c) {
    std::printf("  PASS: %s\n", msg);
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", msg);
    ++failures;
  }
}

// schema: a@0, b@1, c@2 — the three input columns, in slot order.
static std::vector<ColumnBinding> schema() {
  return {{"a", 0, ExprKind::Num}, {"b", 1, ExprKind::Num},
          {"c", 2, ExprKind::Num}};
}

// Build the CPU evaluator's per-row double vector from the f32 columns at row i
// (widening f32 -> double, exactly the bytes the GPU storage buffers hold).
static std::vector<double> rowAt(const std::vector<std::vector<float>>& cols,
                                 std::size_t i) {
  std::vector<double> row(cols.size());
  for (std::size_t s = 0; s < cols.size(); ++s) {
    row[s] = static_cast<double>(cols[s][i]);
  }
  return row;
}

// FORMULA: GPU runFormula(expr) must be bit-equal to static_cast<float>(evalNum)
// per row (the FormulaTransform sink).
static void testFormula(ComputeStage& stage,
                        const std::vector<std::vector<float>>& cols,
                        const char* src, const char* label) {
  auto r = compileExpr(src, schema());
  if (!r.ok || r.expr.resultKind != ExprKind::Num) {
    std::fprintf(stderr, "  FAIL: formula '%s' did not compile to Num: %s\n", src,
                 r.error.c_str());
    ++failures;
    return;
  }
  std::vector<float> gpu;
  if (!stage.runFormula(cols, r.expr, gpu)) {
    std::fprintf(stderr, "  FAIL: runFormula('%s') returned false\n", src);
    ++failures;
    return;
  }
  const std::size_t rows = cols.front().size();
  bool ok = (gpu.size() == rows);
  for (std::size_t i = 0; ok && i < rows; ++i) {
    const float cpu = static_cast<float>(evalNum(*r.expr.root, rowAt(cols, i)));
    // Bit-exact compare (memcmp, so a NaN bit-pattern would also have to match).
    if (std::memcmp(&gpu[i], &cpu, sizeof(float)) != 0) {
      std::fprintf(stderr,
                   "    mismatch at row %zu: cpu=%.9g gpu=%.9g  (expr '%s')\n", i,
                   cpu, gpu[i], src);
      ok = false;
    }
  }
  check(ok, label);
}

// FILTER: GPU runFilter(pred) survivor indices must exactly equal the CPU
// evalBool survivor set (the FilterTransform compaction).
static void testFilter(ComputeStage& stage,
                       const std::vector<std::vector<float>>& cols,
                       const char* src, const char* label) {
  auto r = compileExpr(src, schema());
  if (!r.ok || r.expr.resultKind != ExprKind::Bool) {
    std::fprintf(stderr, "  FAIL: filter '%s' did not compile to Bool: %s\n", src,
                 r.error.c_str());
    ++failures;
    return;
  }
  std::vector<std::uint32_t> gpu;
  if (!stage.runFilter(cols, r.expr, gpu)) {
    std::fprintf(stderr, "  FAIL: runFilter('%s') returned false\n", src);
    ++failures;
    return;
  }
  const std::size_t rows = cols.front().size();
  std::vector<std::uint32_t> cpu;
  for (std::uint32_t i = 0; i < rows; ++i) {
    if (evalBool(*r.expr.root, rowAt(cols, i))) cpu.push_back(i);
  }
  bool ok = (gpu == cpu);
  if (!ok) {
    std::fprintf(stderr, "    survivor mismatch: cpu kept %zu, gpu kept %zu\n",
                 cpu.size(), gpu.size());
  }
  check(ok, label);
}

int main() {
  dc::DawnDevice dev;
  if (!dev.init()) {
    std::fprintf(stderr, "DawnDevice::init failed: %s\n",
                 dev.errorMessage().c_str());
    std::fprintf(stderr,
                 "Hint: VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/"
                 "lvp_icd.x86_64.json to force lavapipe.\n");
    return 1;
  }
  std::printf("DawnDevice up: backend=%s adapter=\"%s\"\n",
              dev.backendName().c_str(), dev.adapterName().c_str());

  // --- Input: three f32 columns, N not a multiple of 64 (exercise round-up +
  // the in-kernel bounds guard). Values are exactly representable in f32 so the
  // f32-stable ops below compare bit-exactly. ------------------------------
  const std::uint32_t kN = 1000;
  std::vector<std::vector<float>> cols(3, std::vector<float>(kN));
  for (std::uint32_t i = 0; i < kN; ++i) {
    cols[0][i] = (static_cast<float>(i) - 500.0f) * 0.25f;   // a: signed, frac
    cols[1][i] = static_cast<float>((i % 17)) - 8.0f;        // b: small range
    cols[2][i] = static_cast<float>((i % 5) + 1);            // c: 1..5
  }

  ComputeStage stage(dev);

  std::printf("\n-- FORMULA (GPU == static_cast<float>(evalNum), bit-exact) --\n");
  testFormula(stage, cols, "a + b * c", "formula: a + b*c");
  testFormula(stage, cols, "a * 2 - b", "formula: a*2 - b");
  testFormula(stage, cols, "abs(a) + min(b, c)", "formula: abs(a)+min(b,c)");
  testFormula(stage, cols, "max(a, b, c)", "formula: max(a,b,c) N-ary");
  testFormula(stage, cols, "clamp(a, b, c + 10)", "formula: clamp(a,b,c+10)");
  testFormula(stage, cols, "floor(a) + c", "formula: floor(a)+c");
  testFormula(stage, cols, "a > b ? a : b", "formula: ternary a>b?a:b");
  testFormula(stage, cols, "a - c * (b + 1)", "formula: a - c*(b+1)");

  std::printf("\n-- FILTER (GPU survivor set == CPU evalBool survivor set) --\n");
  testFilter(stage, cols, "a > b", "filter: a > b");
  testFilter(stage, cols, "a >= 0 && b < 5", "filter: a>=0 && b<5");
  testFilter(stage, cols, "a < b || c == 3", "filter: a<b || c==3");
  testFilter(stage, cols, "!(a == b) && c >= 2", "filter: !(a==b) && c>=2");
  testFilter(stage, cols, "min(a, b) <= c", "filter: min(a,b) <= c");

  if (failures == 0) {
    std::printf(
        "\nENC-617a GPU filter+formula: OK (%u rows, every formula output "
        "bit-equal to evalNum, every filter survivor set equal to evalBool; "
        "backend=%s)\n",
        kN, dev.backendName().c_str());
    std::printf(
        "VERDICT(native): AST->WGSL filter/formula run BYTE-IDENTICAL to the "
        "CPU evaluator through the reusable ComputeStage.\n");
    return 0;
  }
  std::fprintf(stderr, "\nENC-617a GPU filter+formula: %d FAILURES\n", failures);
  return 1;
}
