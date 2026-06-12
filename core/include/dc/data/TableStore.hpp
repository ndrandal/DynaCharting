// ENC-592 (P1.1) — TableStore + Column model: the bottom primitive of the
// data→visual layer (the "grammar of graphics" project, RESEARCH §4.1).
//
// WHAT THIS IS
// ------------
// A TABLE is a named control-plane resource holding equal-length, named, typed
// COLUMNS. A column **is** a buffer — just typed and named — so the existing
// 13-byte binary ingest feed (op 1 APPEND / op 2 UPDATE_RANGE) fills it with NO
// new wire format. Per RESEARCH §4.1:
//
//   column = { name, dtype, bufferId }   over the existing CpuBuffer ids
//   dtypes  = f32 | i32 | cat (dictionary: u32 codes + a string palette)
//                                | timestamp (i64 epoch-ms)
//
// The named/typed column is *the* missing abstraction every later ticket needs:
// scales need a domain + dtype, encodings reference columns by name, auto-domain
// needs a dtype. This ticket builds ONLY the table/column primitive — no scales,
// encodings, marks, pivot ingest, dirty/reactive mechanism (ENC-593…596).
//
// THE TIMESTAMP TRAP (non-negotiable, RESEARCH §4.1 + SPEC constraints)
// --------------------------------------------------------------------
// `timestamp` is i64 epoch-ms and MUST stay CPU-side. It is NEVER reinterpreted
// as f32: epoch-ms (~1.7e12 today) overflows the f32 mantissa (~16.7M), so a
// naive f32 cast silently corrupts time. The GPU path (a LATER ticket) consumes
// time pre-normalized to a *relative* f32 offset. Here we only expose timestamps
// as i64; asking for an f32 view of a timestamp column is rejected.
//
// DECOUPLING — BufferByteSource
// -----------------------------
// A column's bytes live in whatever buffer store the engine ingests into
// (IngestProcessor on the WASM host; CpuBufferStore on the Dawn path). TableStore
// does NOT own those bytes and does NOT depend on a concrete store: it reads
// through a tiny BufferByteSource seam (size + bytes by Id). This keeps the
// primitive pure `dc` (C++17, no GPU) and lets a column be backed by either
// store. `makeIngestByteSource()` adapts the existing IngestProcessor.
//
// FORWARD-COMPAT HOOKS (declared, not implemented — ENC-594/595)
// --------------------------------------------------------------
// The model is shaped so a stable per-row identity (ENC-594) and a generic
// dirty/reactive mechanism (ENC-595) can be ADDED without redesign:
//   * Table::version() — a monotonic counter bumped on structural change; a
//     later reactive layer can extend this to a per-input dirty epoch.
//   * Table carries an optional `rowIdColumn` slot: a column may be DESIGNATED
//     the row-identity column now (so the schema has a place for it), but this
//     ticket implements no identity semantics over it.
#pragma once

#include "dc/ids/Id.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// DType — the column element type. Drives byte width + how a column's bytes are
// reinterpreted. See RESEARCH §4.1.
// ---------------------------------------------------------------------------
enum class DType : std::uint8_t {
  F32,        // 32-bit float           (4 bytes) — native GPU
  I32,        // 32-bit signed int      (4 bytes) — native GPU
  Cat,        // category code: u32 dictionary index (4 bytes) + string palette
  Timestamp,  // i64 epoch-ms           (8 bytes) — CPU-ONLY, never f32 (overflow)
};

// Byte width of one element of `dt`.
inline constexpr std::size_t dtypeByteWidth(DType dt) {
  switch (dt) {
    case DType::F32:
    case DType::I32:
    case DType::Cat:
      return 4;
    case DType::Timestamp:
      return 8;
  }
  return 0;
}

inline const char* toString(DType dt) {
  switch (dt) {
    case DType::F32: return "f32";
    case DType::I32: return "i32";
    case DType::Cat: return "cat";
    case DType::Timestamp: return "timestamp";
  }
  return "unknown";
}

// Parse a dtype string ("f32"/"i32"/"cat"/"timestamp"). Returns nullopt if the
// string is not a known dtype (the manifest loader, ENC-605, fails fast on this).
std::optional<DType> parseDType(const std::string& s);

// ---------------------------------------------------------------------------
// BufferByteSource — the seam between a column and the bytes that back it. The
// underlying buffer is filled by the existing ingest feed; TableStore only reads
// it. Implemented for IngestProcessor via makeIngestByteSource().
// ---------------------------------------------------------------------------
struct BufferByteSource {
  // Bytes for `bufferId`, or nullptr if the buffer does not exist / is empty.
  std::function<const std::uint8_t*(Id)> data;
  // Size in bytes of `bufferId` (0 if absent).
  std::function<std::uint32_t(Id)> size;
};

// Adapt an IngestProcessor (or anything with getBufferData/getBufferSize) into a
// BufferByteSource. Templated so it works with IngestProcessor AND CpuBufferStore
// without TableStore depending on either header.
template <typename Store>
BufferByteSource makeBufferByteSource(const Store& store) {
  return BufferByteSource{
      [&store](Id id) { return store.getBufferData(id); },
      [&store](Id id) { return store.getBufferSize(id); },
  };
}

// ---------------------------------------------------------------------------
// CatDictionary — the string palette half of a `cat` column. The column buffer
// holds u32 codes; this maps code → label and interns label → code. Control-plane
// only (strings never go to the GPU). Codes are assigned densely from 0.
// ---------------------------------------------------------------------------
class CatDictionary {
 public:
  // Intern `label`, returning its (stable) u32 code. Repeated labels return the
  // same code; new labels get the next dense code.
  std::uint32_t intern(const std::string& label);

  // Label for `code`, or empty string if the code is out of range.
  const std::string& labelOf(std::uint32_t code) const;

  // Code for `label` if interned, else nullopt.
  std::optional<std::uint32_t> codeOf(const std::string& label) const;

  std::uint32_t size() const {
    return static_cast<std::uint32_t>(labels_.size());
  }

 private:
  std::vector<std::string> labels_;                       // code → label
  std::unordered_map<std::string, std::uint32_t> index_;  // label → code
};

// ---------------------------------------------------------------------------
// Column — a named, typed view over a buffer id. Owns no bytes; the bytes live in
// the BufferByteSource. A `cat` column additionally owns its dictionary.
// ---------------------------------------------------------------------------
struct Column {
  std::string name;
  DType dtype{DType::F32};
  Id bufferId{kInvalidId};

  // Only meaningful when dtype == Cat. The string palette for the u32 codes.
  CatDictionary dict;
};

// A typed, length-checked view over a column's bytes. Returned by TableStore's
// reinterpret helpers. T must match the column dtype's storage type (f32→float,
// i32→int32_t, cat→uint32_t, timestamp→int64_t); a mismatch yields an empty view.
template <typename T>
struct ColumnView {
  const T* data{nullptr};
  std::size_t count{0};  // number of T elements

  bool valid() const { return data != nullptr; }
  std::size_t size() const { return count; }
  const T& operator[](std::size_t i) const { return data[i]; }
  const T* begin() const { return data; }
  const T* end() const { return data + count; }
};

// ---------------------------------------------------------------------------
// TableStore — defines tables, binds named typed columns to buffer ids, and
// answers length/typing queries against a BufferByteSource. The store holds the
// schema (the control plane); the data plane (bytes) lives in the buffer store
// and grows via the unchanged ingest feed — TableStore observes that growth.
// ---------------------------------------------------------------------------
class TableStore {
 public:
  // ----- table definition (the create/declare API, ENC-592 scope) -----------

  // Define an (empty-schema) table named `name` with logical id `tableId`.
  // Fails (returns false) if a table with that id already exists.
  bool defineTable(Id tableId, std::string name);

  // Add a column to a table. Fails (false) if the table is unknown, a column of
  // that name already exists in the table, or the dtype is invalid. The bound
  // buffer need not exist yet — it is filled later by the ingest feed.
  bool addColumn(Id tableId, const std::string& name, DType dtype, Id bufferId);

  // Designate `columnName` as the table's row-identity column (ENC-594 hook).
  // This ONLY records the designation in the schema; no identity semantics are
  // implemented here. Fails if the table or column is unknown.
  bool setRowIdColumn(Id tableId, const std::string& columnName);

  // Remove a table and its schema. Returns false if unknown.
  bool removeTable(Id tableId);

  bool hasTable(Id tableId) const;

  // ----- queries ------------------------------------------------------------

  // The column named `name` in `tableId`, or nullptr.
  const Column* column(Id tableId, const std::string& name) const;

  // The column named `name` in `tableId`, but only if its dtype == `expect`.
  // Returns nullptr on table/column miss OR dtype mismatch. This is the
  // "query columns by name + dtype" entry the encoding stage (ENC-600) uses.
  const Column* column(Id tableId, const std::string& name, DType expect) const;

  // Names of all columns in `tableId`, in insertion order.
  std::vector<std::string> columnNames(Id tableId) const;

  // The designated row-id column name, if any (ENC-594 hook).
  std::optional<std::string> rowIdColumn(Id tableId) const;

  // Row count of a single column = its buffer byte length / dtype width.
  // Returns 0 for an unknown table/column.
  std::size_t columnRowCount(Id tableId, const std::string& name,
                             const BufferByteSource& src) const;

  // Row count of the TABLE. A table's columns must stay equal-length (they append
  // in lockstep via the ingest feed). This returns the COMMON row count when the
  // columns agree, and the MINIMUM otherwise (a partially-applied batch can leave
  // columns transiently ragged between ticks). Use rowCountConsistent() to assert
  // the strict invariant. An empty-schema table has row count 0.
  std::size_t rowCount(Id tableId, const BufferByteSource& src) const;

  // True iff every column in `tableId` reports the same row count (the lockstep
  // invariant). Trivially true for a 0- or 1-column table.
  bool rowCountConsistent(Id tableId, const BufferByteSource& src) const;

  // ----- typed reinterpret --------------------------------------------------
  //
  // Reinterpret a column's bytes as a typed array. The element type T must match
  // the column's dtype storage type, else an empty (invalid) view is returned:
  //   f32→float, i32→int32_t, cat→uint32_t, timestamp→int64_t.
  // IMPORTANT: a timestamp column has NO float view by design (f32 epoch-ms
  // overflow). viewF32() on a timestamp column returns an empty view.
  ColumnView<float> viewF32(Id tableId, const std::string& name,
                            const BufferByteSource& src) const;
  ColumnView<std::int32_t> viewI32(Id tableId, const std::string& name,
                                   const BufferByteSource& src) const;
  ColumnView<std::uint32_t> viewCat(Id tableId, const std::string& name,
                                    const BufferByteSource& src) const;
  ColumnView<std::int64_t> viewTimestamp(Id tableId, const std::string& name,
                                         const BufferByteSource& src) const;

  // Mutable dictionary for a `cat` column (e.g. the pivot ingest interns labels
  // as codes are appended). nullptr if the table/column is unknown or not Cat.
  CatDictionary* catDict(Id tableId, const std::string& name);
  const CatDictionary* catDict(Id tableId, const std::string& name) const;

  // ENC-595 hook: a monotonic version bumped on every STRUCTURAL change
  // (defineTable / addColumn / setRowIdColumn / removeTable). A later reactive
  // layer can build a per-input dirty epoch on top. 0 for an unknown table.
  std::uint64_t version(Id tableId) const;

  std::size_t tableCount() const { return tables_.size(); }

 private:
  struct Table {
    Id id{kInvalidId};
    std::string name;
    std::vector<Column> columns;                       // insertion order
    std::unordered_map<std::string, std::size_t> byName;  // name → index
    std::optional<std::string> rowIdColumn;            // ENC-594 hook
    std::uint64_t version{0};                          // ENC-595 hook
  };

  Table* find(Id tableId);
  const Table* find(Id tableId) const;
  const Column* findColumn(const Table& t, const std::string& name) const;

  // Raw byte span + element count for a column, validated against `expectWidth`.
  // Returns {nullptr,0} if the column is missing or the buffer size is not a
  // whole multiple of the element width.
  std::pair<const std::uint8_t*, std::size_t> rawColumn(
      Id tableId, const std::string& name, std::size_t expectWidth,
      const BufferByteSource& src) const;

  std::unordered_map<Id, Table> tables_;
};

}  // namespace dc
