// ENC-594 (P1.3) — Stable per-row identity. See RowIdentity.hpp for the design.
#include "dc/data/RowIdentity.hpp"

#include "dc/ingest/IngestProcessor.hpp"

#include <cstring>
#include <limits>

namespace dc {

// Build one 13-byte ingest record (op 1 APPEND) appending `bytes` to `bufferId`,
// reusing the EXACT existing wire format — no new format for the id column.
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

bool RowIdentity::bind(TableStore& tables, Id tableId,
                       const std::string& columnName) {
  const Column* c = tables.column(tableId, columnName, DType::I32);
  if (!c) return false;  // unknown table/column OR not i32
  if (!tables.setRowIdColumn(tableId, columnName)) return false;
  bound_ = true;
  tableId_ = tableId;
  columnName_ = columnName;
  bufferId_ = c->bufferId;
  nextId_ = 0;
  return true;
}

std::vector<std::int32_t> RowIdentity::appendIds(IngestProcessor& ingest,
                                                 std::size_t rowCount) {
  if (!bound_ || rowCount == 0) return {};

  // Refuse to cross the i32 wall so every id is a clean non-negative i32.
  constexpr std::uint32_t kMaxId =
      static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
  if (nextId_ > kMaxId ||
      rowCount > static_cast<std::size_t>(kMaxId - nextId_) + 1) {
    return {};
  }

  std::vector<std::int32_t> ids;
  ids.reserve(rowCount);
  for (std::size_t i = 0; i < rowCount; ++i) {
    ids.push_back(static_cast<std::int32_t>(nextId_++));
  }

  std::vector<std::uint8_t> batch;
  appendRecord(batch, bufferId_, ids.data(),
               static_cast<std::uint32_t>(ids.size() * sizeof(std::int32_t)));
  ingest.processBatch(batch.data(), static_cast<std::uint32_t>(batch.size()));
  return ids;
}

std::size_t RowIdentity::liveRowCount(const BufferByteSource& src) const {
  if (!bound_) return 0;
  const std::uint32_t bytes = src.size ? src.size(bufferId_) : 0;
  return bytes / sizeof(std::int32_t);
}

std::int32_t RowIdentity::idAt(const BufferByteSource& src,
                               std::size_t rowIndex) const {
  if (!bound_) return kNoRowId;
  const std::uint8_t* bytes = src.data ? src.data(bufferId_) : nullptr;
  const std::size_t rows = liveRowCount(src);
  if (!bytes || rowIndex >= rows) return kNoRowId;
  std::int32_t v;
  std::memcpy(&v, bytes + rowIndex * sizeof(std::int32_t), sizeof(std::int32_t));
  return v;
}

std::optional<std::size_t> RowIdentity::indexOfId(const BufferByteSource& src,
                                                  std::int32_t id) const {
  if (!bound_ || id < 0) return std::nullopt;
  const std::uint8_t* bytes = src.data ? src.data(bufferId_) : nullptr;
  const std::size_t rows = liveRowCount(src);
  if (!bytes || rows == 0) return std::nullopt;

  // Ids are appended monotonically, so the live column is sorted ascending: the
  // first live row holds the smallest surviving id. The target index, if present,
  // is (id - frontId). Validate against the actual stored value (defensive vs. a
  // partially-applied batch).
  std::int32_t frontId;
  std::memcpy(&frontId, bytes, sizeof(std::int32_t));
  if (id < frontId) return std::nullopt;  // evicted (or never assigned)
  const std::size_t idx = static_cast<std::size_t>(id - frontId);
  if (idx >= rows) return std::nullopt;  // not yet assigned
  std::int32_t at;
  std::memcpy(&at, bytes + idx * sizeof(std::int32_t), sizeof(std::int32_t));
  if (at != id) return std::nullopt;
  return idx;
}

}  // namespace dc
