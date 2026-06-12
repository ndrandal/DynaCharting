// ENC-601 (P1.10) — ENCODE PASS + validateDrawItem exact-stride contract
//                    (RESEARCH §3 + §4.3). THE highest-risk Phase-1 ticket.
//
// WHAT THIS IS
// ------------
// The compiler that turns (table, mark, encoding) into a renderable DrawItem +
// Geometry whose vertex/instance buffer is BYTE-FOR-BYTE the exact stride the
// target Dawn pipeline requires. One mark INSTANCE per table row; the encode pass
// resolves each channel (ENC-600) and packs it at the exact offset/type the
// pipeline's `requiredVertexFormat` mandates, then writes those bytes into a
// CpuBufferStore (class-1 incremental — only the appended tail).
//
// THE EXACT-STRIDE CONTRACT (the risk, RESEARCH §3 validateDrawItem gate)
// ----------------------------------------------------------------------
// The renderer is dumb: it trusts that a geometry bound to pipeline P has bytes
// laid out EXACTLY as P's vertex attributes read them. The CommandProcessor
// enforces `geometry.format == pipeline.requiredVertexFormat` in validateDrawItem
// BEFORE any draw. The encode pass mirrors that gate at COMPILE time: a mark
// declares the format it packs to; compile() asserts that format equals the
// pipeline's requiredVertexFormat (looked up in the PipelineCatalog) and REJECTS
// otherwise — you can never produce geometry the renderer would reject at draw.
// The produced Geometry.format is the mark's format, so the downstream
// validateDrawItem is guaranteed to pass.
//
// The packers are byte-exact against the EXISTING formats (Types.hpp / the Dawn
// backends), NOT a new format:
//   * points@1     : Pos2_Clip  (8B  : f32 x, f32 y)             one vertex/row
//   * line2d@1     : Pos2_Clip  (8B  : f32 x, f32 y)  LineList — 2*(N-1) verts
//   * lineAA@1     : Rect4      (16B : f32 x0,y0,x1,y1) instanced — N-1 segments
//   * instancedRect@1   : Rect4   (16B : f32 x0,y0,x1,y1)        one instance/row
//   * instancedCandle@1 : Candle6 (24B : f32 x,open,high,low,close,halfWidth)
//                                                               one instance/row
// Color is NOT in any of these buffers — every targeted pipeline takes a single
// uniform color (candle: two, up/down). The encode pass routes the encoding's
// color onto the DrawItem (color / colorUp / colorDown), keeping the per-row
// buffer byte-exact. Per-instance color is Phase 2 (ENC-608/609).
//
// ROW-ID THREADING (ENC-594) — a DELIBERATE design choice
// -------------------------------------------------------
// Every targeted format is byte-locked; there is NO spare lane for a durable row
// id inside the vertex/instance record without breaking the stride the renderer
// reads. So the encode pass carries the row id OUT-OF-BAND: alongside the packed
// buffer it produces a parallel `instanceRowIds` array — one durable id per
// emitted instance, in instance order — reserved for the future per-instance
// picking path (ENC-594's rowIdColumn -> rendered instance -> source row). The id
// is NOT smuggled into the format. (If picking later needs the id ON the GPU, it
// rides the Phase-2 per-instance-color formats, which have room; the Phase-1
// formats stay exact.) This is the open design question the exact-stride contract
// forces, resolved here in favor of byte-exactness.
//
// INCREMENTAL (class-1, RESEARCH §3/§4.4)
// ---------------------------------------
// compileInto() packs only the NEWLY-appended rows and writeRange()s them at the
// correct tail offset of the CpuBufferStore buffer, so a per-tick update is O(Δ),
// not O(N) — the dirty-range coalescing then collapses an append to one range.
#pragma once

#include "dc/data/RowIdentity.hpp"
#include "dc/data/TableStore.hpp"
#include "dc/encode/Encoding.hpp"
#include "dc/ids/Id.hpp"
#include "dc/pipelines/PipelineCatalog.hpp"
#include "dc/render/CpuBufferStore.hpp"
#include "dc/scene/Geometry.hpp"
#include "dc/scene/Types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// Mark — the four Phase-1 mark primitives (RESEARCH §4.3), each mapped to ONE
// existing pipeline + the channels it consumes.
//   Point  -> points@1         (x, y)
//   Line   -> line2d@1 (default) or lineAA@1 (when antialiased)  (x, y per row)
//   Rect   -> instancedRect@1   (x, y, x2, y2)
//   Candle -> instancedCandle@1 (x, open, high, low, close, size=halfWidth)
// ---------------------------------------------------------------------------
enum class Mark : std::uint8_t {
  Point,
  Line,
  Rect,
  Candle,
  // ENC-608 (P2.1) — the KEYSTONE per-cell-colored rect.
  //   RectColor -> instancedRectColor@1  (x, y, x2, y2 + per-row packed RGBA8)
  // Same geometry as Rect, but the fill color resolves PER ROW (pre-packed RGBA8
  // via Encoding::setColorField) into the Rect4Color instance record instead of a
  // single uniform — so weather-radar/correlation/footprint/pie render native.
  RectColor,
  // ENC-609 (P2.2) — the per-POINT color + size scatter.
  //   PointColor -> instancedPointColor@1  (x, y + per-row packed RGBA8 + size)
  // One instance per row; the per-row color rides Encoding::setColorField (or the
  // constant-color fallback) and the per-row SIZE (point diameter in pixels)
  // resolves through the Size channel — a scatter where every dot has its own
  // color and size, not a uniform color / 1px point.
  PointColor,
  // ENC-613 (P2.3) — the polar ARC / wedge mark.
  //   Arc -> triGradient@1 (Pos2Color4) via POLAR coords
  // A wedge spanning [theta, theta2] x [r, r2] (X=theta, Y=r, X2=theta2, Y2=r2)
  // tessellated into a triangle fan in CLIP space (cx + r*cos theta, cy + r*sin
  // theta) — what radial/pie/rose charts need (the affine mat3 cannot express
  // polar). Color is the per-row uniform (packed per-vertex into the gradient
  // format). See compileArcPolar in EncodePass.cpp.
  Arc,
};

const char* toString(Mark m);

// Line rendering variant: line2d (1px LineList) vs lineAA (instanced AA quads).
enum class LineStyle : std::uint8_t {
  Line2d,  // line2d@1  — Pos2_Clip LineList
  LineAA,  // lineAA@1  — Rect4 instanced segments
};

// ---------------------------------------------------------------------------
// MarkSpec — the static facts about a mark: its pipeline key, the vertex format
// it packs to, and the channels it REQUIRES. compile() validates the encoding
// supplies every required channel and that the format matches the pipeline.
// ---------------------------------------------------------------------------
struct MarkSpec {
  std::string pipeline;          // "points@1" / "instancedCandle@1" / ...
  VertexFormat format;           // the byte-exact target format
  std::vector<Channel> required; // channels that MUST be bound
};

// Resolve a (mark, lineStyle) to its MarkSpec. lineStyle is ignored for non-line
// marks.
MarkSpec markSpecOf(Mark mark, LineStyle lineStyle = LineStyle::Line2d);

// ---------------------------------------------------------------------------
// ENC-613 — ArcOptions: how an Arc/wedge mark is tessellated + placed in polar
// coords. Only consulted for Mark::Arc; ignored by every other mark.
//   * polar — the clip-space center the (angle, radius) is measured from.
//   * segments — triangle fan segments PER UNIT of angular span (clamped to a
//     sane min/max per wedge) controlling the smoothness of the arc edge.
// ---------------------------------------------------------------------------
struct ArcOptions {
  PolarParams polar{};       // center (defaults to clip origin 0,0)
  int segmentsPerArc{24};    // fan segments across each wedge's angular span
};

// ---------------------------------------------------------------------------
// EncodeError — why a compile was rejected (the validateDrawItem-at-compile gate
// + channel-completeness gate). Ok carries no error.
// ---------------------------------------------------------------------------
enum class EncodeError : std::uint8_t {
  Ok,
  UnknownPipeline,        // pipeline key not in the catalog
  FormatMismatch,         // mark's format != pipeline.requiredVertexFormat
  MissingChannel,         // a required channel is unbound
  UnresolvableChannel,    // a bound channel's column is missing / non-f32 / short
  RaggedTable,            // columns disagree on row count (lockstep broken)
};

const char* toString(EncodeError e);

// ---------------------------------------------------------------------------
// EncodeResult — the product of a compile: a DrawItem-shaped descriptor + a
// Geometry-shaped descriptor + the packed bytes + the out-of-band row ids.
//
// The caller wires `geometry` + `drawItem` into the Scene (the format on
// `geometry` is exactly the mark's format, so the Scene's validateDrawItem will
// accept it) and the packed bytes are already in the CpuBufferStore at
// `geometry.vertexBufferId` (compileInto) — or available here (compile()).
// ---------------------------------------------------------------------------
struct EncodeResult {
  bool ok{false};
  EncodeError error{EncodeError::Ok};
  std::string message;

  // Descriptor for the Geometry resource to create (format is byte-exact).
  Geometry geometry{};

  // Descriptor for the DrawItem to create. Carries the pipeline key + the
  // uniform color(s) the encoding resolved (color / colorUp / colorDown).
  DrawItem drawItem{};

  // The packed vertex/instance bytes for the WHOLE table (compile()), or the
  // appended tail only (compileInto leaves the bytes in the store, this holds the
  // tail it wrote). Byte-exact to `geometry.format`'s stride.
  std::vector<std::uint8_t> bytes;

  // ENC-594 row-id threading: one durable id per EMITTED INSTANCE, in instance
  // order. For point/rect/candle that is one id per row; for a line it is one id
  // per SEGMENT (the row at the segment's start endpoint). Empty if the table has
  // no rowIdColumn / no RowIdentity was supplied. NOT packed into `bytes`.
  std::vector<std::int32_t> instanceRowIds;

  // Number of emitted instances (rect/candle/lineAA) or 0 for vertex marks.
  std::uint32_t instanceCount{0};
};

// ---------------------------------------------------------------------------
// EncodePass — the (table, mark, encoding) -> DrawItem + Geometry compiler.
//
// Stateless w.r.t. data (it reads the live table each call); holds only the
// PipelineCatalog used for the validateDrawItem-at-compile gate. The Geometry /
// DrawItem ids are supplied by the caller (it owns id allocation).
// ---------------------------------------------------------------------------
class EncodePass {
 public:
  EncodePass() = default;

  // Compile (table, mark, encoding) into a full EncodeResult, packing ALL rows.
  // Validates the exact-stride contract at compile time and REJECTS on any
  // EncodeError (result.ok == false, result.error set, result.bytes empty).
  //
  //   geometryId / drawItemId / vertexBufferId — ids the caller pre-allocated.
  //   rowIds — optional ENC-594 identity (nullptr = no row-id threading).
  //   arc — ENC-613 polar/tessellation options (only used for Mark::Arc).
  EncodeResult compile(Mark mark, const Encoding& enc, const TableStore& tables,
                       Id tableId, const BufferByteSource& src, Id geometryId,
                       Id drawItemId, Id vertexBufferId,
                       const RowIdentity* rowIds = nullptr,
                       LineStyle lineStyle = LineStyle::Line2d,
                       const ArcOptions& arc = ArcOptions{}) const;

  // Incremental compile: pack ONLY rows [fromRow, totalRows) and writeRange()
  // them into `store` at the correct tail offset (class-1 O(Δ)). The full-table
  // geometry/draw-item descriptor is returned in the result (vertexCount /
  // instanceCount reflect the WHOLE table); result.bytes holds only the tail
  // written this call; result.instanceRowIds holds only the new instances' ids.
  //
  // `fromRow` is the row count packed on the PREVIOUS tick (0 on the first). The
  // caller tracks it. A `fromRow` >= totalRows is a no-op tail (ok, empty bytes).
  EncodeResult compileInto(Mark mark, const Encoding& enc,
                           const TableStore& tables, Id tableId,
                           const BufferByteSource& src, CpuBufferStore& store,
                           Id geometryId, Id drawItemId, Id vertexBufferId,
                           std::size_t fromRow,
                           const RowIdentity* rowIds = nullptr,
                           LineStyle lineStyle = LineStyle::Line2d,
                           const ArcOptions& arc = ArcOptions{}) const;

 private:
  PipelineCatalog catalog_;
};

}  // namespace dc
