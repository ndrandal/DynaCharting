// ENC-617a (Epic ENC-617) — AST -> WGSL codegen unit test (GPU-FREE).
//
// The AST->WGSL backend is testable WITHOUT a GPU: compile a representative
// expression with the SAME compileExpr() the CPU path uses, emit its WGSL, and
// assert the generated string has the right intrinsics / storage-buffer loads /
// structure. This is the fast-build half of ENC-617a's validation; the
// byte-identical GPU==CPU proof is the Dawn test (dc_enc617a_dawn_compute_expr).
#include "dc/transform/Expr.hpp"
#include "dc/transform/ExprWgsl.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

using namespace dc;

// schema: a@0, b@1, c@2 — all numeric (mirrors dc_uce_expr.cpp).
static std::vector<ColumnBinding> schema() {
  return {{"a", 0, ExprKind::Num}, {"b", 1, ExprKind::Num},
          {"c", 2, ExprKind::Num}};
}

// Compile `src` and emit the WGSL expression text (no surrounding kernel).
static std::string wgsl(const char* src) {
  auto r = compileExpr(src, schema());
  if (!r.ok) {
    std::fprintf(stderr, "  compile FAILED for '%s': %s\n", src, r.error.c_str());
    return "<compile-error>";
  }
  return emitExprWgsl(*r.expr.root);
}

// True if `hay` contains `needle`.
static bool has(const std::string& hay, const char* needle) {
  return hay.find(needle) != std::string::npos;
}

int main() {
  std::printf("=== ENC-617a AST -> WGSL codegen ===\n");

  // ----- column refs become slot-indexed storage loads ---------------------
  check(wgsl("a") == "col0[i]", "column a -> col0[i]");
  check(wgsl("c") == "col2[i]", "column c -> col2[i]");

  // ----- literals carry the f32 'f' suffix + a decimal point ---------------
  check(wgsl("5") == "5.0f", "int literal -> 5.0f");
  check(has(wgsl("3.5"), "3.5") && has(wgsl("3.5"), "f"), "float literal has f");

  // ----- arithmetic + precedence (fully parenthesized) ---------------------
  check(wgsl("a + b") == "(col0[i] + col1[i])", "add");
  check(wgsl("1 + 2 * 3") == "(1.0f + (2.0f * 3.0f))", "precedence preserved");
  check(wgsl("-a") == "(-col0[i])", "unary neg");

  // ----- division stays IEEE '/' (no guard) --------------------------------
  check(wgsl("a / b") == "(col0[i] / col1[i])", "division is plain '/'");

  // ----- '%' and mod() both expand to truncated remainder ------------------
  check(has(wgsl("a % b"), "trunc("), "'%' expands via trunc()");
  check(has(wgsl("mod(a, b)"), "trunc("), "mod() expands via trunc()");

  // ----- comparisons + logic -> bool sub-expressions -----------------------
  check(wgsl("a > b") == "(col0[i] > col1[i])", "greater-than");
  check(wgsl("a >= b && b >= c") ==
            "((col0[i] >= col1[i]) && (col1[i] >= col2[i]))",
        "chained && comparisons");
  check(wgsl("!(a == b)") == "(!(col0[i] == col1[i]))", "not of equality");

  // ----- ternary -> WGSL select(false, true, cond) -------------------------
  check(wgsl("a > b ? a : b") ==
            "select(col1[i], col0[i], (col0[i] > col1[i]))",
        "ternary -> select(false,true,cond)");

  // ----- math fns -> intrinsics --------------------------------------------
  check(has(wgsl("abs(a)"), "abs(col0[i])"), "abs -> abs()");
  check(has(wgsl("sqrt(a)"), "sqrt(col0[i])"), "sqrt -> sqrt()");
  check(has(wgsl("exp(a)"), "exp(col0[i])"), "exp -> exp()");
  check(has(wgsl("log(a)"), "log(col0[i])"), "log -> log()");
  check(has(wgsl("log2(a)"), "log2(col0[i])"), "log2 -> log2()");
  check(has(wgsl("sin(a)"), "sin(col0[i])"), "sin -> sin()");
  check(has(wgsl("cos(a)"), "cos(col0[i])"), "cos -> cos()");
  check(has(wgsl("atan2(a, b)"), "atan2(col0[i], col1[i])"), "atan2 -> atan2()");
  check(has(wgsl("pow(a, b)"), "pow(col0[i], col1[i])"), "pow -> pow()");
  check(has(wgsl("clamp(a, b, c)"), "clamp(col0[i], col1[i], col2[i])"),
        "clamp -> clamp()");
  check(has(wgsl("floor(a)"), "floor(col0[i])"), "floor -> floor()");
  check(has(wgsl("sign(a)"), "sign(col0[i])"), "sign -> sign()");

  // ----- fns without a direct intrinsic -> exact WGSL equivalents ----------
  check(has(wgsl("hypot(a, b)"), "length(vec2<f32>(col0[i], col1[i]))"),
        "hypot -> length(vec2)");
  check(has(wgsl("log10(a)"), "log(col0[i])") && has(wgsl("log10(a)"), "0.434"),
        "log10 -> log()*(1/ln10)");
  check(has(wgsl("cbrt(a)"), "pow(abs(col0[i])") && has(wgsl("cbrt(a)"), "sign("),
        "cbrt -> sign*pow(abs,1/3)");
  check(wgsl("isnan(a)") == "(col0[i] != col0[i])", "isnan -> x != x");
  check(has(wgsl("isfinite(a)"), "abs(col0[i]) <= 3.4028235e38f"),
        "isfinite -> abs(x)<=FLT_MAX");

  // ----- N-ary min/max -> left-folded nested binary intrinsics -------------
  check(wgsl("min(a, b)") == "min(col0[i], col1[i])", "min(2) -> min()");
  check(wgsl("min(a, b, c)") == "min(min(col0[i], col1[i]), col2[i])",
        "min(3) -> nested min()");
  check(wgsl("max(a, b, c)") == "max(max(col0[i], col1[i]), col2[i])",
        "max(3) -> nested max()");

  // ----- full formula kernel: bindings + @compute + guard + write ----------
  {
    auto r = compileExpr("a * 2 + b", schema());
    check(r.ok, "formula compiles");
    std::string k = buildFormulaKernelWgsl(r.expr, 3);
    check(has(k, "@group(0) @binding(0) var<storage, read> col0 : array<f32>;"),
          "formula: col0 binding 0");
    check(has(k, "@group(0) @binding(2) var<storage, read> col2 : array<f32>;"),
          "formula: col2 binding 2");
    check(has(k, "@binding(3) var<storage, read_write> outCol : array<f32>;"),
          "formula: output f32 at binding numColumns");
    check(has(k, "@compute @workgroup_size(64)"), "formula: workgroup_size 64");
    check(has(k, "if (i >= arrayLength(&outCol)) { return; }"),
          "formula: bounds guard");
    check(has(k, "outCol[i] = ((col0[i] * 2.0f) + col1[i]);"),
          "formula: per-row write of the expression");
  }

  // ----- full filter mask kernel: bool predicate -> u32 0/1 mask -----------
  {
    auto r = compileExpr("a > b", schema());
    check(r.ok, "filter compiles");
    std::string k = buildFilterMaskKernelWgsl(r.expr, 3);
    check(has(k, "@binding(3) var<storage, read_write> outCol : array<u32>;"),
          "filter: output u32 mask at binding numColumns");
    check(has(k, "@compute @workgroup_size(64)"), "filter: workgroup_size 64");
    check(has(k, "outCol[i] = select(0u, 1u, (col0[i] > col1[i]));"),
          "filter: per-row 0/1 mask write");
  }

  std::printf("\n=== %d passed, %d failed ===\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
