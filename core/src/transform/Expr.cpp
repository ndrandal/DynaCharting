// ENC-616a — Expression DSL: lexer + Pratt parser + type checker + CPU evaluator.
// See dc/transform/Expr.hpp for the grammar, the sandbox walls, and the typing
// contract. This file is the ONLY place the grammar is realized.
#include "dc/transform/Expr.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <unordered_map>

namespace dc {
namespace {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------
enum class Tok : std::uint8_t {
  End, Num, Ident,
  Plus, Minus, Star, Slash, Percent,
  Lt, Le, Gt, Ge, EqEq, NeEq,
  AndAnd, OrOr, Bang,
  Question, Colon, Comma, LParen, RParen,
};

struct Token {
  Tok kind{Tok::End};
  double num{0.0};
  std::string text;  // for Ident
  std::size_t pos{0};
};

bool tokenize(const std::string& s, std::vector<Token>& out, std::string& err) {
  std::size_t i = 0;
  auto fail = [&](const std::string& m, std::size_t p) {
    err = "lex error at " + std::to_string(p) + ": " + m;
    return false;
  };
  while (i < s.size()) {
    char c = s[i];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
    std::size_t start = i;
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
      char* end = nullptr;
      double v = std::strtod(s.c_str() + i, &end);
      if (end == s.c_str() + i) return fail("malformed number", start);
      i = static_cast<std::size_t>(end - s.c_str());
      out.push_back({Tok::Num, v, {}, start});
      continue;
    }
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      std::size_t j = i;
      while (j < s.size() &&
             (std::isalnum(static_cast<unsigned char>(s[j])) || s[j] == '_' ||
              s[j] == '.')) {
        ++j;
      }
      out.push_back({Tok::Ident, 0.0, s.substr(i, j - i), start});
      i = j;
      continue;
    }
    auto two = [&](char a, char b) {
      return c == a && i + 1 < s.size() && s[i + 1] == b;
    };
    auto one = [&](Tok t) { out.push_back({t, 0.0, {}, start}); ++i; };
    if (two('<', '=')) { out.push_back({Tok::Le, 0, {}, start}); i += 2; continue; }
    if (two('>', '=')) { out.push_back({Tok::Ge, 0, {}, start}); i += 2; continue; }
    if (two('=', '=')) { out.push_back({Tok::EqEq, 0, {}, start}); i += 2; continue; }
    if (two('!', '=')) { out.push_back({Tok::NeEq, 0, {}, start}); i += 2; continue; }
    if (two('&', '&')) { out.push_back({Tok::AndAnd, 0, {}, start}); i += 2; continue; }
    if (two('|', '|')) { out.push_back({Tok::OrOr, 0, {}, start}); i += 2; continue; }
    switch (c) {
      case '+': one(Tok::Plus); break;
      case '-': one(Tok::Minus); break;
      case '*': one(Tok::Star); break;
      case '/': one(Tok::Slash); break;
      case '%': one(Tok::Percent); break;
      case '<': one(Tok::Lt); break;
      case '>': one(Tok::Gt); break;
      case '!': one(Tok::Bang); break;
      case '?': one(Tok::Question); break;
      case ':': one(Tok::Colon); break;
      case ',': one(Tok::Comma); break;
      case '(': one(Tok::LParen); break;
      case ')': one(Tok::RParen); break;
      default: return fail(std::string("illegal character '") + c + "'", start);
    }
  }
  out.push_back({Tok::End, 0.0, {}, i});
  return true;
}

// ---------------------------------------------------------------------------
// Function table: name -> (Fn, minArgs, maxArgs). maxArgs==0 => variadic.
// ---------------------------------------------------------------------------
struct FnSpec { Fn fn; int minArgs; int maxArgs; };  // maxArgs 0 = unbounded

const std::unordered_map<std::string, FnSpec>& fnTable() {
  static const std::unordered_map<std::string, FnSpec> t = {
      {"abs", {Fn::Abs, 1, 1}},     {"sqrt", {Fn::Sqrt, 1, 1}},
      {"cbrt", {Fn::Cbrt, 1, 1}},   {"exp", {Fn::Exp, 1, 1}},
      {"log", {Fn::Log, 1, 1}},     {"log2", {Fn::Log2, 1, 1}},
      {"log10", {Fn::Log10, 1, 1}}, {"floor", {Fn::Floor, 1, 1}},
      {"ceil", {Fn::Ceil, 1, 1}},   {"round", {Fn::Round, 1, 1}},
      {"trunc", {Fn::Trunc, 1, 1}}, {"sign", {Fn::Sign, 1, 1}},
      {"sin", {Fn::Sin, 1, 1}},     {"cos", {Fn::Cos, 1, 1}},
      {"tan", {Fn::Tan, 1, 1}},     {"atan", {Fn::Atan, 1, 1}},
      {"atan2", {Fn::Atan2, 2, 2}}, {"pow", {Fn::Pow, 2, 2}},
      {"hypot", {Fn::Hypot, 2, 2}}, {"mod", {Fn::Mod, 2, 2}},
      {"isnan", {Fn::IsNan, 1, 1}}, {"isfinite", {Fn::IsFinite, 1, 1}},
      {"min", {Fn::Min, 1, 0}},     {"max", {Fn::Max, 1, 0}},
      {"clamp", {Fn::Clamp, 3, 3}},
  };
  return t;
}

using NodePtr = std::unique_ptr<ExprNode>;

// ---------------------------------------------------------------------------
// Pratt parser. Precedence climbing over the token stream. Produces an UNTYPED
// tree; typeCheck() walks it afterwards to fill `kind` and reject mismatches.
// ---------------------------------------------------------------------------
class Parser {
 public:
  Parser(const std::vector<Token>& toks,
         const std::vector<ColumnBinding>& schema)
      : toks_(toks) {
    for (const auto& b : schema) byName_[b.name] = b;
  }

  // Parse a full expression; trailing tokens (other than End) are an error.
  NodePtr parse(std::string& err) {
    NodePtr n = parseExpr(0, err);
    if (!n) return nullptr;
    if (peek().kind != Tok::End) {
      err = errAt("unexpected trailing token");
      return nullptr;
    }
    return n;
  }

 private:
  const Token& peek() const { return toks_[i_]; }
  const Token& advance() { return toks_[i_++]; }
  std::string errAt(const std::string& m) const {
    return "parse error at " + std::to_string(peek().pos) + ": " + m;
  }

  // Binding power of an infix operator (higher binds tighter). Ternary handled
  // specially. 0 means "not an infix operator".
  static int lbp(Tok t) {
    switch (t) {
      case Tok::OrOr: return 1;
      case Tok::AndAnd: return 2;
      case Tok::EqEq: case Tok::NeEq: return 3;
      case Tok::Lt: case Tok::Le: case Tok::Gt: case Tok::Ge: return 4;
      case Tok::Plus: case Tok::Minus: return 5;
      case Tok::Star: case Tok::Slash: case Tok::Percent: return 6;
      default: return 0;
    }
  }

  static BinaryOp binOp(Tok t) {
    switch (t) {
      case Tok::Plus: return BinaryOp::Add;
      case Tok::Minus: return BinaryOp::Sub;
      case Tok::Star: return BinaryOp::Mul;
      case Tok::Slash: return BinaryOp::Div;
      case Tok::Percent: return BinaryOp::Mod;
      case Tok::Lt: return BinaryOp::Lt;
      case Tok::Le: return BinaryOp::Le;
      case Tok::Gt: return BinaryOp::Gt;
      case Tok::Ge: return BinaryOp::Ge;
      case Tok::EqEq: return BinaryOp::Eq;
      case Tok::NeEq: return BinaryOp::Ne;
      case Tok::AndAnd: return BinaryOp::And;
      case Tok::OrOr: return BinaryOp::Or;
      default: return BinaryOp::Add;
    }
  }

  // Precedence-climbing with the ternary as the lowest-precedence right-assoc op.
  NodePtr parseExpr(int minBp, std::string& err) {
    NodePtr lhs = parseUnary(err);
    if (!lhs) return nullptr;
    for (;;) {
      Tok t = peek().kind;
      // Ternary: cond ? a : b  (binds looser than any binary; right assoc).
      if (t == Tok::Question && minBp == 0) {
        advance();  // ?
        NodePtr a = parseExpr(0, err);
        if (!a) return nullptr;
        if (peek().kind != Tok::Colon) { err = errAt("expected ':' in ternary"); return nullptr; }
        advance();  // :
        NodePtr b = parseExpr(0, err);
        if (!b) return nullptr;
        auto n = std::make_unique<ExprNode>();
        n->type = NodeType::Ternary;
        n->children.push_back(std::move(lhs));
        n->children.push_back(std::move(a));
        n->children.push_back(std::move(b));
        lhs = std::move(n);
        continue;
      }
      int bp = lbp(t);
      if (bp == 0 || bp <= minBp) break;
      advance();  // operator
      NodePtr rhs = parseExpr(bp, err);
      if (!rhs) return nullptr;
      auto n = std::make_unique<ExprNode>();
      n->type = NodeType::Binary;
      n->binaryOp = binOp(t);
      n->children.push_back(std::move(lhs));
      n->children.push_back(std::move(rhs));
      lhs = std::move(n);
    }
    return lhs;
  }

  NodePtr parseUnary(std::string& err) {
    Tok t = peek().kind;
    if (t == Tok::Minus || t == Tok::Bang) {
      advance();
      NodePtr operand = parseUnary(err);
      if (!operand) return nullptr;
      auto n = std::make_unique<ExprNode>();
      n->type = NodeType::Unary;
      n->unaryOp = (t == Tok::Minus) ? UnaryOp::Neg : UnaryOp::Not;
      n->children.push_back(std::move(operand));
      return n;
    }
    return parsePrimary(err);
  }

  NodePtr parsePrimary(std::string& err) {
    const Token& tk = peek();
    switch (tk.kind) {
      case Tok::Num: {
        advance();
        auto n = std::make_unique<ExprNode>();
        n->type = NodeType::NumLit;
        n->num = tk.num;
        return n;
      }
      case Tok::LParen: {
        advance();
        NodePtr inner = parseExpr(0, err);
        if (!inner) return nullptr;
        if (peek().kind != Tok::RParen) { err = errAt("expected ')'"); return nullptr; }
        advance();
        return inner;
      }
      case Tok::Ident: {
        std::string name = tk.text;
        advance();
        if (name == "true" || name == "false") {
          auto n = std::make_unique<ExprNode>();
          n->type = NodeType::BoolLit;
          n->boolean = (name == "true");
          return n;
        }
        if (peek().kind == Tok::LParen) {
          return parseCall(name, tk.pos, err);
        }
        // column reference
        auto it = byName_.find(name);
        if (it == byName_.end()) {
          err = "parse error at " + std::to_string(tk.pos) +
                ": unknown column '" + name + "'";
          return nullptr;
        }
        auto n = std::make_unique<ExprNode>();
        n->type = NodeType::Column;
        n->colSlot = it->second.slot;
        n->colName = name;
        n->kind = it->second.kind;  // bound now; typeCheck trusts it
        return n;
      }
      default:
        err = errAt("expected a value");
        return nullptr;
    }
  }

  NodePtr parseCall(const std::string& name, std::size_t pos, std::string& err) {
    auto it = fnTable().find(name);
    if (it == fnTable().end()) {
      err = "parse error at " + std::to_string(pos) + ": unknown function '" +
            name + "'";
      return nullptr;
    }
    advance();  // '('
    auto n = std::make_unique<ExprNode>();
    n->type = NodeType::Call;
    n->fn = it->second.fn;
    if (peek().kind != Tok::RParen) {
      for (;;) {
        NodePtr arg = parseExpr(0, err);
        if (!arg) return nullptr;
        n->children.push_back(std::move(arg));
        if (peek().kind == Tok::Comma) { advance(); continue; }
        break;
      }
    }
    if (peek().kind != Tok::RParen) { err = errAt("expected ')' after args"); return nullptr; }
    advance();
    const FnSpec& spec = it->second;
    int argc = static_cast<int>(n->children.size());
    if (argc < spec.minArgs || (spec.maxArgs != 0 && argc > spec.maxArgs)) {
      err = "parse error at " + std::to_string(pos) + ": function '" + name +
            "' got " + std::to_string(argc) + " args";
      return nullptr;
    }
    return n;
  }

  const std::vector<Token>& toks_;
  std::unordered_map<std::string, ColumnBinding> byName_;
  std::size_t i_{0};
};

// ---------------------------------------------------------------------------
// Type checker. Bottom-up: assigns each node a kind, rejects mismatches. Errors
// are surfaced as fail-fast strings (no row ever runs).
// ---------------------------------------------------------------------------
bool typeCheck(ExprNode& n, std::string& err) {
  for (auto& c : n.children) {
    if (!typeCheck(*c, err)) return false;
  }
  switch (n.type) {
    case NodeType::NumLit: n.kind = ExprKind::Num; return true;
    case NodeType::BoolLit: n.kind = ExprKind::Bool; return true;
    case NodeType::Column: /* kind set at bind */ return true;
    case NodeType::Unary: {
      ExprKind c = n.children[0]->kind;
      if (n.unaryOp == UnaryOp::Neg) {
        if (c != ExprKind::Num) { err = "type error: unary '-' needs num"; return false; }
        n.kind = ExprKind::Num;
      } else {  // Not
        if (c != ExprKind::Bool) { err = "type error: '!' needs bool"; return false; }
        n.kind = ExprKind::Bool;
      }
      return true;
    }
    case NodeType::Binary: {
      ExprKind a = n.children[0]->kind, b = n.children[1]->kind;
      switch (n.binaryOp) {
        case BinaryOp::Add: case BinaryOp::Sub: case BinaryOp::Mul:
        case BinaryOp::Div: case BinaryOp::Mod:
          if (a != ExprKind::Num || b != ExprKind::Num) {
            err = "type error: arithmetic needs num operands"; return false;
          }
          n.kind = ExprKind::Num; return true;
        case BinaryOp::Lt: case BinaryOp::Le: case BinaryOp::Gt: case BinaryOp::Ge:
          if (a != ExprKind::Num || b != ExprKind::Num) {
            err = "type error: comparison needs num operands"; return false;
          }
          n.kind = ExprKind::Bool; return true;
        case BinaryOp::Eq: case BinaryOp::Ne:
          if (a != b) { err = "type error: '==' / '!=' needs same-kind operands"; return false; }
          n.kind = ExprKind::Bool; return true;
        case BinaryOp::And: case BinaryOp::Or:
          if (a != ExprKind::Bool || b != ExprKind::Bool) {
            err = "type error: '&&' / '||' needs bool operands"; return false;
          }
          n.kind = ExprKind::Bool; return true;
      }
      return false;
    }
    case NodeType::Ternary: {
      if (n.children[0]->kind != ExprKind::Bool) {
        err = "type error: ternary condition must be bool"; return false;
      }
      ExprKind a = n.children[1]->kind, b = n.children[2]->kind;
      if (a != b) { err = "type error: ternary arms must have the same kind"; return false; }
      n.kind = a;
      return true;
    }
    case NodeType::Call: {
      // All functions take Num args and return Num, EXCEPT isnan/isfinite which
      // return Bool.
      for (auto& c : n.children) {
        if (c->kind != ExprKind::Num) {
          err = "type error: function arguments must be num"; return false;
        }
      }
      n.kind = (n.fn == Fn::IsNan || n.fn == Fn::IsFinite) ? ExprKind::Bool
                                                           : ExprKind::Num;
      return true;
    }
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// compileExpr — the public entry: lex -> parse -> type-check.
// ---------------------------------------------------------------------------
CompileResult compileExpr(const std::string& source,
                          const std::vector<ColumnBinding>& schema) {
  CompileResult r;
  std::vector<Token> toks;
  std::string err;
  if (!tokenize(source, toks, err)) { r.error = err; return r; }
  Parser parser(toks, schema);
  NodePtr root = parser.parse(err);
  if (!root) { r.error = err; return r; }
  if (!typeCheck(*root, err)) { r.error = err; return r; }
  r.expr.resultKind = root->kind;
  r.expr.root = std::move(root);
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------
// Evaluator — per-row tree walk. Num and Bool dispatch to dedicated walkers so
// the hot path has no boxed-variant overhead (and mirrors the future WGSL split).
// ---------------------------------------------------------------------------
double evalNum(const ExprNode& n, const std::vector<double>& row) {
  switch (n.type) {
    case NodeType::NumLit: return n.num;
    case NodeType::Column: return row[n.colSlot];
    case NodeType::Unary:  // only Neg reaches evalNum
      return -evalNum(*n.children[0], row);
    case NodeType::Binary: {
      double a = evalNum(*n.children[0], row);
      double b = evalNum(*n.children[1], row);
      switch (n.binaryOp) {
        case BinaryOp::Add: return a + b;
        case BinaryOp::Sub: return a - b;
        case BinaryOp::Mul: return a * b;
        case BinaryOp::Div: return a / b;          // IEEE: /0 -> inf/nan
        case BinaryOp::Mod: return std::fmod(a, b);
        default: return 0.0;  // comparisons are Bool, never reach here
      }
    }
    case NodeType::Ternary:
      return evalBool(*n.children[0], row) ? evalNum(*n.children[1], row)
                                           : evalNum(*n.children[2], row);
    case NodeType::Call: {
      auto a0 = [&] { return evalNum(*n.children[0], row); };
      auto a1 = [&] { return evalNum(*n.children[1], row); };
      switch (n.fn) {
        case Fn::Abs: return std::fabs(a0());
        case Fn::Sqrt: return std::sqrt(a0());
        case Fn::Cbrt: return std::cbrt(a0());
        case Fn::Exp: return std::exp(a0());
        case Fn::Log: return std::log(a0());
        case Fn::Log2: return std::log2(a0());
        case Fn::Log10: return std::log10(a0());
        case Fn::Floor: return std::floor(a0());
        case Fn::Ceil: return std::ceil(a0());
        case Fn::Round: return std::round(a0());
        case Fn::Trunc: return std::trunc(a0());
        case Fn::Sign: { double v = a0(); return (v > 0) - (v < 0); }
        case Fn::Sin: return std::sin(a0());
        case Fn::Cos: return std::cos(a0());
        case Fn::Tan: return std::tan(a0());
        case Fn::Atan: return std::atan(a0());
        case Fn::Atan2: return std::atan2(a0(), a1());
        case Fn::Pow: return std::pow(a0(), a1());
        case Fn::Hypot: return std::hypot(a0(), a1());
        case Fn::Mod: return std::fmod(a0(), a1());
        case Fn::Min: {
          double m = a0();
          for (std::size_t i = 1; i < n.children.size(); ++i)
            m = std::fmin(m, evalNum(*n.children[i], row));
          return m;
        }
        case Fn::Max: {
          double m = a0();
          for (std::size_t i = 1; i < n.children.size(); ++i)
            m = std::fmax(m, evalNum(*n.children[i], row));
          return m;
        }
        case Fn::Clamp: {
          double v = a0(), lo = a1(), hi = evalNum(*n.children[2], row);
          return std::fmin(std::fmax(v, lo), hi);
        }
        case Fn::IsNan: case Fn::IsFinite: return 0.0;  // Bool, never reach here
      }
      return 0.0;
    }
    case NodeType::BoolLit: return n.boolean ? 1.0 : 0.0;  // defensive
  }
  return 0.0;
}

bool evalBool(const ExprNode& n, const std::vector<double>& row) {
  switch (n.type) {
    case NodeType::BoolLit: return n.boolean;
    case NodeType::Unary:  // only Not reaches evalBool
      return !evalBool(*n.children[0], row);
    case NodeType::Binary: {
      switch (n.binaryOp) {
        case BinaryOp::Lt: return evalNum(*n.children[0], row) < evalNum(*n.children[1], row);
        case BinaryOp::Le: return evalNum(*n.children[0], row) <= evalNum(*n.children[1], row);
        case BinaryOp::Gt: return evalNum(*n.children[0], row) > evalNum(*n.children[1], row);
        case BinaryOp::Ge: return evalNum(*n.children[0], row) >= evalNum(*n.children[1], row);
        case BinaryOp::Eq:
        case BinaryOp::Ne: {
          bool eq;
          if (n.children[0]->kind == ExprKind::Bool) {
            eq = evalBool(*n.children[0], row) == evalBool(*n.children[1], row);
          } else {
            eq = evalNum(*n.children[0], row) == evalNum(*n.children[1], row);
          }
          return n.binaryOp == BinaryOp::Eq ? eq : !eq;
        }
        case BinaryOp::And:  // short-circuit
          return evalBool(*n.children[0], row) && evalBool(*n.children[1], row);
        case BinaryOp::Or:
          return evalBool(*n.children[0], row) || evalBool(*n.children[1], row);
        default: return false;
      }
    }
    case NodeType::Ternary:
      return evalBool(*n.children[0], row) ? evalBool(*n.children[1], row)
                                           : evalBool(*n.children[2], row);
    case NodeType::Call:
      if (n.fn == Fn::IsNan) return std::isnan(evalNum(*n.children[0], row));
      if (n.fn == Fn::IsFinite) return std::isfinite(evalNum(*n.children[0], row));
      return false;
    case NodeType::Column:  // a bool-kind column (none on the CPU path today)
      return row[n.colSlot] != 0.0;
    default:
      return evalNum(n, row) != 0.0;  // defensive
  }
}

}  // namespace dc
