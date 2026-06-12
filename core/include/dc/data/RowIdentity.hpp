// ENC-594 (P1.3) — Stable per-row identity.
//
// WHAT THIS IS
// ------------
// Every table row gets a DURABLE id — a number that names that exact row for the
// lifetime of the data, surviving appends and retention/eviction. The id lives in
// the table itself, in the `rowIdColumn` slot ENC-592 (TableStore) stubbed: a
// designated column that holds one id per row, filled in lockstep with the data
// columns through the UNCHANGED 13-byte ingest feed (op 1 APPEND).
//
// WHY (forward-compat, RESEARCH §4.4 + the §5/§6 interaction hooks)
// ----------------------------------------------------------------
// The (LATER) encode pass emits one mark INSTANCE per table row; per-instance
// PICKING then needs to map a rendered instance back to its source row WITHOUT a
// retrofit. If every row already carries a stable id at the TABLE level, the
// encode pass can carry that id into the instance for free. This ticket builds
// ONLY the table-level identity — NOT picking, NOT the encode pass.
//
// THE STABILITY CONTRACT
// ----------------------
//   * Ids are assigned monotonically from a per-table counter: row N gets id N
//     (densely, starting at 0). The counter only ever increases.
//   * An id, once written into a row, is NEVER rewritten — appends only add new
//     rows with new ids; existing rows keep theirs.
//   * Retention/eviction drops the OLDEST rows (front bytes of every column,
//     including the id column, evicted in lockstep). Surviving rows keep their
//     exact ids — the id is data in the column, so evicting the front shifts
//     positions but not values. The monotonic counter guarantees a future append
//     never reuses an evicted id.
//
// DTYPE
// -----
// The id column is `i32` (native GPU-friendly, 4 bytes) so the future encode pass
// can carry it straight into a per-instance attribute. i32 holds ~2.1e9 distinct
// ids — ample for a streaming session; the counter is tracked as u32 and the
// allocator refuses to assign past INT32_MAX so the value is always a clean,
// non-negative i32 (no GPU sign-bit surprise).
//
// This is a pure-`dc` control-plane helper (C++17, no GPU). It writes ids into
// the id column's buffer via the same IngestProcessor the data columns use, so
// there is no second code path and no new wire format.
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

class IngestProcessor;

// ---------------------------------------------------------------------------
// RowIdentity — assigns and tracks durable per-row ids for a table whose
// `rowIdColumn` (an i32 column) it owns. One RowIdentity instance manages the id
// counter for one table id; bind it once, then call appendIds() each time you
// append rows to the data columns so the id column grows in lockstep.
// ---------------------------------------------------------------------------
class RowIdentity {
 public:
  // The id assigned to a row whose ids have NOT been allocated. Returned by
  // idAt()/indexOfId() on a miss. Distinct from any real (>=0) id.
  static constexpr std::int32_t kNoRowId = -1;

  // Designate `columnName` (which MUST already be an i32 column of `tableId`) as
  // the table's row-identity column and begin managing it. Records the
  // designation via TableStore::setRowIdColumn (the ENC-592 hook). Fails (false)
  // if the table/column is unknown or the column is not i32.
  bool bind(TableStore& tables, Id tableId, const std::string& columnName);

  bool bound() const { return bound_; }
  Id tableId() const { return tableId_; }
  const std::string& columnName() const { return columnName_; }
  Id bufferId() const { return bufferId_; }

  // Assign ids for `rowCount` freshly-appended rows and APPEND them (as i32) into
  // the id column's buffer via `ingest` (op 1 APPEND, the unchanged feed). Call
  // this AFTER appending `rowCount` rows to the data columns, so the id column
  // stays the same length as the data columns. Returns the ids assigned (the new
  // rows' ids), in order. No-op returning {} if not bound or rowCount == 0, or if
  // assigning would overflow the i32 id space.
  std::vector<std::int32_t> appendIds(IngestProcessor& ingest,
                                      std::size_t rowCount);

  // The number of ids assigned so far over this table's lifetime (== the next id
  // to be assigned). Monotonic; unaffected by eviction.
  std::uint32_t nextId() const { return nextId_; }

  // ----- queries (read the id column through the live buffer) ----------------

  // The durable id stored at the current row index `rowIndex` (0-based into the
  // LIVE, post-eviction id column), or kNoRowId if out of range / unbound.
  std::int32_t idAt(const BufferByteSource& src, std::size_t rowIndex) const;

  // The current row index of the row whose durable id is `id`, or nullopt if no
  // live row has that id (it was never assigned, or it was evicted). O(rows) —
  // the id column is monotonically increasing along its length, so this is a
  // simple scan/search; callers needing it hot should cache.
  std::optional<std::size_t> indexOfId(const BufferByteSource& src,
                                       std::int32_t id) const;

  // Number of live rows currently in the id column (its length / 4).
  std::size_t liveRowCount(const BufferByteSource& src) const;

 private:
  bool bound_{false};
  Id tableId_{kInvalidId};
  std::string columnName_;
  Id bufferId_{kInvalidId};
  std::uint32_t nextId_{0};  // monotonic counter (next id to hand out)
};

}  // namespace dc
