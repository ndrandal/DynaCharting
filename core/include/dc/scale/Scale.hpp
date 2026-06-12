// ENC-596 (P1.5) — SCALE subsystem: linear scale + streaming O(Δ) auto-domain.
//
// WHAT THIS IS
// ------------
// A SCALE maps a column's numeric DOMAIN → a visual RANGE (RESEARCH §3 + §4.2).
// It is the highest-leverage primitive of the data→visual layer: it replaces the
// single baked affine transform (the mat3 is demoted to pan/zoom viewport) and —
// crucially — replaces the O(N) full rescan in `AutoScale::computeYRange` with a
// STREAMING running-reducer auto-domain that updates in O(Δ) as a column grows.
//
// THE STREAMING AUTO-DOMAIN (the correctness win, RESEARCH §4.2)
// -------------------------------------------------------------
// Columns grow append-only via the unchanged 13-byte ingest feed (op 1 APPEND).
// `RunningDomain` keeps a running [min,max] and only folds in the NEW tail rows
// since it was last observed (O(Δ) per tick), instead of rescanning all N rows
// every frame. Over an append-only stream this is bit-identical to a brute-force
// O(N) min/max — the unit test proves that equivalence against a growing column.
//
// SCOPE (ENC-596 only)
// --------------------
// ONLY the linear scale + streaming auto-domain + the Scale interface. Time /
// band / point / color scales are SEPARATE tickets (ENC-597/598/610…). The Scale
// interface is shaped so those extend cleanly: a scale owns a Domain (numeric
// [min,max] here; an ordered category set for band/point later) and a Range, and
// exposes map()/invert(). NiceTicks integration is ENC-599 — `nice()` here is a
// minimal, self-contained 1/2/2.5/5·10ᵏ rounding so the interface has the hook
// without pulling in the full ticks engine.
#pragma once

#include "dc/data/TableStore.hpp"
#include "dc/ids/Id.hpp"

#include <cstddef>
#include <string>

namespace dc {

// ---------------------------------------------------------------------------
// ScaleType — discriminates scale kinds. Only Linear is implemented here; the
// other values are reserved so the enum (and any switch over it) is stable as
// later tickets land their scales.
// ---------------------------------------------------------------------------
enum class ScaleType : std::uint8_t {
  Linear,  // ENC-596 (this ticket)
  // --- reserved for later tickets (NOT implemented here) ---
  Log,     // ENC-597
  Time,    // ENC-597
  Band,    // ENC-598
  Point,   // ENC-598
  Color,   // ENC-610
};

// ---------------------------------------------------------------------------
// Domain — the numeric interval [min,max] a scale maps FROM. Starts empty
// (min > max sentinel); folding any value makes it non-empty. This is the piece
// the streaming reducer maintains. (Band/point scales will carry an ordered
// category set instead — a different Domain shape behind the same Scale API.)
// ---------------------------------------------------------------------------
struct Domain {
  double min{0.0};
  double max{0.0};
  bool empty{true};

  // Fold a single value into [min,max] in O(1). The first value seeds both ends.
  void fold(double v) {
    if (empty) {
      min = max = v;
      empty = false;
    } else {
      if (v < min) min = v;
      if (v > max) max = v;
    }
  }

  // Span = max - min. 0 for an empty or degenerate (all-equal) domain.
  double span() const { return empty ? 0.0 : (max - min); }
};

// ---------------------------------------------------------------------------
// Range — the visual interval [r0,r1] a scale maps TO (e.g. pixels or clip
// coords). May be inverted (r0 > r1) for a flipped axis (screen-y grows down).
// ---------------------------------------------------------------------------
struct Range {
  double r0{0.0};
  double r1{1.0};
};

// ---------------------------------------------------------------------------
// RunningDomain — the O(Δ) streaming auto-domain reducer. Wraps a Domain and
// remembers how many rows it has already folded; reduceFrom*() folds ONLY the
// new tail rows of an append-only column, so a per-tick update costs O(Δ), not
// O(N). The result is identical to a full O(N) min/max over the whole column.
//
// INVARIANTS / CAVEATS
//   * Append-only: assumes rows are only appended (never removed or reordered).
//     If the observed count SHRINKS (e.g. the column was replaced), reset() must
//     be called and the whole column re-folded — otherwise the running [min,max]
//     would be stale. consumeCount() lets a caller detect a shrink.
//   * in-place updateRange (op 2) edits existing rows WITHOUT growing the count;
//     those edits are NOT seen by an O(Δ) tail fold (a fundamental property of a
//     streaming reducer). Auto-domain is defined over the append-only stream.
// ---------------------------------------------------------------------------
class RunningDomain {
 public:
  // The accumulated domain over all rows folded so far.
  const Domain& domain() const { return domain_; }

  // How many rows have been folded (the high-water mark of the tail).
  std::size_t consumedCount() const { return consumed_; }

  // Drop all accumulated state. Call before re-folding a column whose row count
  // shrank or whose identity changed.
  void reset() {
    domain_ = Domain{};
    consumed_ = 0;
  }

  // Fold a single appended value (advances the consumed high-water mark by 1).
  void foldOne(double v) {
    domain_.fold(v);
    ++consumed_;
  }

  // Fold the NEW tail of a contiguous f64 array of length `count`. Folds only
  // rows [consumed_, count); a no-op if nothing new arrived. If `count` is LESS
  // than what we already consumed (a shrink), reset() + a full re-fold is the
  // caller's responsibility — here we conservatively reset and fold all.
  void reduceFrom(const double* data, std::size_t count);

  // Same, over an f32 array (the native GPU column type). Values widen to f64.
  void reduceFrom(const float* data, std::size_t count);

  // Stream the new tail of an f32 column bound by (tableId, columnName) through
  // a TableStore + byte source. Returns false if the column is missing or not an
  // f32 column (a timestamp column has no f32 view by design — use a time scale).
  bool reduceColumnF32(const TableStore& tables, Id tableId,
                       const std::string& columnName,
                       const BufferByteSource& src);

 private:
  Domain domain_;
  std::size_t consumed_{0};
};

// ---------------------------------------------------------------------------
// Scale — the abstract scale interface. A scale owns a Domain and a Range and
// maps between them. Concrete scales (LinearScale here) implement map/invert.
// The interface is intentionally minimal so band/point/log/time scales extend it
// without redesign.
// ---------------------------------------------------------------------------
class Scale {
 public:
  virtual ~Scale() = default;

  virtual ScaleType type() const = 0;

  // Map a domain value → range value.
  virtual double map(double domainValue) const = 0;

  // Inverse: map a range value → domain value. (A band/point scale's invert may
  // be approximate or snap-to-slot; that's fine under this contract.)
  virtual double invert(double rangeValue) const = 0;

  const Domain& domain() const { return domain_; }
  const Range& range() const { return range_; }

  void setDomain(const Domain& d) { domain_ = d; }
  void setDomain(double lo, double hi) {
    domain_.min = lo;
    domain_.max = hi;
    domain_.empty = false;
  }
  void setRange(const Range& r) { range_ = r; }
  void setRange(double r0, double r1) {
    range_.r0 = r0;
    range_.r1 = r1;
  }

 protected:
  Domain domain_;
  Range range_{0.0, 1.0};
};

// ---------------------------------------------------------------------------
// LinearScale — y = r0 + (x - dMin)/(dMax - dMin) · (r1 - r0). Subsumes the
// affine sx/tx the baked transform used for data axes. invert() is the exact
// algebraic inverse.
//
// DEGENERATE DOMAIN (all-equal or single value, span == 0): there is no unique
// slope. We map every domain value to the MIDPOINT of the range (D3-style "park
// it in the middle"), and invert() returns the domain min — both well-defined
// and matching d3's behaviour for a zero-width domain.
//
// AUTO-DOMAIN: bind an f32 column with bindColumn(); each updateDomain() folds
// the new tail rows in O(Δ) and copies the running [min,max] into the scale's
// domain. The scale is then ready to map() the live data range.
// ---------------------------------------------------------------------------
class LinearScale : public Scale {
 public:
  LinearScale() = default;
  LinearScale(const Domain& d, const Range& r) {
    domain_ = d;
    range_ = r;
  }

  ScaleType type() const override { return ScaleType::Linear; }

  double map(double domainValue) const override;
  double invert(double rangeValue) const override;

  // Round the domain outward to "nice" 1/2/2.5/5·10ᵏ bounds over `targetTicks`
  // intervals. Minimal, self-contained (full NiceTicks is ENC-599). No-op on an
  // empty or degenerate domain. Returns *this for chaining.
  LinearScale& nice(int targetTicks = 5);

  // ----- streaming auto-domain ----------------------------------------------

  // Bind an f32 column as this scale's auto-domain source. Resets any prior
  // accumulated streaming state.
  void bindColumn(Id tableId, std::string columnName);

  bool hasBoundColumn() const { return bound_; }

  // Fold the bound column's NEW tail rows (O(Δ)) and adopt the running [min,max]
  // as the scale domain. No-op (returns false) if no column is bound, the column
  // is missing/non-f32, or no rows have arrived yet. Range is left untouched.
  bool updateDomain(const TableStore& tables, const BufferByteSource& src);

  // The underlying streaming reducer (exposed for inspection / tests).
  const RunningDomain& runningDomain() const { return running_; }

 private:
  RunningDomain running_;
  Id boundTable_{kInvalidId};
  std::string boundColumn_;
  bool bound_{false};
};

}  // namespace dc
