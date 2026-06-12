// ENC-617a — AST -> WGSL codegen. See dc/transform/ExprWgsl.hpp for the contract.
//
// This is the SECOND backend over the ExprNode AST (the first is Expr.cpp's CPU
// tree-walk). emitExprWgsl() is structurally the mirror of evalNum/evalBool: one
// case per NodeType, each emitting the WGSL that computes the same value in f32.
#include "dc/transform/ExprWgsl.hpp"

#include <cstdio>

namespace dc {
namespace {

// Format a numeric literal as a WGSL f32 constant. WGSL requires a decimal point
// or an `f` suffix for a float literal; we always append `f` and force a decimal
// representation. %.17g round-trips a double exactly; the f32 narrow then happens
// in-shader (the literal's f32 value matches the CPU path's double->f32 narrow at
// the formula sink for any literal exactly representable, which authored
// constants are).
std::string numLit(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  std::string s(buf);
  // Ensure it reads as a float literal: if there is no '.', 'e'/'E', or 'inf'/
  // 'nan', append ".0" before the 'f' suffix so e.g. "5" -> "5.0f".
  bool hasDotOrExp = false;
  for (char c : s) {
    if (c == '.' || c == 'e' || c == 'E') { hasDotOrExp = true; break; }
  }
  if (!hasDotOrExp) s += ".0";
  s += "f";
  return s;
}

const char* binSym(BinaryOp op) {
  switch (op) {
    case BinaryOp::Add: return "+";
    case BinaryOp::Sub: return "-";
    case BinaryOp::Mul: return "*";
    case BinaryOp::Lt: return "<";
    case BinaryOp::Le: return "<=";
    case BinaryOp::Gt: return ">";
    case BinaryOp::Ge: return ">=";
    case BinaryOp::Eq: return "==";
    case BinaryOp::Ne: return "!=";
    case BinaryOp::And: return "&&";
    case BinaryOp::Or: return "||";
    // Div / Mod are emitted specially (see emit()).
    default: return "?";
  }
}

void emit(const ExprNode& n, std::string& out);

// Emit a function call. Most map 1:1 to a WGSL intrinsic; the handful without a
// direct intrinsic are expanded to an exact f32 equivalent of the CPU semantics
// (Expr.cpp's evalNum):
//   cbrt(x)      -> sign(x)*pow(abs(x),1/3)   (WGSL pow needs a non-neg base)
//   log10(x)     -> log(x)*(1/ln10)
//   sign(x)      -> sign() intrinsic (matches (v>0)-(v<0): -1/0/+1)
//   mod(a,b)     -> a - b*trunc(a/b)          (C fmod truncated remainder)
//   N-ary min/max-> nested binary min()/max()
//   isnan(x)     -> (x != x)                  (a NaN is the only non-self-equal)
//   isfinite(x)  -> (abs(x) <= largest f32 finite && x == x)
void emitCall(const ExprNode& n, std::string& out) {
  auto a = [&](std::size_t k) -> const ExprNode& { return *n.children[k]; };
  auto intrinsic = [&](const char* name) {
    out += name; out += "(";
    for (std::size_t k = 0; k < n.children.size(); ++k) {
      if (k) out += ", ";
      emit(a(k), out);
    }
    out += ")";
  };
  switch (n.fn) {
    case Fn::Abs: intrinsic("abs"); return;
    case Fn::Sqrt: intrinsic("sqrt"); return;
    case Fn::Exp: intrinsic("exp"); return;
    case Fn::Log: intrinsic("log"); return;
    case Fn::Log2: intrinsic("log2"); return;
    case Fn::Floor: intrinsic("floor"); return;
    case Fn::Ceil: intrinsic("ceil"); return;
    case Fn::Round: intrinsic("round"); return;
    case Fn::Trunc: intrinsic("trunc"); return;
    case Fn::Sign: intrinsic("sign"); return;
    case Fn::Sin: intrinsic("sin"); return;
    case Fn::Cos: intrinsic("cos"); return;
    case Fn::Tan: intrinsic("tan"); return;
    case Fn::Atan: intrinsic("atan"); return;
    case Fn::Atan2: intrinsic("atan2"); return;
    case Fn::Pow: intrinsic("pow"); return;
    case Fn::Clamp: intrinsic("clamp"); return;
    case Fn::Hypot: {
      // WGSL has no hypot; length(vec2(a,b)) == sqrt(a*a+b*b).
      out += "length(vec2<f32>("; emit(a(0), out); out += ", "; emit(a(1), out);
      out += "))";
      return;
    }
    case Fn::Cbrt: {
      // pow needs a non-negative base; restore the sign explicitly.
      out += "(sign("; emit(a(0), out); out += ") * pow(abs(";
      emit(a(0), out); out += "), 0.3333333333333333f))";
      return;
    }
    case Fn::Log10: {
      out += "(log("; emit(a(0), out); out += ") * 0.4342944819032518f)";
      return;
    }
    case Fn::Mod: {
      // C fmod: a - b*trunc(a/b)  (truncated remainder; matches std::fmod sign).
      out += "("; emit(a(0), out); out += " - "; emit(a(1), out);
      out += " * trunc(("; emit(a(0), out); out += ") / ("; emit(a(1), out);
      out += ")))";
      return;
    }
    case Fn::IsNan: {
      out += "("; emit(a(0), out); out += " != "; emit(a(0), out); out += ")";
      return;
    }
    case Fn::IsFinite: {
      // finite == not inf and not nan. (abs(x) <= FLT_MAX) is false for +-inf and
      // for NaN (NaN compares false), so it captures both in one test.
      out += "(abs("; emit(a(0), out); out += ") <= 3.4028235e38f)";
      return;
    }
    case Fn::Min:
    case Fn::Max: {
      // N-ary reducer -> left-folded nested binary min()/max(). One arg folds to
      // that arg. min(a,b,c) -> min(min(a, b), c) (mirrors evalNum's fold order).
      const char* fn = (n.fn == Fn::Min) ? "min" : "max";
      const std::size_t c = n.children.size();
      for (std::size_t k = 0; k + 1 < c; ++k) { out += fn; out += "("; }
      emit(a(0), out);
      for (std::size_t k = 1; k < c; ++k) {
        out += ", "; emit(a(k), out); out += ")";
      }
      return;
    }
  }
}

// emit one node, fully parenthesized so the result is precedence-safe to nest.
void emit(const ExprNode& n, std::string& out) {
  switch (n.type) {
    case NodeType::NumLit:
      out += numLit(n.num);
      return;
    case NodeType::BoolLit:
      out += n.boolean ? "true" : "false";
      return;
    case NodeType::Column:
      // The slot's storage buffer, indexed by the kernel's row variable `i`.
      out += "col"; out += std::to_string(n.colSlot); out += "[i]";
      return;
    case NodeType::Unary:
      out += "(";
      out += (n.unaryOp == UnaryOp::Neg) ? "-" : "!";
      emit(*n.children[0], out);
      out += ")";
      return;
    case NodeType::Binary: {
      if (n.binaryOp == BinaryOp::Div) {
        // f32 IEEE division (/0 -> inf/nan, matching CPU). Plain WGSL `/`.
        out += "("; emit(*n.children[0], out); out += " / ";
        emit(*n.children[1], out); out += ")";
        return;
      }
      if (n.binaryOp == BinaryOp::Mod) {
        // `%` arithmetic mod: C fmod semantics, same expansion as Fn::Mod.
        out += "("; emit(*n.children[0], out); out += " - ";
        emit(*n.children[1], out); out += " * trunc((";
        emit(*n.children[0], out); out += ") / (";
        emit(*n.children[1], out); out += ")))";
        return;
      }
      out += "(";
      emit(*n.children[0], out);
      out += " "; out += binSym(n.binaryOp); out += " ";
      emit(*n.children[1], out);
      out += ")";
      return;
    }
    case NodeType::Ternary:
      // WGSL has no ?: operator; select(false_val, true_val, cond) is the exact
      // equivalent and is type-uniform (both arms share the AST's unified kind).
      out += "select(";
      emit(*n.children[2], out);  // false arm
      out += ", ";
      emit(*n.children[1], out);  // true arm
      out += ", ";
      emit(*n.children[0], out);  // condition
      out += ")";
      return;
    case NodeType::Call:
      emitCall(n, out);
      return;
  }
}

// Emit the @group(0) storage-buffer bindings shared by both kernels: one
// read-only array<f32> per input column (slot order), then the output buffer.
void emitBindings(std::string& out, std::uint32_t numColumns,
                  const char* outType) {
  for (std::uint32_t c = 0; c < numColumns; ++c) {
    out += "@group(0) @binding(" + std::to_string(c) +
           ") var<storage, read> col" + std::to_string(c) +
           " : array<f32>;\n";
  }
  out += "@group(0) @binding(" + std::to_string(numColumns) +
         ") var<storage, read_write> outCol : array<" + outType + ">;\n";
}

// Emit a keep-alive that references EVERY column binding so Tint's auto-derived
// bind-group layout retains all of them. Without this, an expression that does
// not mention some column (e.g. `a * 2 - b` never reads col2) would yield a
// pipeline layout missing that binding's slot — but ComputeStage always binds
// all numColumns buffers at sequential bindings 0..C-1, so CreateBindGroup would
// then fail validation ("binding index k not present in the bind group layout")
// and the dispatch would silently never write its output (all-zeros). A single
// phony-assigned arrayLength() touch per column forces every binding to be
// statically used WITHOUT affecting the computed result. (The slot k <-> binding
// k <-> uploaded-column k contract this file documents is what keeps the layout
// and the bind group in lock-step.)
void emitColumnKeepAlive(std::string& out, std::uint32_t numColumns) {
  for (std::uint32_t c = 0; c < numColumns; ++c) {
    out += "  _ = arrayLength(&col" + std::to_string(c) + ");\n";
  }
}

}  // namespace

std::string emitExprWgsl(const ExprNode& node) {
  std::string out;
  emit(node, out);
  return out;
}

std::string buildFormulaKernelWgsl(const CompiledExpr& expr,
                                   std::uint32_t numColumns) {
  std::string out;
  emitBindings(out, numColumns, "f32");
  out += "\n@compute @workgroup_size(64)\n";
  out += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  out += "  let i = gid.x;\n";
  out += "  if (i >= arrayLength(&outCol)) { return; }\n";
  emitColumnKeepAlive(out, numColumns);
  out += "  outCol[i] = ";
  emit(*expr.root, out);
  out += ";\n}\n";
  return out;
}

std::string buildFilterMaskKernelWgsl(const CompiledExpr& expr,
                                      std::uint32_t numColumns) {
  std::string out;
  emitBindings(out, numColumns, "u32");
  out += "\n@compute @workgroup_size(64)\n";
  out += "fn main(@builtin(global_invocation_id) gid : vec3<u32>) {\n";
  out += "  let i = gid.x;\n";
  out += "  if (i >= arrayLength(&outCol)) { return; }\n";
  emitColumnKeepAlive(out, numColumns);
  out += "  outCol[i] = select(0u, 1u, ";
  emit(*expr.root, out);
  out += ");\n}\n";
  return out;
}

}  // namespace dc
