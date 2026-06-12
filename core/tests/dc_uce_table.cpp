// ENC-592 (P1.1) — TableStore + Column model tests.
//
// Covers, per the ticket: column typing / reinterpret, row-count + length
// consistency, append growth (via the UNCHANGED 13-byte ingest feed), the cat
// dictionary, and the timestamp-stays-i64 trap (no f32 view of epoch-ms).
#include "dc/data/TableStore.hpp"
#include "dc/ingest/IngestProcessor.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int passed = 0;
static int failed = 0;

static void check(bool cond, const char* name) {
  if (cond) {
    std::printf("  PASS: %s\n", name);
    ++passed;
  } else {
    std::fprintf(stderr, "  FAIL: %s\n", name);
    ++failed;
  }
}

// Build one 13-byte ingest record (op=1 APPEND) appending `bytes` to `bufferId`,
// using the EXACT existing wire format — no new format for table columns.
static void appendRecord(std::vector<std::uint8_t>& out, dc::Id bufferId,
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

int main() {
  std::printf("=== ENC-592 TableStore + Column model ===\n");

  dc::IngestProcessor ingest;
  dc::TableStore tables;
  auto src = dc::makeBufferByteSource(ingest);

  // Buffer ids for the columns of one table.
  const dc::Id kBufPrice = 100;   // f32
  const dc::Id kBufCount = 101;   // i32
  const dc::Id kBufSymbol = 102;  // cat (u32 codes)
  const dc::Id kBufTime = 103;    // timestamp (i64 epoch-ms)
  const dc::Id kTable = 1;

  // ----- table definition ---------------------------------------------------
  {
    check(tables.defineTable(kTable, "quotes"), "defineTable ok");
    check(!tables.defineTable(kTable, "dup"), "defineTable dup id rejected");
    check(tables.hasTable(kTable), "hasTable");

    check(tables.addColumn(kTable, "price", dc::DType::F32, kBufPrice),
          "addColumn price/f32");
    check(tables.addColumn(kTable, "count", dc::DType::I32, kBufCount),
          "addColumn count/i32");
    check(tables.addColumn(kTable, "symbol", dc::DType::Cat, kBufSymbol),
          "addColumn symbol/cat");
    check(tables.addColumn(kTable, "t", dc::DType::Timestamp, kBufTime),
          "addColumn t/timestamp");

    check(!tables.addColumn(kTable, "price", dc::DType::F32, 999),
          "addColumn dup name rejected");
    check(!tables.addColumn(999, "x", dc::DType::F32, 1),
          "addColumn unknown table rejected");

    auto names = tables.columnNames(kTable);
    check(names.size() == 4 && names[0] == "price" && names[3] == "t",
          "columnNames in insertion order");
  }

  // ----- query by name + dtype ----------------------------------------------
  {
    check(tables.column(kTable, "price") != nullptr, "column by name");
    check(tables.column(kTable, "price", dc::DType::F32) != nullptr,
          "column by name+dtype match");
    check(tables.column(kTable, "price", dc::DType::I32) == nullptr,
          "column dtype mismatch -> null");
    check(tables.column(kTable, "missing") == nullptr, "missing column -> null");
    check(tables.column(kTable, "price")->bufferId == kBufPrice,
          "column carries bufferId");
  }

  // ----- empty table / column has row count 0 -------------------------------
  {
    check(tables.rowCount(kTable, src) == 0, "row count 0 before any append");
    check(tables.columnRowCount(kTable, "price", src) == 0,
          "column row count 0 before append");
    check(tables.rowCountConsistent(kTable, src),
          "empty columns are consistent");
  }

  // ----- cat dictionary -----------------------------------------------------
  // Intern labels -> dense codes; these codes are what land in the cat buffer.
  std::uint32_t cAAPL, cMSFT, cNVDA;
  {
    dc::CatDictionary* dict = tables.catDict(kTable, "symbol");
    check(dict != nullptr, "catDict for cat column");
    check(tables.catDict(kTable, "price") == nullptr,
          "catDict null for non-cat column");
    cAAPL = dict->intern("AAPL");
    cMSFT = dict->intern("MSFT");
    cNVDA = dict->intern("NVDA");
    check(cAAPL == 0 && cMSFT == 1 && cNVDA == 2, "dense codes from 0");
    check(dict->intern("AAPL") == cAAPL, "re-intern returns same code");
    check(dict->labelOf(cMSFT) == "MSFT", "labelOf round-trips");
    check(dict->codeOf("NVDA").value_or(99) == cNVDA, "codeOf round-trips");
    check(!dict->codeOf("ZZZZ").has_value(), "codeOf unknown -> nullopt");
  }

  // ----- append growth via the UNCHANGED 13-byte ingest feed ----------------
  // Append 3 rows in lockstep across all four columns.
  {
    float prices[3] = {10.0f, 20.5f, 30.25f};
    std::int32_t counts[3] = {5, 6, 7};
    std::uint32_t codes[3] = {cAAPL, cMSFT, cNVDA};
    std::int64_t times[3] = {1700000000000LL, 1700000060000LL, 1700000120000LL};

    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufPrice, prices, sizeof(prices));
    appendRecord(batch, kBufCount, counts, sizeof(counts));
    appendRecord(batch, kBufSymbol, codes, sizeof(codes));
    appendRecord(batch, kBufTime, times, sizeof(times));
    ingest.processBatch(batch.data(),
                        static_cast<std::uint32_t>(batch.size()));

    check(tables.rowCount(kTable, src) == 3, "row count 3 after append");
    check(tables.rowCountConsistent(kTable, src),
          "lockstep columns are consistent");
    check(tables.columnRowCount(kTable, "price", src) == 3,
          "per-column row count 3");
  }

  // ----- typed reinterpret --------------------------------------------------
  {
    auto pv = tables.viewF32(kTable, "price", src);
    check(pv.valid() && pv.size() == 3, "viewF32 valid, 3 elems");
    check(pv[0] == 10.0f && pv[2] == 30.25f, "viewF32 values");

    auto cv = tables.viewI32(kTable, "count", src);
    check(cv.valid() && cv[1] == 6, "viewI32 values");

    auto sv = tables.viewCat(kTable, "symbol", src);
    check(sv.valid() && sv[0] == cAAPL && sv[2] == cNVDA, "viewCat codes");
    // codes -> labels through the dictionary
    const dc::CatDictionary* d = tables.catDict(kTable, "symbol");
    check(d->labelOf(sv[1]) == "MSFT", "cat code -> label via dict");

    auto tv = tables.viewTimestamp(kTable, "t", src);
    check(tv.valid() && tv.size() == 3, "viewTimestamp valid");
    check(tv[0] == 1700000000000LL, "timestamp value preserved as i64");
  }

  // ----- the dtype guard: views reject wrong-typed columns ------------------
  {
    // price is F32: an i32/cat/timestamp view of it must be empty even though
    // i32/cat share the 4-byte width (no silent aliasing).
    check(!tables.viewI32(kTable, "price", src).valid(),
          "viewI32 of f32 column rejected");
    check(!tables.viewCat(kTable, "price", src).valid(),
          "viewCat of f32 column rejected");
    check(!tables.viewTimestamp(kTable, "price", src).valid(),
          "viewTimestamp of f32 column rejected");
  }

  // ----- THE TIMESTAMP TRAP: epoch-ms stays i64, never f32 ------------------
  {
    // A timestamp column has NO float view by design — epoch-ms overflows the
    // f32 mantissa (~16.7M). viewF32 of a timestamp column must be empty.
    check(!tables.viewF32(kTable, "t", src).valid(),
          "viewF32 of timestamp column rejected (f64-trap guard)");
    // And the i64 value is bit-exact (a value far past the f32-mantissa wall).
    auto tv = tables.viewTimestamp(kTable, "t", src);
    check(tv[2] == 1700000120000LL, "timestamp i64 exact past 16.7M");
    // Sanity: 1.7e12 truncated through f32 would NOT round-trip — prove the i64
    // path differs from the lossy f32 path.
    float lossy = static_cast<float>(1700000120000LL);
    check(static_cast<std::int64_t>(lossy) != 1700000120000LL,
          "f32 round-trip of epoch-ms IS lossy (why timestamp stays i64)");
  }

  // ----- length consistency: ragged append detected ------------------------
  {
    // Append one more row to ONLY the price column -> columns become ragged.
    float extra = 40.0f;
    std::vector<std::uint8_t> batch;
    appendRecord(batch, kBufPrice, &extra, sizeof(extra));
    ingest.processBatch(batch.data(),
                        static_cast<std::uint32_t>(batch.size()));

    check(tables.columnRowCount(kTable, "price", src) == 4, "price now 4 rows");
    check(tables.columnRowCount(kTable, "count", src) == 3, "count still 3");
    check(!tables.rowCountConsistent(kTable, src),
          "ragged columns flagged inconsistent");
    check(tables.rowCount(kTable, src) == 3,
          "table row count = min of fully-populated rows");
  }

  // ----- updateRange (op 2) edits in place, row count unchanged -------------
  {
    // Patch price[0] = 99.0f via op=2 at offset 0.
    float patched = 99.0f;
    std::vector<std::uint8_t> rec;
    auto u32 = [&rec](std::uint32_t v) {
      rec.push_back(static_cast<std::uint8_t>(v & 0xFF));
      rec.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
      rec.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
      rec.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    };
    rec.push_back(2);  // op = UPDATE_RANGE
    u32(static_cast<std::uint32_t>(kBufPrice));
    u32(0);                 // offset 0
    u32(sizeof(patched));   // 4 bytes
    const auto* p = reinterpret_cast<const std::uint8_t*>(&patched);
    rec.insert(rec.end(), p, p + sizeof(patched));
    ingest.processBatch(rec.data(), static_cast<std::uint32_t>(rec.size()));

    auto pv = tables.viewF32(kTable, "price", src);
    check(pv.valid() && pv[0] == 99.0f, "updateRange patched price[0]");
    check(pv.size() == 4, "updateRange did not change row count");
  }

  // ----- forward-compat hooks (declared, not implemented) -------------------
  {
    std::uint64_t v0 = tables.version(kTable);
    check(v0 > 0, "version is non-zero after structural changes");
    check(tables.setRowIdColumn(kTable, "t"), "setRowIdColumn ok");
    check(tables.rowIdColumn(kTable).value_or("") == "t",
          "rowIdColumn designation recorded (ENC-594 hook)");
    check(!tables.setRowIdColumn(kTable, "nope"),
          "setRowIdColumn unknown column rejected");
    check(tables.version(kTable) > v0, "version bumps on structural change");
  }

  // ----- removeTable --------------------------------------------------------
  {
    check(tables.removeTable(kTable), "removeTable ok");
    check(!tables.hasTable(kTable), "table gone after remove");
    check(!tables.removeTable(kTable), "removeTable unknown -> false");
    check(tables.version(kTable) == 0, "version 0 for unknown table");
  }

  // ----- dtype parsing ------------------------------------------------------
  {
    check(dc::parseDType("f32").value() == dc::DType::F32, "parseDType f32");
    check(dc::parseDType("timestamp").value() == dc::DType::Timestamp,
          "parseDType timestamp");
    check(!dc::parseDType("float64").has_value(), "parseDType unknown -> null");
    check(dc::dtypeByteWidth(dc::DType::Timestamp) == 8, "timestamp width 8");
    check(dc::dtypeByteWidth(dc::DType::Cat) == 4, "cat width 4");
  }

  std::printf("=== ENC-592 Results: %d passed, %d failed ===\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
