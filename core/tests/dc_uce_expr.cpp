// ENC-616a — Expression DSL tests (RESEARCH §5.3).
//
// Covers: arithmetic, comparisons, logic, ternary, the math/reducer functions,
// operator precedence + parentheses, column refs, and — critically — that type
// errors are rejected at COMPILE (parse/type-check) time, never at a row.
#include "dc/transform/Expr.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int passed = 0;
static int failed = 0;
static void check(bool c, const char* name) {
  if (c) { std::printf("  PASS: %s\n", name); ++passed; }
  else { std::fprintf(stderr, "  FAIL: %s\n", name); ++failed; }
}

using namespace dc;

// schema: a@0, b@1, c@2 — all numeric.
static std::vector<ColumnBinding> schema() {
  return {{"a", 0, ExprKind::Num}, {"b", 1, ExprKind::Num},
          {"c", 2, ExprKind::Num}};
}

// Compile + eval a numeric expression over one row {a,b,c}.
static double evalN(const char* src, double a, double b, double c, bool* okOut = nullptr) {
  auto r = compileExpr(src, schema());
  if (okOut) *okOut = r.ok;
  if (!r.ok || r.expr.resultKind != ExprKind::Num) return std::nan("");
  return evalNum(*r.expr.root, {a, b, c});
}
static bool evalB(const char* src, double a, double b, double c, bool* okOut = nullptr) {
  auto r = compileExpr(src, schema());
  if (okOut) *okOut = r.ok;
  if (!r.ok || r.expr.resultKind != ExprKind::Bool) return false;
  return evalBool(*r.expr.root, {a, b, c});
}
static bool nearly(double x, double y) { return std::fabs(x - y) < 1e-9; }

int main() {
  std::printf("=== ENC-616a Expression DSL ===\n");

  // ----- arithmetic + precedence -------------------------------------------
  check(nearly(evalN("1 + 2 * 3", 0, 0, 0), 7.0), "precedence * over +");
  check(nearly(evalN("(1 + 2) * 3", 0, 0, 0), 9.0), "parens override precedence");
  check(nearly(evalN("a + b - c", 5, 3, 2), 6.0), "column add/sub");
  check(nearly(evalN("a / b", 7, 2, 0), 3.5), "division");
  check(nearly(evalN("a % b", 7, 3, 0), 1.0), "modulo");
  check(nearly(evalN("-a + 1", 4, 0, 0), -3.0), "unary minus");
  check(nearly(evalN("2 - -3", 0, 0, 0), 5.0), "minus of unary minus");

  // ----- comparisons + logic + ternary -------------------------------------
  check(evalB("a > b", 5, 3, 0), "greater-than true");
  check(!evalB("a > b", 1, 3, 0), "greater-than false");
  check(evalB("a >= b && b >= c", 3, 2, 1), "chained && comparisons");
  check(evalB("a < b || b < c", 5, 4, 9), "|| short-circuit-friendly");
  check(evalB("!(a == b)", 1, 2, 0), "not of equality");
  check(evalB("a == a", 3, 0, 0), "num equality");
  check(evalB("true && (1 < 2)", 0, 0, 0), "bool literal in logic");
  check(nearly(evalN("a > b ? a : b", 3, 7, 0), 7.0), "ternary picks larger");
  check(nearly(evalN("a > b ? a : b", 9, 7, 0), 9.0), "ternary picks larger 2");
  // nested ternary
  check(nearly(evalN("a > 0 ? (b > 0 ? 1 : 2) : 3", 1, -1, 0), 2.0),
        "nested ternary");

  // ----- math / reducer functions ------------------------------------------
  check(nearly(evalN("abs(-5)", 0, 0, 0), 5.0), "abs");
  check(nearly(evalN("sqrt(16)", 0, 0, 0), 4.0), "sqrt");
  check(nearly(evalN("pow(2, 10)", 0, 0, 0), 1024.0), "pow");
  check(nearly(evalN("min(3, 1, 2)", 0, 0, 0), 1.0), "min variadic");
  check(nearly(evalN("max(3, 1, 9, 4)", 0, 0, 0), 9.0), "max variadic");
  check(nearly(evalN("floor(3.7)", 0, 0, 0), 3.0), "floor");
  check(nearly(evalN("ceil(3.2)", 0, 0, 0), 4.0), "ceil");
  check(nearly(evalN("round(2.5)", 0, 0, 0), 3.0), "round");
  check(nearly(evalN("clamp(15, 0, 10)", 0, 0, 0), 10.0), "clamp hi");
  check(nearly(evalN("clamp(-5, 0, 10)", 0, 0, 0), 0.0), "clamp lo");
  check(nearly(evalN("sign(-3) + sign(4)", 0, 0, 0), 0.0), "sign");
  check(nearly(evalN("log(exp(1))", 0, 0, 0), 1.0), "log(exp(1))==1");
  check(nearly(evalN("hypot(3, 4)", 0, 0, 0), 5.0), "hypot");
  // isnan / isfinite return bool
  check(evalB("isnan(0 / 0)", 0, 0, 0), "isnan(0/0) -> true");
  check(!evalB("isfinite(1 / 0)", 0, 0, 0), "isfinite(inf) -> false");
  check(evalB("isfinite(a)", 42, 0, 0), "isfinite(finite) -> true");

  // ----- IEEE semantics: divide-by-zero is inf/nan, never a trap -----------
  check(std::isinf(evalN("1 / 0", 0, 0, 0)), "1/0 -> inf (no trap)");

  // ----- TYPE ERRORS rejected at COMPILE (fail fast) -----------------------
  {
    bool ok = true;
    (void)evalN("1 + true", 0, 0, 0, &ok);
    check(!ok, "REJECT num + bool");
    ok = true; (void)evalB("!a", 0, 0, 0, &ok);
    check(!ok, "REJECT '!' on numeric column");
    ok = true; (void)evalN("a && b", 0, 0, 0, &ok);
    check(!ok, "REJECT '&&' on numeric operands");
    ok = true; (void)evalN("a ? b : c", 0, 0, 0, &ok);
    check(!ok, "REJECT non-bool ternary condition");
    ok = true; (void)evalN("a > b ? 1 : true", 0, 0, 0, &ok);
    check(!ok, "REJECT mixed-kind ternary arms");
    // unknown column
    ok = true; (void)evalN("zzz + 1", 0, 0, 0, &ok);
    check(!ok, "REJECT unknown column");
    // unknown function
    ok = true; (void)evalN("frobnicate(a)", 0, 0, 0, &ok);
    check(!ok, "REJECT unknown function");
    // wrong arity
    ok = true; (void)evalN("pow(2)", 0, 0, 0, &ok);
    check(!ok, "REJECT pow with 1 arg");
    // malformed syntax
    ok = true; (void)evalN("1 + + ", 0, 0, 0, &ok);
    check(!ok, "REJECT malformed syntax");
    ok = true; (void)evalN("(1 + 2", 0, 0, 0, &ok);
    check(!ok, "REJECT unbalanced paren");
    // a predicate used as a formula must be num -> but here we only check kind
    ok = true; auto r = compileExpr("a > b", schema());
    check(r.ok && r.expr.resultKind == ExprKind::Bool,
          "comparison compiles to bool kind");
  }

  std::printf("=== ENC-616a Expr Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
