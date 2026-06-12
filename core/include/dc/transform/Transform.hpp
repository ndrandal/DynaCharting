// ENC-616a — Transform node interface + the input/output schema model.
//
// WHAT A TRANSFORM IS (RESEARCH §5.2)
// -----------------------------------
// "A Transform is a pure node inputs: ColumnRef[] -> outputs: Column[], referencing
// upstream outputs by id.field, forming a DAG. Each transform declares an output
// schema as a function of input schema, so the compiler validates dtype
// compatibility at manifest-load (fail fast)."
//
// Two responsibilities, split so typing is fail-fast and runs WITHOUT data:
//   1. inferSchema(inputSchema) -> outputSchema | error   — pure, data-free. The
//      §6.1 check-2 gate: an output column set + dtypes derived solely from the
//      input column set + dtypes. Rejected here (a formula referencing a missing
//      column, a filter predicate that is not boolean) => the node never runs.
//   2. evaluate(ctx) — reads input columns through the ColumnResolver, writes
//      output columns into the ColumnStore. The only place rows are touched.
//
// COLUMN SCHEMA / RESOLVER
// ------------------------
// A ColumnSchema is the typed column set flowing along one edge: ordered
// {name, dtype}. The ColumnResolver gives a transform uniform READ access to its
// inputs whether they live in the TableStore (ingest columns) or the ColumnStore
// (an upstream node's outputs) — same typed discipline, same f64-timestamp guard.
// On the CPU path every numeric dtype (f32/i32/cat) is read as a double for the
// expression evaluator; timestamp is offered as i64 (callers that need it ask
// explicitly — no f32 epoch-ms).
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"
#include "dc/transform/ColumnStore.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dc {

// One typed column in a schema (the unit a transform infers over).
struct SchemaColumn {
  std::string name;
  DType dtype{DType::F32};
};

// The ordered typed column set on one DAG edge.
struct ColumnSchema {
  std::vector<SchemaColumn> columns;

  const SchemaColumn* find(const std::string& name) const {
    for (const auto& c : columns)
      if (c.name == name) return &c;
    return nullptr;
  }
  bool has(const std::string& name) const { return find(name) != nullptr; }
};

// Result of inferSchema(): the output schema, or a fail-fast error string.
struct SchemaResult {
  ColumnSchema schema;
  bool ok{false};
  std::string error;
};

// ---------------------------------------------------------------------------
// ColumnResolver — a transform's read view over ONE input source. It abstracts
// "where the bytes are": a TableStore input column or a ColumnStore upstream
// output. Built by the DAG for each node from that node's single declared input.
//
// A node in this PR has exactly ONE input source (filter/formula are unary). The
// resolver exposes that source's row count + per-column double reads (the form
// the expression evaluator consumes) and typed passthrough for non-numeric copy.
// ---------------------------------------------------------------------------
class ColumnResolver {
 public:
  // The number of rows in the input source.
  std::function<std::size_t()> rowCount;

  // Value of column `name` at row `i`, as a double (f32/i32/cat widened; a
  // timestamp is widened too but callers should avoid it for arithmetic — kept
  // for completeness, never used to feed the GPU). NaN if the column is absent.
  std::function<double(const std::string& name, std::size_t i)> readNum;

  // dtype of input column `name` (F32 default if absent — schema already
  // validated the name, so this is a fast accessor).
  std::function<DType(const std::string& name)> dtypeOf;

  // Raw i64 timestamp read (for copy-through of a timestamp column without the
  // f32 trap). 0 if the column is absent / not a timestamp.
  std::function<std::int64_t(const std::string& name, std::size_t i)> readTimestamp;
};

// ---------------------------------------------------------------------------
// EvalContext — everything a transform's evaluate() needs: its own node id (the
// key it writes outputs under), the resolver over its input, and the ColumnStore
// it writes into. The input schema is supplied so evaluate() can iterate columns.
// ---------------------------------------------------------------------------
struct EvalContext {
  Id nodeId{kInvalidId};
  const ColumnSchema* inputSchema{nullptr};
  const ColumnResolver* input{nullptr};
  ColumnStore* out{nullptr};
};

// ---------------------------------------------------------------------------
// TransformNode — the pure node interface. Concrete transforms (Filter, Formula,
// …) implement inferSchema (data-free typing) + evaluate (the row work). Named
// *Node* to avoid colliding with the affine 2D `dc::Transform` in scene/Types.hpp.
// ---------------------------------------------------------------------------
class TransformNode {
 public:
  virtual ~TransformNode() = default;

  // The transform op name ("filter" / "formula") — diagnostics only.
  virtual const char* op() const = 0;

  // Data-free typing: given the input edge schema, produce the output edge
  // schema or a fail-fast error. Called once at DAG-build time. MUST NOT touch
  // any bytes — it only reasons over names + dtypes.
  virtual SchemaResult inferSchema(const ColumnSchema& input) const = 0;

  // Run the node: read inputs via ctx.input, write outputs into ctx.out under
  // ctx.nodeId. Called only when the DAG decides the node is dirty.
  virtual void evaluate(const EvalContext& ctx) const = 0;
};

}  // namespace dc
