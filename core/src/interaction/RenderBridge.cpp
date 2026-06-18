// ENC-654 (B5a) — RenderBridge implementation. See RenderBridge.hpp.
#include "dc/interaction/RenderBridge.hpp"

#include <algorithm>
#include <cstdint>

namespace dc {

std::size_t materializeNodeToTable(const ColumnSchema& schema,
                                   const ColumnStore& cols, Id node,
                                   IngestProcessor& ingest, TableStore& tables,
                                   Id tableId, Id firstBufferId,
                                   const std::string& tableName) {
  tables.defineTable(tableId, tableName);
  std::size_t rows = 0;
  Id buf = firstBufferId;
  for (const auto& c : schema.columns) {
    const std::uint8_t* bytes = nullptr;
    std::uint32_t len = 0;
    std::size_t n = 0;
    switch (c.dtype) {
      case DType::F32: {
        auto v = cols.viewF32(node, c.name);
        n = v.count;
        bytes = reinterpret_cast<const std::uint8_t*>(v.data);
        len = static_cast<std::uint32_t>(v.count * sizeof(float));
        break;
      }
      case DType::I32: {
        auto v = cols.viewI32(node, c.name);
        n = v.count;
        bytes = reinterpret_cast<const std::uint8_t*>(v.data);
        len = static_cast<std::uint32_t>(v.count * sizeof(std::int32_t));
        break;
      }
      case DType::Cat: {
        auto v = cols.viewCat(node, c.name);
        n = v.count;
        bytes = reinterpret_cast<const std::uint8_t*>(v.data);
        len = static_cast<std::uint32_t>(v.count * sizeof(std::uint32_t));
        break;
      }
      case DType::Timestamp: {
        auto v = cols.viewTimestamp(node, c.name);
        n = v.count;
        bytes = reinterpret_cast<const std::uint8_t*>(v.data);
        len = static_cast<std::uint32_t>(v.count * sizeof(std::int64_t));
        break;
      }
      case DType::List:
        continue;  // ragged: not a scalar render source (out of scope)
    }
    rows = std::max(rows, n);
    ingest.ensureBuffer(buf);
    if (bytes && len) {
      ingest.setBufferData(buf, bytes, len);
    } else {
      // Empty column (e.g. all rows filtered out): leave the buffer present but 0B.
      std::uint8_t dummy = 0;
      ingest.setBufferData(buf, &dummy, 0);
    }
    tables.addColumn(tableId, c.name, c.dtype, buf);
    ++buf;
  }
  return rows;
}

}  // namespace dc
