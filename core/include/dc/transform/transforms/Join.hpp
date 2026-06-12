// ENC-616d — `join`/`lookup` transform (RESEARCH §5.1 relational tier, §7.3
// red-team note). THE prerequisite for ALL edge-bearing charts.
//
// THE PROBLEM IT SOLVES (RESEARCH §7.3)
// -------------------------------------
// An edge-bearing chart (node-link graph, sankey, chord) stores its edges in one
// table keyed by endpoint *id* — an edge row is {src, dst} — and its node
// positions/attributes in ANOTHER table keyed by that same id. Nothing positions
// the edge until each endpoint id is RESOLVED against the node table to pull the
// node's x/y onto the edge row. Without this primitive "no edge can be positioned
// from generic positions+edges" — it is the one concrete missing relational op.
//
// WHAT THIS NODE DOES
// -------------------
// A binary (arity-2) transform: LEFT input = the rows being resolved (e.g. the
// edges table), RIGHT input = the lookup table (e.g. the nodes table). It HASHES
// the right side by its key column once, then for each left row looks the key up
// and APPENDS the selected right columns onto the left row. The output schema is
// the LEFT columns (passed through) PLUS the appended right columns, renamed by a
// per-lookup prefix so multiple lookups don't collide.
//
// MULTI-KEY (the edge-bearing use case, RESEARCH §6.2 manifest lines 356–359)
// --------------------------------------------------------------------------
// One node can declare SEVERAL lookups against the same right table — e.g. resolve
// BOTH endpoints of an edge in one pass:
//   Lookup{ leftKey:"src", prefix:"src", fields:{"x","y"} }
//   Lookup{ leftKey:"dst", prefix:"dst", fields:{"x","y"} }
// against rightKey "id" → emits src.x / src.y / dst.x / dst.y on every edge row.
// That is exactly what binds a line/ribbon mark's (x,y)→(x2,y2) endpoints.
//
// MISS POLICY
// -----------
// A left key with no match in the right table is handled per `MissPolicy`:
//   * Null   — emit a sentinel (NaN for f32, 0 for i32/cat/timestamp) and KEEP the
//              row (the default; lets the chart still draw the surviving endpoint /
//              a downstream filter prune it).
//   * Drop   — drop the whole left row (only rows where EVERY lookup hit survive).
//
// FAIL-FAST TYPING (inferSchemaBinary, data-free)
// -----------------------------------------------
// Rejected at DAG-build before any row runs:
//   * a left key / right key / pulled field that does not exist in its schema;
//   * a key DTYPE MISMATCH (left key dtype != right key dtype) — you cannot resolve
//     a cat key against an f32 key;
//   * an appended (prefixed) output name that collides with a left column or with
//     another lookup's output.
// The pulled right column keeps its dtype (timestamp stays i64 — no f32 trap).
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/transform/Transform.hpp"

#include <string>
#include <vector>

namespace dc {

// How an unmatched left key is handled.
enum class JoinMiss : std::uint8_t {
  Null,  // emit sentinel, keep the row (default)
  Drop,  // drop the left row entirely
};

// One lookup against the (shared) right table: resolve `leftKey` against the right
// table's key, pulling `fields` onto the left row, each renamed `prefix + "." + f`.
struct JoinLookup {
  std::string leftKey;            // left column holding the key (e.g. "src")
  std::string prefix;             // output prefix for pulled fields (e.g. "src")
  std::vector<std::string> fields;  // right columns to pull (e.g. {"x","y"})
};

class JoinTransform : public TransformNode {
 public:
  // `rightKey` is the right table's key column every lookup resolves against
  // (e.g. the nodes table's "id"). `lookups` is one-or-more resolutions sharing it.
  JoinTransform(std::string rightKey, std::vector<JoinLookup> lookups,
                JoinMiss miss = JoinMiss::Null)
      : rightKey_(std::move(rightKey)),
        lookups_(std::move(lookups)),
        miss_(miss) {}

  const char* op() const override { return "join"; }
  int arity() const override { return 2; }

  // Unary inferSchema is never reached for a binary node; provide a clear error.
  SchemaResult inferSchema(const ColumnSchema& /*left*/) const override;
  SchemaResult inferSchemaBinary(const ColumnSchema& left,
                                 const ColumnSchema& right) const override;
  void evaluate(const EvalContext& ctx) const override;

  // The fully-qualified output name for a pulled field ("prefix.field").
  static std::string qualified(const std::string& prefix,
                               const std::string& field) {
    return prefix + "." + field;
  }

 private:
  std::string rightKey_;
  std::vector<JoinLookup> lookups_;
  JoinMiss miss_;
};

}  // namespace dc
