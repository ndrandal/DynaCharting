// ENC-593 (P1.2) — Long→wide pivot ingest. See PivotIngest.hpp for the design.
#include "dc/data/PivotIngest.hpp"

#include "dc/data/RowIdentity.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace dc {

// A group sentinel for tables WITHOUT a group column: all events for a rowKey
// share this single group so they coalesce into one row.
static constexpr std::uint64_t kNoGroup =
    std::numeric_limits<std::uint64_t>::max();

// Build one 13-byte ingest record (op 1 APPEND) — the UNCHANGED wire format.
static void appendRecord(std::vector<std::uint8_t>& out, Id bufferId,
                         const void* bytes, std::uint32_t len) {
  auto u32 = [&out](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  out.push_back(1);  // op = APPEND
  u32(static_cast<std::uint32_t>(bufferId));
  u32(0);    // offset (ignored for append)
  u32(len);  // payloadBytes
  const auto* p = static_cast<const std::uint8_t*>(bytes);
  out.insert(out.end(), p, p + len);
}

PivotIngest::PivotIngest(TableStore& tables, IngestProcessor& ingest)
    : tables_(tables), ingest_(ingest) {}

bool PivotIngest::setTable(Id tableId) {
  if (!tables_.hasTable(tableId)) return false;
  tableId_ = tableId;
  return true;
}

const Column* PivotIngest::columnOf(const std::string& columnName) const {
  if (tableId_ == kInvalidId) return nullptr;
  return tables_.column(tableId_, columnName);
}

bool PivotIngest::setRowKeyColumn(const std::string& columnName) {
  if (!columnOf(columnName)) return false;
  rowKeyColumn_ = columnName;
  return true;
}

bool PivotIngest::setGroupKeyColumn(const std::string& columnName) {
  if (!columnOf(columnName)) return false;
  groupKeyColumn_ = columnName;
  return true;
}

bool PivotIngest::mapField(const std::string& field,
                           const std::string& columnName) {
  if (!columnOf(columnName)) return false;
  fieldToColumn_[field] = columnName;
  return true;
}

PivotIngest::PivotKey PivotIngest::makeKey(
    std::int64_t rowKey, std::optional<std::uint32_t> groupKey) const {
  PivotKey k;
  k.rowKey = rowKey;
  // Only fold the group in when a group column is configured; otherwise collapse
  // all events for the rowKey into one row.
  k.groupKey = groupKeyColumn_ ? (groupKey ? *groupKey : 0) : kNoGroup;
  return k;
}

bool PivotIngest::pushEvent(std::int64_t rowKey, const std::string& field,
                            const PivotValue& value,
                            std::optional<std::uint32_t> groupKey) {
  auto fit = fieldToColumn_.find(field);
  if (fit == fieldToColumn_.end()) return false;  // unmapped field
  const Column* col = columnOf(fit->second);
  if (!col) return false;
  if (col->dtype != value.dtype) return false;  // dtype mismatch — reject

  // Auto-flush older open rows once a strictly-newer rowKey appears (the stream
  // is time-ordered, so they are complete). Done BEFORE inserting this event so
  // the new row stays open.
  if (autoFlush_ && !pending_.empty()) {
    std::int64_t maxOpen = pending_.rbegin()->first.rowKey;
    if (rowKey > maxOpen) {
      // Flush every open row with a strictly-smaller rowKey.
      while (!pending_.empty() && pending_.begin()->first.rowKey < rowKey) {
        appendRow(pending_.begin()->second);
        pending_.erase(pending_.begin());
      }
    }
  }

  PivotKey key = makeKey(rowKey, groupKey);
  PendingRow& row = pending_[key];
  row.rowKey = rowKey;
  row.groupKey = groupKey;
  row.values[fit->second] = value;
  return true;
}

bool PivotIngest::pushCatEvent(std::int64_t rowKey, const std::string& field,
                               const std::string& label,
                               std::optional<std::uint32_t> groupKey) {
  auto fit = fieldToColumn_.find(field);
  if (fit == fieldToColumn_.end()) return false;
  CatDictionary* dict = tables_.catDict(tableId_, fit->second);
  if (!dict) return false;  // column missing or not cat
  std::uint32_t code = dict->intern(label);
  return pushEvent(rowKey, field, pvCat(code), groupKey);
}

void PivotIngest::emitValue(const std::string& columnName,
                            const PivotValue& v) {
  const Column* col = columnOf(columnName);
  if (!col) return;
  // A value contributes its real bits only when present AND its dtype matches the
  // column; otherwise the column gets its typed MISSING sentinel.
  const bool have = !v.absent && v.dtype == col->dtype;
  std::vector<std::uint8_t> batch;
  switch (col->dtype) {
    case DType::F32: {
      float x = have ? v.f32 : std::numeric_limits<float>::quiet_NaN();
      appendRecord(batch, col->bufferId, &x, sizeof(x));
      break;
    }
    case DType::I32: {
      std::int32_t x = have ? v.i32 : 0;
      appendRecord(batch, col->bufferId, &x, sizeof(x));
      break;
    }
    case DType::Cat: {
      std::uint32_t x = have ? v.cat : 0u;
      appendRecord(batch, col->bufferId, &x, sizeof(x));
      break;
    }
    case DType::Timestamp: {
      std::int64_t x = have ? v.ts : 0;
      appendRecord(batch, col->bufferId, &x, sizeof(x));
      break;
    }
  }
  ingest_.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
}

void PivotIngest::appendRow(const PendingRow& row) {
  // Emit every column in declaration order so columns stay equal-length (the
  // lockstep invariant). For each column we pick: the rowKey value (rowKey col),
  // the groupKey value (group col), the accumulated field value, or the typed
  // missing sentinel — but every column gets exactly one element.
  for (const std::string& colName : tables_.columnNames(tableId_)) {
    // The row-id column is owned by RowIdentity (appended separately below); skip.
    if (rowId_ && rowId_->bound() && colName == rowId_->columnName()) continue;

    // rowKey column: write the rowKey, dtype-adapted.
    if (rowKeyColumn_ && colName == *rowKeyColumn_) {
      const Column* col = columnOf(colName);
      PivotValue rk;
      switch (col->dtype) {
        case DType::Timestamp: rk = pvTimestamp(row.rowKey); break;
        case DType::I32: rk = pvI32(static_cast<std::int32_t>(row.rowKey)); break;
        case DType::F32: rk = pvF32(static_cast<float>(row.rowKey)); break;
        case DType::Cat: rk = pvCat(static_cast<std::uint32_t>(row.rowKey)); break;
      }
      emitValue(colName, rk);
      continue;
    }

    // groupKey column: write the groupKey code (cat) or its numeric form.
    if (groupKeyColumn_ && colName == *groupKeyColumn_) {
      std::uint32_t g = row.groupKey ? *row.groupKey : 0u;
      emitValue(colName, pvCat(g));
      continue;
    }

    // A mapped field column: write the accumulated value or the missing sentinel.
    auto vit = row.values.find(colName);
    if (vit != row.values.end()) {
      emitValue(colName, vit->second);
    } else {
      // Missing field — emitValue fills the typed sentinel for an off-dtype value.
      emitValue(colName, PivotValue{});
    }
  }

  // ENC-594: append the durable id for this row in lockstep.
  if (rowId_ && rowId_->bound()) {
    rowId_->appendIds(ingest_, 1);
  }

  ++rowsAppended_;
}

std::size_t PivotIngest::flushAll() {
  std::size_t n = 0;
  for (const auto& [key, row] : pending_) {
    appendRow(row);
    ++n;
  }
  pending_.clear();
  return n;
}

bool PivotIngest::flushRow(std::int64_t rowKey,
                           std::optional<std::uint32_t> groupKey) {
  PivotKey key = makeKey(rowKey, groupKey);
  auto it = pending_.find(key);
  if (it == pending_.end()) return false;
  appendRow(it->second);
  pending_.erase(it);
  return true;
}

}  // namespace dc
