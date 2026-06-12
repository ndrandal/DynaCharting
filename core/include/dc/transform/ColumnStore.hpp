// ENC-616a — Typed ColumnStore: the home of transform OUTPUT columns.
//
// WHAT THIS IS (RESEARCH §5.2)
// ----------------------------
// "Intermediate columns live in a typed ColumnStore (sibling of CpuBufferStore)."
// A Transform reads INPUT columns (from the TableStore, backed by the ingest feed)
// and writes OUTPUT columns HERE. Unlike TableStore — which only *observes* bytes
// it does not own through a BufferByteSource — the ColumnStore OWNS the bytes of
// every derived column, because a transform produces brand-new values (a formula
// result, a filtered/compacted slice) that exist nowhere upstream.
//
// ADDRESSING — `id.field`
// -----------------------
// Per §5.2 a transform "references upstream outputs by id.field". A stored column
// is keyed by (nodeId, field): nodeId is the producing transform's DependentId and
// field is the output column name it declared. A downstream node names an input as
// `{nodeId, field}` and reads it exactly as it reads a TableStore column — same
// typed-view discipline (f32->float, i32->int32, cat->u32; timestamp stays i64,
// never f32, the same f64 trap TableStore guards).
//
// THE READ SEAM
// -------------
// ColumnStore exposes the same typed ColumnView<T> as TableStore, plus a
// byteSource() that adapts it to the existing BufferByteSource so generic code can
// treat an owned derived column and an observed ingest column uniformly. The DAG's
// ColumnResolver (Transform.hpp) layers TableStore inputs + ColumnStore outputs
// behind ONE lookup so an expression's column refs resolve against either.
//
// Pure `dc` (C++17, no GPU). Owns std::vector<uint8_t> per column; the encode pass
// (a later sub-PR) uploads the terminal geometry via the existing dirty-range path.
#pragma once

#include "dc/data/TableStore.hpp"  // DType, ColumnView, dtypeByteWidth
#include "dc/ids/Id.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// A derived column's address: which transform produced it (`node`) and the output
// field name. Equality is structural so it can key a hash map.
struct ColumnRef {
  Id node{kInvalidId};
  std::string field;

  bool operator==(const ColumnRef& o) const {
    return node == o.node && field == o.field;
  }
};

struct ColumnRefHash {
  std::size_t operator()(const ColumnRef& r) const {
    return std::hash<Id>{}(r.node) ^ (std::hash<std::string>{}(r.field) << 1);
  }
};

// ---------------------------------------------------------------------------
// ColumnStore — owns derived typed columns keyed by ColumnRef. A transform calls
// alloc*() to (re)create an output column for its node+field, fills it (write
// helpers or by appending), and the store hands out typed views + row counts.
// ---------------------------------------------------------------------------
class ColumnStore {
 public:
  // (Re)create the column (node, field) with dtype `dt`, sized for `rows` rows,
  // zero-initialized. Replaces any existing column at that key (a transform fully
  // rewrites its outputs each recompute). Returns false only on an invalid dtype.
  bool allocColumn(Id node, const std::string& field, DType dt, std::size_t rows);

  // Drop a single output column, or every column produced by `node` (on unmount /
  // node removal). Safe if absent.
  void dropColumn(Id node, const std::string& field);
  void dropNode(Id node);

  bool hasColumn(Id node, const std::string& field) const;

  // dtype / row-count of an output column (0 / F32 default if absent).
  DType dtypeOf(Id node, const std::string& field) const;
  std::size_t rowCount(Id node, const std::string& field) const;

  // ----- typed views (read) -------------------------------------------------
  // Element type must match the column dtype, else an empty view (same guard as
  // TableStore — a timestamp column has NO f32 view, the epoch-ms f32 trap).
  ColumnView<float> viewF32(Id node, const std::string& field) const;
  ColumnView<std::int32_t> viewI32(Id node, const std::string& field) const;
  ColumnView<std::uint32_t> viewCat(Id node, const std::string& field) const;
  ColumnView<std::int64_t> viewTimestamp(Id node, const std::string& field) const;

  // ----- typed writes -------------------------------------------------------
  // Overwrite element `i` (no bounds growth; column must be pre-allocated to the
  // right rows). dtype must match or the write is a no-op. These are the per-row
  // sinks `formula`/`filter` write through.
  void setF32(Id node, const std::string& field, std::size_t i, float v);
  void setI32(Id node, const std::string& field, std::size_t i, std::int32_t v);
  void setCat(Id node, const std::string& field, std::size_t i, std::uint32_t v);
  void setTimestamp(Id node, const std::string& field, std::size_t i,
                    std::int64_t v);

  std::size_t columnCount() const { return cols_.size(); }

  // Backing storage for one derived column (public so the .cpp's typed
  // view/write template helpers can name it; not part of the intended API).
  struct Stored {
    DType dtype{DType::F32};
    std::vector<std::uint8_t> bytes;
  };

 private:
  Stored* find(Id node, const std::string& field);
  const Stored* find(Id node, const std::string& field) const;

  std::unordered_map<ColumnRef, Stored, ColumnRefHash> cols_;
};

}  // namespace dc
