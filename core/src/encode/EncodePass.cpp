// ENC-601 (P1.10) — Encode pass + validateDrawItem exact-stride contract.
// ENC-603 — point + line marks; ENC-604 — rect + candle marks.
// See EncodePass.hpp for the contract.
#include "dc/encode/EncodePass.hpp"

#include <cstring>

namespace dc {

const char* toString(Mark m) {
  switch (m) {
    case Mark::Point: return "point";
    case Mark::Line: return "line";
    case Mark::Rect: return "rect";
    case Mark::Candle: return "candle";
    case Mark::RectColor: return "rectColor";
  }
  return "unknown";
}

const char* toString(EncodeError e) {
  switch (e) {
    case EncodeError::Ok: return "ok";
    case EncodeError::UnknownPipeline: return "unknown-pipeline";
    case EncodeError::FormatMismatch: return "format-mismatch";
    case EncodeError::MissingChannel: return "missing-channel";
    case EncodeError::UnresolvableChannel: return "unresolvable-channel";
    case EncodeError::RaggedTable: return "ragged-table";
  }
  return "unknown";
}

// ---------------------------------------------------------------------------
// Mark specs (ENC-603/604): pipeline key + byte-exact format + required channels.
// ---------------------------------------------------------------------------
MarkSpec markSpecOf(Mark mark, LineStyle lineStyle) {
  switch (mark) {
    case Mark::Point:
      // points@1 — Pos2_Clip (x, y), one vertex per row.
      return {"points@1", VertexFormat::Pos2_Clip, {Channel::X, Channel::Y}};
    case Mark::Line:
      if (lineStyle == LineStyle::LineAA) {
        // lineAA@1 — Rect4 (segment p0=xy, p1=zw), instanced.
        return {"lineAA@1", VertexFormat::Rect4, {Channel::X, Channel::Y}};
      }
      // line2d@1 — Pos2_Clip (x, y) LineList.
      return {"line2d@1", VertexFormat::Pos2_Clip, {Channel::X, Channel::Y}};
    case Mark::Rect:
      // instancedRect@1 — Rect4 (x0, y0, x1, y1), one instance per row.
      return {"instancedRect@1", VertexFormat::Rect4,
              {Channel::X, Channel::Y, Channel::X2, Channel::Y2}};
    case Mark::RectColor:
      // ENC-608 keystone — instancedRectColor@1, Rect4Color (rect4 + packed
      // RGBA8 + reserved scalar lane), one instance per row. Same position
      // channels as Rect; the per-row color rides Encoding::setColorField.
      return {"instancedRectColor@1", VertexFormat::Rect4Color,
              {Channel::X, Channel::Y, Channel::X2, Channel::Y2}};
    case Mark::Candle:
      // instancedCandle@1 — Candle6 (x, open, high, low, close, halfWidth).
      return {"instancedCandle@1", VertexFormat::Candle6,
              {Channel::X, Channel::Open, Channel::High, Channel::Low,
               Channel::Close, Channel::Size}};
  }
  return {"", VertexFormat::Pos2_Clip, {}};
}

namespace {

// Append a little-endian f32 to a byte vector (the exact wire all the vertex
// formats use — std::memcpy preserves the host's IEEE-754 little-endian layout,
// matching how the Dawn backends read the attributes).
inline void pushF32(std::vector<std::uint8_t>& out, float v) {
  const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
  out.insert(out.end(), p, p + sizeof(float));
}

// Append a little-endian u32 (ENC-608: the packed RGBA8 color + the reserved
// scalar/row-id lane in the Rect4Color instance record).
inline void pushU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
  out.insert(out.end(), p, p + sizeof(std::uint32_t));
}

// Apply the encoding's color(s) onto the draw item. Phase-1 single uniform color
// (+ candle up/down). Defaults on DrawItem stay if the encoding omits a color.
void applyColors(const Encoding& enc, Mark mark, DrawItem& di) {
  if (const auto& c = enc.color()) {
    di.color[0] = c->r; di.color[1] = c->g; di.color[2] = c->b; di.color[3] = c->a;
  }
  if (mark == Mark::Candle) {
    if (const auto& cu = enc.colorUp()) {
      di.colorUp[0] = cu->r; di.colorUp[1] = cu->g;
      di.colorUp[2] = cu->b; di.colorUp[3] = cu->a;
    }
    if (const auto& cd = enc.colorDown()) {
      di.colorDown[0] = cd->r; di.colorDown[1] = cd->g;
      di.colorDown[2] = cd->b; di.colorDown[3] = cd->a;
    }
  }
}

// A per-row resolved value bundle for one mark, filled by resolveRow(). Only the
// channels a mark needs are read; the rest stay 0.
struct RowVals {
  double x{0}, y{0}, x2{0}, y2{0}, size{0};
  double open{0}, high{0}, low{0}, close{0};
};

// Resolve the channels a `mark` needs for `row`. Returns false (with `err`) on a
// missing/unresolvable required channel — the exact-stride contract cannot pack a
// hole, so the whole compile is rejected.
bool resolveRow(Mark mark, const Encoding& enc, std::size_t row,
                const TableStore& tables, Id tableId,
                const BufferByteSource& src, RowVals& rv, EncodeError& err) {
  auto get = [&](Channel ch, double& dst) -> bool {
    auto v = enc.resolve(ch, row, tables, tableId, src);
    if (!v) {
      // Distinguish unbound (MissingChannel, caught earlier) from unresolvable.
      err = enc.has(ch) ? EncodeError::UnresolvableChannel
                        : EncodeError::MissingChannel;
      return false;
    }
    dst = *v;
    return true;
  };

  switch (mark) {
    case Mark::Point:
    case Mark::Line:
      return get(Channel::X, rv.x) && get(Channel::Y, rv.y);
    case Mark::Rect:
    case Mark::RectColor:
      return get(Channel::X, rv.x) && get(Channel::Y, rv.y) &&
             get(Channel::X2, rv.x2) && get(Channel::Y2, rv.y2);
    case Mark::Candle:
      return get(Channel::X, rv.x) && get(Channel::Open, rv.open) &&
             get(Channel::High, rv.high) && get(Channel::Low, rv.low) &&
             get(Channel::Close, rv.close) && get(Channel::Size, rv.size);
  }
  return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// The shared compile core. Packs rows [fromRow, totalRows) into `out` (bytes) and
// fills `ids` (one per emitted instance/segment). Fills the geometry/draw-item
// descriptors on `res` for the WHOLE table. On any gate failure, sets res.error
// and returns with res.ok == false.
//
// `store`==nullptr => full compile (compile()): out is the WHOLE table's bytes.
// `store`!=nullptr => incremental (compileInto()): out is the tail, written into
// the store at its correct byte offset.
// ---------------------------------------------------------------------------
static void compileCore(const PipelineCatalog& catalog, Mark mark,
                        const Encoding& enc, const TableStore& tables,
                        Id tableId, const BufferByteSource& src,
                        Id geometryId, Id drawItemId, Id vertexBufferId,
                        std::size_t fromRow, const RowIdentity* rowIds,
                        LineStyle lineStyle, CpuBufferStore* store,
                        EncodeResult& res) {
  res = EncodeResult{};
  const MarkSpec spec = markSpecOf(mark, lineStyle);

  // ----- gate 1: pipeline exists (validateDrawItem: UNKNOWN_PIPELINE) ---------
  const PipelineSpec* ps = catalog.find(spec.pipeline);
  if (!ps) {
    res.error = EncodeError::UnknownPipeline;
    res.message = "pipeline not in catalog: " + spec.pipeline;
    return;
  }

  // ----- gate 2: EXACT-STRIDE — format == requiredVertexFormat ----------------
  // This is the validateDrawItem contract enforced at COMPILE time. If a mark's
  // packer format does not equal the pipeline's required format, the renderer
  // would reject the geometry at draw; we reject here instead.
  if (spec.format != ps->requiredVertexFormat) {
    res.error = EncodeError::FormatMismatch;
    res.message = std::string("mark ") + toString(mark) + " packs " +
                  toString(spec.format) + " but " + spec.pipeline + " requires " +
                  toString(ps->requiredVertexFormat);
    return;
  }

  // ----- gate 3: every required channel is bound ------------------------------
  for (Channel ch : spec.required) {
    if (!enc.has(ch)) {
      res.error = EncodeError::MissingChannel;
      res.message = std::string("mark ") + toString(mark) +
                    " requires channel " + toString(ch);
      return;
    }
  }

  // ----- row count + lockstep -------------------------------------------------
  if (!tables.rowCountConsistent(tableId, src)) {
    res.error = EncodeError::RaggedTable;
    res.message = "table columns disagree on row count (lockstep broken)";
    return;
  }
  const std::size_t totalRows = tables.rowCount(tableId, src);

  const std::uint32_t stride = strideOf(spec.format);

  // Per-mark instance/vertex arithmetic over the WHOLE table.
  const bool isLine = (mark == Mark::Line);
  const bool isLineList = isLine && (lineStyle == LineStyle::Line2d);
  const bool isInstanced = (ps->drawMode == DrawMode::InstancedTriangles);

  // Total emitted records (instances or line segments) for the whole table.
  std::uint32_t totalInstances = 0;  // rect/candle/lineAA segments
  std::uint32_t totalVertices = 0;   // what Geometry.vertexCount carries
  if (mark == Mark::Point) {
    totalVertices = static_cast<std::uint32_t>(totalRows);
  } else if (isLineList) {
    // LineList polyline: 2 verts per segment, N-1 segments.
    totalVertices = totalRows >= 2
                        ? static_cast<std::uint32_t>(2 * (totalRows - 1))
                        : 0;
  } else if (isLine) {  // lineAA: instanced segments
    totalInstances = totalRows >= 2
                         ? static_cast<std::uint32_t>(totalRows - 1)
                         : 0;
    totalVertices = totalInstances;  // instanced: vertexCount == instanceCount
  } else {  // rect / candle: one instance per row
    totalInstances = static_cast<std::uint32_t>(totalRows);
    totalVertices = totalInstances;
  }

  // ----- geometry + draw-item descriptors (whole table) -----------------------
  res.geometry.id = geometryId;
  res.geometry.vertexBufferId = vertexBufferId;
  res.geometry.format = spec.format;            // byte-exact; satisfies the gate
  res.geometry.vertexCount = totalVertices;

  res.drawItem.id = drawItemId;
  res.drawItem.pipeline = spec.pipeline;
  res.drawItem.geometryId = geometryId;
  applyColors(enc, mark, res.drawItem);

  res.instanceCount = totalInstances;

  // ----- determine the row range to PACK this call ----------------------------
  // For a line mark, segment k couples rows k and k+1, so appending rows from
  // `fromRow` reactivates segment (fromRow-1): the segment joining the last old
  // row to the first new row. Start packing one row earlier so that boundary
  // segment is (re)emitted with the new tail.
  std::size_t packFrom = fromRow;
  if (isLine && fromRow > 0) packFrom = fromRow - 1;
  if (packFrom > totalRows) packFrom = totalRows;

  // Byte offset in the store where this tail begins (incremental write).
  std::uint32_t storeOffset = 0;
  if (mark == Mark::Point) {
    storeOffset = static_cast<std::uint32_t>(packFrom) * stride;
  } else if (isLineList) {
    // segment s starts at vertex 2*s; first packed segment is `packFrom`.
    storeOffset = static_cast<std::uint32_t>(packFrom) * 2u * stride;
  } else {  // lineAA / rect / candle: one record per (segment|row)
    storeOffset = static_cast<std::uint32_t>(packFrom) * stride;
  }

  // ----- pack -----------------------------------------------------------------
  std::vector<std::uint8_t>& out = res.bytes;
  out.clear();
  std::vector<std::int32_t>& ids = res.instanceRowIds;
  ids.clear();

  auto rowId = [&](std::size_t row) -> std::int32_t {
    if (!rowIds || !rowIds->bound()) return RowIdentity::kNoRowId;
    return rowIds->idAt(src, row);
  };

  EncodeError rowErr = EncodeError::Ok;

  if (mark == Mark::Point) {
    for (std::size_t r = packFrom; r < totalRows; ++r) {
      RowVals rv;
      if (!resolveRow(mark, enc, r, tables, tableId, src, rv, rowErr)) {
        res.error = rowErr;
        res.message = "row " + std::to_string(r) + ": " + toString(rowErr);
        return;
      }
      pushF32(out, static_cast<float>(rv.x));
      pushF32(out, static_cast<float>(rv.y));
      ids.push_back(rowId(r));  // one id per point vertex
    }
  } else if (isLine) {
    // Segment s connects row s -> row s+1. Resolve adjacent rows; cache the
    // previous row's resolved (x,y) so each is computed once.
    if (totalRows >= 2) {
      RowVals prev;
      bool havePrev = false;
      for (std::size_t s = packFrom; s + 1 < totalRows; ++s) {
        // Resolve row s (reuse cache) and row s+1.
        RowVals a, b;
        if (havePrev) {
          a = prev;
        } else if (!resolveRow(mark, enc, s, tables, tableId, src, a, rowErr)) {
          res.error = rowErr;
          res.message = "row " + std::to_string(s) + ": " + toString(rowErr);
          return;
        }
        if (!resolveRow(mark, enc, s + 1, tables, tableId, src, b, rowErr)) {
          res.error = rowErr;
          res.message = "row " + std::to_string(s + 1) + ": " + toString(rowErr);
          return;
        }
        prev = b;
        havePrev = true;

        if (isLineList) {
          // Pos2_Clip LineList: two endpoint vertices per segment.
          pushF32(out, static_cast<float>(a.x));
          pushF32(out, static_cast<float>(a.y));
          pushF32(out, static_cast<float>(b.x));
          pushF32(out, static_cast<float>(b.y));
        } else {
          // Rect4 segment record: (x0,y0,x1,y1).
          pushF32(out, static_cast<float>(a.x));
          pushF32(out, static_cast<float>(a.y));
          pushF32(out, static_cast<float>(b.x));
          pushF32(out, static_cast<float>(b.y));
        }
        ids.push_back(rowId(s));  // segment id = its START row's id
      }
    }
  } else if (mark == Mark::Rect) {
    for (std::size_t r = packFrom; r < totalRows; ++r) {
      RowVals rv;
      if (!resolveRow(mark, enc, r, tables, tableId, src, rv, rowErr)) {
        res.error = rowErr;
        res.message = "row " + std::to_string(r) + ": " + toString(rowErr);
        return;
      }
      // Rect4: x0, y0, x1, y1.
      pushF32(out, static_cast<float>(rv.x));
      pushF32(out, static_cast<float>(rv.y));
      pushF32(out, static_cast<float>(rv.x2));
      pushF32(out, static_cast<float>(rv.y2));
      ids.push_back(rowId(r));
    }
  } else if (mark == Mark::RectColor) {
    // ENC-608 keystone — Rect4Color: x0, y0, x1, y1 (f32) + packed RGBA8 color
    // (per row, pre-resolved) + reserved scalar/row-id lane (0 for now). The
    // color is the only difference from the plain Rect packing; the scale stage
    // pre-resolves it, so the compiler is pure copy (zero compute).
    for (std::size_t r = packFrom; r < totalRows; ++r) {
      RowVals rv;
      if (!resolveRow(mark, enc, r, tables, tableId, src, rv, rowErr)) {
        res.error = rowErr;
        res.message = "row " + std::to_string(r) + ": " + toString(rowErr);
        return;
      }
      auto rgba8 = enc.resolveColorRgba8(r, tables, tableId, src);
      if (!rgba8) {
        res.error = EncodeError::UnresolvableChannel;
        res.message = "row " + std::to_string(r) + ": color column unresolvable";
        return;
      }
      pushF32(out, static_cast<float>(rv.x));
      pushF32(out, static_cast<float>(rv.y));
      pushF32(out, static_cast<float>(rv.x2));
      pushF32(out, static_cast<float>(rv.y2));
      pushU32(out, *rgba8);  // per-instance packed RGBA8 (the keystone)
      // Reserved scalar/row-id lane: 0 today. Documented in
      // DawnInstancedRectColorBackend — a future per-instance ROW ID for picking
      // (ENC-594) rides this lane WITHOUT a new format. Not built now.
      pushU32(out, 0u);
      ids.push_back(rowId(r));
    }
  } else {  // Candle
    for (std::size_t r = packFrom; r < totalRows; ++r) {
      RowVals rv;
      if (!resolveRow(mark, enc, r, tables, tableId, src, rv, rowErr)) {
        res.error = rowErr;
        res.message = "row " + std::to_string(r) + ": " + toString(rowErr);
        return;
      }
      // Candle6: x, open, high, low, close, halfWidth (== Size channel).
      pushF32(out, static_cast<float>(rv.x));
      pushF32(out, static_cast<float>(rv.open));
      pushF32(out, static_cast<float>(rv.high));
      pushF32(out, static_cast<float>(rv.low));
      pushF32(out, static_cast<float>(rv.close));
      pushF32(out, static_cast<float>(rv.size));
      ids.push_back(rowId(r));
    }
  }

  (void)isInstanced;

  // ----- incremental write into the store -------------------------------------
  if (store && !out.empty()) {
    store->writeRange(vertexBufferId, storeOffset, out.data(),
                      static_cast<std::uint32_t>(out.size()));
  } else if (store && out.empty() && packFrom == 0) {
    // Empty table on a fresh compile: reserve a 0-length buffer so the geometry's
    // vertexBufferId exists (a later append will writeRange into it).
    store->reserve(vertexBufferId, 0);
  }

  res.ok = true;
  res.error = EncodeError::Ok;
}

EncodeResult EncodePass::compile(Mark mark, const Encoding& enc,
                                 const TableStore& tables, Id tableId,
                                 const BufferByteSource& src, Id geometryId,
                                 Id drawItemId, Id vertexBufferId,
                                 const RowIdentity* rowIds,
                                 LineStyle lineStyle) const {
  EncodeResult res;
  compileCore(catalog_, mark, enc, tables, tableId, src, geometryId, drawItemId,
              vertexBufferId, /*fromRow=*/0, rowIds, lineStyle,
              /*store=*/nullptr, res);
  return res;
}

EncodeResult EncodePass::compileInto(Mark mark, const Encoding& enc,
                                     const TableStore& tables, Id tableId,
                                     const BufferByteSource& src,
                                     CpuBufferStore& store, Id geometryId,
                                     Id drawItemId, Id vertexBufferId,
                                     std::size_t fromRow,
                                     const RowIdentity* rowIds,
                                     LineStyle lineStyle) const {
  EncodeResult res;
  compileCore(catalog_, mark, enc, tables, tableId, src, geometryId, drawItemId,
              vertexBufferId, fromRow, rowIds, lineStyle, &store, res);
  return res;
}

}  // namespace dc
