// ENC-600 (P1.9) — ENCODING / channel-binding model (RESEARCH §3 + §4.4).
//
// WHAT THIS IS
// ------------
// An ENCODING binds a mark's visual CHANNELS to a data source:
//
//   encoding = { channel -> { scale, field|value } }
//
// A mark emits one INSTANCE per table row; each bound channel resolves, for row
// `i`, to a single scalar (position / size) or an RGBA color, per RESEARCH §4.4:
//
//   * field + scale   ->  scale.map( column[field][i] )         (the common case)
//   * field, no scale ->  column[field][i]                       (identity)
//   * value (const)   ->  the constant, same for every row       (no column read)
//
// The resolved scalars are then PACKED into the exact vertex/instance stride the
// target pipeline requires by the encode pass (ENC-601). This header is ONLY the
// data model + per-row resolution — the packing + validateDrawItem gate live in
// EncodePass.hpp.
//
// SCOPE (Phase 1)
// ---------------
// Numeric channels resolve through a `Scale` (LinearScale today; time/band arrive
// in ENC-597/598). COLOR in Phase 1 is a SINGLE color per draw item — every
// existing pipeline this layer targets (points@1, line2d@1, lineAA@1,
// instancedRect@1, instancedCandle@1) carries one (or two, for candle)
// uniform color, NOT a per-instance color attribute. So a color channel resolves
// to a constant RGBA that lands on the DrawItem, never into the per-row buffer.
// Per-instance color marks (instancedРectColor / instancedPointColor) are Phase 2
// (ENC-608/609) and deliberately out of scope here.
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"
#include "dc/scale/Scale.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// Channel — the named visual roles a mark exposes. A given mark consumes a fixed
// SUBSET (e.g. a rect uses x,y,x2,y2; a candle uses x,open,high,low,close,size).
// The encode pass (per mark) knows which channels it needs and reads them by
// name from the resolved encoding.
// ---------------------------------------------------------------------------
enum class Channel : std::uint8_t {
  X,      // primary x position
  Y,      // primary y position
  X2,     // secondary x (rect right edge / segment end x)
  Y2,     // secondary y (rect top edge / segment end y)
  Size,   // half-extent (candle/bar half-width, point size)
  Open,   // OHLC open  (candle)
  High,   // OHLC high  (candle)
  Low,    // OHLC low   (candle)
  Close,  // OHLC close (candle)
  Color,  // fill / stroke color (Phase 1: a single uniform color per draw item)
};

const char* toString(Channel c);

// ---------------------------------------------------------------------------
// ChannelBinding — how ONE channel gets its value, per RESEARCH §4.4:
//   { scale, field } | { value }
//
//   * field set   -> read column `field` for the row; if `scale` is set, run it
//                    through scale.map() (else identity = the raw column value).
//   * field unset -> the channel is a CONSTANT `value`, identical for every row.
//
// `scale` is a NON-owning pointer (the caller owns the LinearScale, already
// auto-domained for the frame). A null scale on a field binding = identity.
// ---------------------------------------------------------------------------
struct ChannelBinding {
  // Constant binding: field empty, value used for every row.
  static ChannelBinding constant(double v) {
    ChannelBinding b;
    b.value = v;
    return b;
  }

  // Identity field binding: raw column value, no scale.
  static ChannelBinding fieldIdentity(std::string columnName) {
    ChannelBinding b;
    b.field = std::move(columnName);
    return b;
  }

  // Scaled field binding: scale.map(column value).
  static ChannelBinding fieldScaled(std::string columnName, const Scale* s) {
    ChannelBinding b;
    b.field = std::move(columnName);
    b.scale = s;
    return b;
  }

  bool hasField() const { return !field.empty(); }
  bool isConstant() const { return field.empty(); }

  std::string field;          // column name; empty => constant `value`
  const Scale* scale{nullptr};  // non-owning; null => identity (or unused for const)
  double value{0.0};          // used iff field is empty (constant channel)
};

// RGBA color, 0..1 per component (Phase 1 draw-item-level color).
struct Rgba {
  float r{1.0f}, g{1.0f}, b{1.0f}, a{1.0f};
};

// ---------------------------------------------------------------------------
// Encoding — the per-mark channel map: channel -> ChannelBinding. Numeric
// channels (everything except Color) bind here. The Color channel is special: in
// Phase 1 it is a single uniform color carried on the produced DrawItem, so it is
// stored as an optional constant `Rgba` rather than a per-row binding. (A field
// color binding would require a per-instance color attribute — Phase 2.)
// ---------------------------------------------------------------------------
class Encoding {
 public:
  // Bind a numeric channel (X/Y/X2/Y2/Size/Open/High/Low/Close). Replaces any
  // prior binding for that channel. Color is set via setColor()/setColorUpDown().
  Encoding& set(Channel ch, ChannelBinding binding);

  // Convenience constructors mirroring ChannelBinding's factories.
  Encoding& field(Channel ch, std::string col, const Scale* s = nullptr) {
    return set(ch, ChannelBinding::fieldScaled(std::move(col), s));
  }
  Encoding& constant(Channel ch, double v) {
    return set(ch, ChannelBinding::constant(v));
  }

  // The binding for `ch`, or nullptr if unbound. (Color is never returned here —
  // use color()/colorUp()/colorDown().)
  const ChannelBinding* binding(Channel ch) const;

  bool has(Channel ch) const { return binding(ch) != nullptr; }

  // ----- color (Phase 1: single uniform color(s) per draw item) --------------
  Encoding& setColor(Rgba c) {
    color_ = c;
    return *this;
  }
  // Candle has TWO uniform colors (up / down). These map to DrawItem.colorUp /
  // .colorDown; for non-candle marks, only setColor() is meaningful.
  Encoding& setColorUp(Rgba c) {
    colorUp_ = c;
    return *this;
  }
  Encoding& setColorDown(Rgba c) {
    colorDown_ = c;
    return *this;
  }

  const std::optional<Rgba>& color() const { return color_; }
  const std::optional<Rgba>& colorUp() const { return colorUp_; }
  const std::optional<Rgba>& colorDown() const { return colorDown_; }

  // ----- ENC-608 per-INSTANCE color (the keystone: instancedRectColor@1) ------
  //
  // Phase 1 color is ONE uniform per draw item; the keystone rect needs a
  // DIFFERENT color per instance. The color is PRE-RESOLVED upstream (a future
  // color scale, ENC-610/611) into a packed RGBA8 (one u32 per row, byte order
  // R,G,B,A in the low..high bytes) and stored as an i32/cat column; the encode
  // pass copies it straight into the instance record (zero compute). Binding the
  // pre-packed column here is the in-scope path; tests hand-pack the column.
  //
  //   setColorField(col) — per-row packed RGBA8 from i32/cat column `col`.
  Encoding& setColorField(std::string col) {
    colorField_ = std::move(col);
    return *this;
  }
  const std::optional<std::string>& colorField() const { return colorField_; }

  // Resolve the per-instance packed RGBA8 for `rowIndex`. Reads the bound color
  // column (i32 bit-pattern) when setColorField() was used; otherwise falls back
  // to the constant setColor() value packed to RGBA8 (so an all-constant encoding
  // still produces a valid per-instance buffer). Returns nullopt only when a
  // color FIELD was bound but its column is missing/short for `rowIndex`.
  std::optional<std::uint32_t> resolveColorRgba8(
      std::size_t rowIndex, const TableStore& tables, Id tableId,
      const BufferByteSource& src) const;

  // ----- per-row resolution (RESEARCH §4.4) ----------------------------------
  //
  // Resolve a numeric channel for row `rowIndex` to a single f64. Reads the bound
  // column (f32 view) through `tables`/`src` and, if a scale is attached, runs it
  // through scale.map(); a const binding ignores the row and returns `value`.
  //
  // Returns nullopt iff the channel is unbound, or its field column is
  // missing/non-f32/too short for `rowIndex`. The encode pass treats nullopt as a
  // hard compile error (a mark cannot pack a missing required channel).
  std::optional<double> resolve(Channel ch, std::size_t rowIndex,
                                const TableStore& tables, Id tableId,
                                const BufferByteSource& src) const;

  // The set of field column names this encoding references (for table validation
  // / dirty tracking). Empty for an all-constant encoding.
  std::vector<std::string> referencedFields() const;

 private:
  std::unordered_map<Channel, ChannelBinding> bindings_;
  std::optional<Rgba> color_;
  std::optional<Rgba> colorUp_;
  std::optional<Rgba> colorDown_;
  std::optional<std::string> colorField_;  // ENC-608 per-instance packed RGBA8
};

}  // namespace dc
