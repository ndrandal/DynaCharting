// ENC-616a — Expression DSL for the CPU/WASM transform DAG (RESEARCH §5.3).
//
// WHAT THIS IS
// ------------
// A restricted, side-effect-free expression grammar — the "flavor (a)" escape
// hatch of RESEARCH §5.3. It is the shared language `filter` (a predicate) and
// `formula` (a per-row derived column) compile against, and the same vocabulary
// every later transform sub-PR reuses. Concretely the grammar is:
//
//   * column refs        bare `close`, or qualified `node.field`
//   * literals           numeric (12, 3.14, 1e3) and bool (true / false)
//   * arithmetic         + - * / %        (and unary -)
//   * comparisons        < <= > >= == !=
//   * logic              && || !
//   * ternary            cond ? a : b
//   * ~25 math fns       abs min max sqrt cbrt log log2 log10 exp pow floor ceil
//                        round trunc sign sin cos tan atan atan2 hypot clamp
//                        mod isnan isfinite  (plus min/max as N-ary reducers)
//
// HARD WALLS (the sandbox contract, RESEARCH §5.3): NO assignment, NO loops, NO
// user-defined functions, NO host access. Iteration is confined to the transform
// node that runs the compiled expression over its rows — the expression itself is
// a pure, total, single-row scalar function.
//
// THE AST IS BACKEND-NEUTRAL BY DESIGN
// ------------------------------------
// We parse to an explicit AST (not a closure) precisely so a LATER sub-PR can walk
// the SAME tree to emit a WGSL body for the GPU fast path (RESEARCH §5.3: "the same
// AST compiles to either a WASM-CPU tight loop or a generated WGSL body"). THIS PR
// implements ONLY the CPU eval path — a per-row tree-walk — but the node set, the
// resolved column-binding indices, and the fail-fast type check are all shaped for
// that future codegen.
//
// TYPING (fail fast, at parse/compile — RESEARCH §6.1 check 2)
// -----------------------------------------------------------
// Every value is one of two scalar kinds: Num (f32-domain double on CPU) or Bool.
// The compiler validates kinds bottom-up against each operator's signature and
// REJECTS at compile time (no row ever runs) on a mismatch — e.g. `1 + true`,
// `close ? a : b` (a non-bool condition), `!close` on a numeric column. Column
// refs are bound to a caller-supplied schema (name -> {slot, kind}); an unknown
// column is a compile error. This is the §6.1 "column-set inference" gate at the
// expression grain.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// ExprKind — the static type of an expression value. The DSL is bi-typed: every
// node is Num (a scalar real, f32 domain) or Bool. Comparisons/logic yield Bool;
// arithmetic/math yield Num; the ternary unifies its two arms.
// ---------------------------------------------------------------------------
enum class ExprKind : std::uint8_t { Num, Bool };

inline const char* toString(ExprKind k) {
  return k == ExprKind::Num ? "num" : "bool";
}

// ---------------------------------------------------------------------------
// AST node tags. Kept flat + explicit so a later pass can switch over them to
// emit WGSL. Binary/unary ops and the function set are enumerated (no string
// dispatch at eval time).
// ---------------------------------------------------------------------------
enum class NodeType : std::uint8_t {
  NumLit,    // numeric literal
  BoolLit,   // true / false
  Column,    // a bound column reference (carries a resolved slot index)
  Unary,     // -x  or  !x
  Binary,    // arithmetic / comparison / logic
  Ternary,   // cond ? a : b
  Call,      // a math/reducer function call
};

enum class UnaryOp : std::uint8_t { Neg, Not };

enum class BinaryOp : std::uint8_t {
  // arithmetic (Num,Num -> Num)
  Add, Sub, Mul, Div, Mod,
  // comparison (Num,Num -> Bool)
  Lt, Le, Gt, Ge,
  // equality (same-kind,same-kind -> Bool)
  Eq, Ne,
  // logic (Bool,Bool -> Bool); short-circuit
  And, Or,
};

// The fixed function vocabulary (RESEARCH §5.3 "~25 math/reducer functions").
// Arity is checked at compile time. min/max are variadic (>=1 arg) reducers.
enum class Fn : std::uint8_t {
  Abs, Sqrt, Cbrt, Exp, Log, Log2, Log10,
  Floor, Ceil, Round, Trunc, Sign,
  Sin, Cos, Tan, Atan,
  Atan2, Pow, Hypot, Mod,
  IsNan, IsFinite,
  Min, Max, Clamp,
};

// ---------------------------------------------------------------------------
// ExprNode — one AST node. Children are owned (unique_ptr) so the tree is a
// value the parser returns. `kind` is filled by the type checker.
// ---------------------------------------------------------------------------
struct ExprNode {
  NodeType type{NodeType::NumLit};
  ExprKind kind{ExprKind::Num};

  // NumLit
  double num{0.0};
  // BoolLit
  bool boolean{false};
  // Column: resolved 0-based slot into the row context the evaluator is given.
  // `colName` keeps the source name (diagnostics + later WGSL identifier).
  std::uint32_t colSlot{0};
  std::string colName;
  // Unary / Binary / Call op selector.
  UnaryOp unaryOp{UnaryOp::Neg};
  BinaryOp binaryOp{BinaryOp::Add};
  Fn fn{Fn::Abs};

  std::vector<std::unique_ptr<ExprNode>> children;
};

// ---------------------------------------------------------------------------
// ColumnBinding / Schema — how a column NAME resolves to a row-context SLOT and
// a static kind. The transform builds this from its input columns (all Num on
// the CPU path: f32/i32/cat are read as double; a bool column does not exist —
// bools are only produced by operators). The compiler errors on an unbound name.
// ---------------------------------------------------------------------------
struct ColumnBinding {
  std::string name;
  std::uint32_t slot{0};
  ExprKind kind{ExprKind::Num};
};

// ---------------------------------------------------------------------------
// CompiledExpr — a type-checked AST plus its declared result kind. Evaluate it
// per row by supplying that row's column values (indexed by the binding slots).
// ---------------------------------------------------------------------------
struct CompiledExpr {
  std::unique_ptr<ExprNode> root;
  ExprKind resultKind{ExprKind::Num};

  bool valid() const { return root != nullptr; }
};

// ---------------------------------------------------------------------------
// Result of compile(): either a CompiledExpr or a human-readable error (the
// fail-fast diagnostic surfaced to the manifest loader / test).
// ---------------------------------------------------------------------------
struct CompileResult {
  CompiledExpr expr;
  bool ok{false};
  std::string error;
};

// Parse + type-check `source` against `schema`. On success `ok==true` and
// `expr` holds the compiled tree; on any lex/parse/type error `ok==false` and
// `error` describes it (no tree is produced — fail fast, nothing runs).
CompileResult compileExpr(const std::string& source,
                          const std::vector<ColumnBinding>& schema);

// ---------------------------------------------------------------------------
// Per-row evaluation. `row` holds one value per binding SLOT (so row[binding.slot]
// is that column's value for the current row); all inputs are doubles on the CPU
// path. evalNum returns the scalar result of a Num expression; evalBool the result
// of a Bool expression. Behaviour is undefined-but-safe if the kinds disagree with
// what compile() reported — callers gate on resultKind. NaN/Inf propagate per IEEE
// (a divide-by-zero yields Inf/NaN, never a trap), matching the future WGSL path.
// ---------------------------------------------------------------------------
double evalNum(const ExprNode& node, const std::vector<double>& row);
bool evalBool(const ExprNode& node, const std::vector<double>& row);

}  // namespace dc
