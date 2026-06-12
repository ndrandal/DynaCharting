// ENC-593 (P1.2) — Long→wide pivot ingest.
//
// WHAT THIS IS
// ------------
// The RAW feed is long/tidy: a stream of {t, streamKey, field, value} events —
// one (field, value) pair at a time (RESEARCH §3, the ONLY substrate the manifest
// may assume). But a chart ROW needs several fields joined: a candle row is
// {t, open, high, low, close, volume}; a scatter row is {x, y, size}. This stage
// PIVOTS the long stream into wide rows and lands each field in its TableStore
// column, via the UNCHANGED 13-byte ingest feed (op 1 APPEND) — no new wire
// format, exactly as §4.1 prescribes ("a column is a buffer, just typed/named").
//
//        long events (one field each)                 wide rows (one per rowKey)
//   {t=9:30, AAPL, open,  10.0}  ┐
//   {t=9:30, AAPL, high,  11.0}  │  pivot by rowKey=t           ┌ t   ┐ ┌ open ┐ ...
//   {t=9:30, AAPL, low,    9.5}  ├────────────────────────────▶│ 9:30│ │ 10.0 │
//   {t=9:30, AAPL, close, 10.5}  │  (events sharing a rowKey    └─────┘ └──────┘
//   {t=9:30, AAPL, vol,  1000 }  ┘   coalesce into ONE row)        timestamp f32
//
// THE PIVOT KEY
// -------------
// A pending row is keyed by `rowKey` (the value that names a row — typically the
// timestamp t) and, optionally, a `groupKey` (a category — symbol/series — so two
// streams sharing a t stay separate rows). Events with the same (rowKey[,group])
// accumulate into the same pending row REGARDLESS of arrival order; a field that
// never arrives before the row flushes is filled with a typed MISSING sentinel.
//
// FLUSH SEMANTICS (streaming-safe)
// --------------------------------
// A pending row is APPENDED to the table (all its columns, in lockstep) only when
// it is FLUSHED. Three triggers, composable:
//   * flushRow(key) / flushAll()            — explicit (end of tick / teardown).
//   * autoFlushOnNewRowKey (default ON)     — when an event introduces a rowKey
//     strictly greater than the max open one, the older open row(s) are complete
//     (the stream is time-ordered), so they flush automatically. This is what
//     makes a live append-only feed land rows without an explicit per-tick call.
// Flushing emits each column's value into its buffer in column-declaration order
// so the columns stay equal-length (the TableStore lockstep invariant).
//
// MISSING FIELDS
// --------------
// f32  → NaN     (a real "absent" marker distinct from 0; scales skip NaN)
// i32  → 0
// cat  → code 0  (callers wanting an explicit "(none)" should intern it first)
// timestamp → 0
//
// Pure-`dc` control-plane (C++17, no GPU). Builds on TableStore (schema/columns +
// the cat dictionary) and IngestProcessor (the feed); optionally drives a
// RowIdentity (ENC-594) so each flushed row also gets its durable id.
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class IngestProcessor;
class RowIdentity;

// ---------------------------------------------------------------------------
// PivotValue — one field value as it arrives off the long feed, typed to match
// the destination column. The pivot writes the matching member into the column
// buffer at flush. `cat` values arrive as an already-interned u32 code (intern
// through TableStore::catDict before feeding) OR as a label via pushCatEvent().
// ---------------------------------------------------------------------------
struct PivotValue {
  DType dtype{DType::F32};
  // A default-constructed PivotValue is ABSENT — it carries no field value, so a
  // column receiving it at flush gets the typed MISSING sentinel (not a real 0).
  // pvF32/pvI32/... clear this. This is why the default ctor != "a 0.0 f32".
  bool absent{true};
  union {
    float f32;
    std::int32_t i32;
    std::uint32_t cat;
    std::int64_t ts;
  };
  PivotValue() : ts(0) {}
};

inline PivotValue pvF32(float v) { PivotValue p; p.dtype = DType::F32; p.absent = false; p.f32 = v; return p; }
inline PivotValue pvI32(std::int32_t v) { PivotValue p; p.dtype = DType::I32; p.absent = false; p.i32 = v; return p; }
inline PivotValue pvCat(std::uint32_t v) { PivotValue p; p.dtype = DType::Cat; p.absent = false; p.cat = v; return p; }
inline PivotValue pvTimestamp(std::int64_t v) { PivotValue p; p.dtype = DType::Timestamp; p.absent = false; p.ts = v; return p; }

// ---------------------------------------------------------------------------
// PivotIngest — pivots a long {rowKey, field, value} stream into the wide rows of
// ONE table. Configure the field→column map once, then push events; flushed rows
// append to the table through the ingest feed.
// ---------------------------------------------------------------------------
class PivotIngest {
 public:
  PivotIngest(TableStore& tables, IngestProcessor& ingest);

  // ----- configuration ------------------------------------------------------

  // Pivot into `tableId`. Must be called before configuring fields. The table's
  // columns must already be defined (TableStore::addColumn). Returns false if the
  // table is unknown.
  bool setTable(Id tableId);

  // Designate the column that receives the rowKey value of each wide row (e.g.
  // the timestamp column `t`). Every flushed row writes its rowKey here. The
  // column must exist; its dtype determines how the rowKey is stored. Returns
  // false on unknown table/column.
  bool setRowKeyColumn(const std::string& columnName);

  // OPTIONAL: designate the column that receives the groupKey (e.g. a `symbol`
  // cat column). When set, the pivot key is (rowKey, groupKey) so concurrent
  // series don't collide. Returns false on unknown table/column.
  bool setGroupKeyColumn(const std::string& columnName);

  // Map a long-feed `field` name onto a destination column. Multiple fields map
  // to distinct columns (open→"open", high→"high", …). The column must exist.
  // Returns false on unknown table/column.
  bool mapField(const std::string& field, const std::string& columnName);

  // OPTIONAL: drive a RowIdentity (ENC-594) so each flushed row also gets its
  // durable id appended in lockstep. The RowIdentity must already be bound to the
  // same table. Pass nullptr (default) to skip identity.
  void setRowIdentity(RowIdentity* rowId) { rowId_ = rowId; }

  // When true (default), an event whose rowKey strictly exceeds the largest open
  // pending rowKey flushes the older open row(s) first (time-ordered streams).
  void setAutoFlushOnNewRowKey(bool enable) { autoFlush_ = enable; }

  // ----- pushing long events ------------------------------------------------

  // Push one long event: field `field` (must be mapped) has value `value` for the
  // wide row identified by (rowKey[, groupKey]). Accumulates into the pending row;
  // does NOT append until the row flushes. Returns false if the field is unmapped
  // or the value dtype doesn't match the destination column.
  bool pushEvent(std::int64_t rowKey, const std::string& field,
                 const PivotValue& value,
                 std::optional<std::uint32_t> groupKey = std::nullopt);

  // Sugar: push a `cat` field by LABEL — interns it through the column's
  // dictionary (TableStore::catDict) to a code, then pushEvent. Returns false if
  // the field is unmapped or its column is not a cat column.
  bool pushCatEvent(std::int64_t rowKey, const std::string& field,
                    const std::string& label,
                    std::optional<std::uint32_t> groupKey = std::nullopt);

  // ----- flushing -----------------------------------------------------------

  // Flush every open pending row (e.g. end of a tick / teardown). Returns the
  // number of rows appended.
  std::size_t flushAll();

  // Flush the single pending row with this (rowKey[, groupKey]). Returns true if
  // a matching open row existed and was appended.
  bool flushRow(std::int64_t rowKey,
                std::optional<std::uint32_t> groupKey = std::nullopt);

  // Number of rows appended to the table over this pivot's lifetime.
  std::size_t rowsAppended() const { return rowsAppended_; }

  // Number of pending (un-flushed) rows currently accumulating.
  std::size_t pendingRowCount() const { return pending_.size(); }

 private:
  // The composite pivot key. groupKey is folded in only when a group column is
  // configured; otherwise it is a fixed sentinel so all events with a rowKey share
  // one row.
  struct PivotKey {
    std::int64_t rowKey{0};
    std::uint64_t groupKey{0};
    bool operator<(const PivotKey& o) const {
      if (rowKey != o.rowKey) return rowKey < o.rowKey;
      return groupKey < o.groupKey;
    }
  };

  struct PendingRow {
    std::int64_t rowKey{0};
    std::optional<std::uint32_t> groupKey;
    // field-column-name -> value (only fields seen so far; rest fill at flush).
    std::unordered_map<std::string, PivotValue> values;
  };

  PivotKey makeKey(std::int64_t rowKey,
                   std::optional<std::uint32_t> groupKey) const;

  // Append one fully-resolved row to the table (every mapped column + rowKey +
  // groupKey + row id), filling missing fields with the typed sentinel.
  void appendRow(const PendingRow& row);

  // Append a single typed value to `columnName`'s buffer via the ingest feed.
  void emitValue(const std::string& columnName, const PivotValue& v);

  const Column* columnOf(const std::string& columnName) const;

  TableStore& tables_;
  IngestProcessor& ingest_;
  RowIdentity* rowId_{nullptr};

  Id tableId_{kInvalidId};
  std::optional<std::string> rowKeyColumn_;
  std::optional<std::string> groupKeyColumn_;
  // long-feed field name -> destination column name.
  std::unordered_map<std::string, std::string> fieldToColumn_;

  bool autoFlush_{true};
  // Open pending rows, ordered by key so flushAll / auto-flush are deterministic.
  std::map<PivotKey, PendingRow> pending_;

  std::size_t rowsAppended_{0};
};

}  // namespace dc
