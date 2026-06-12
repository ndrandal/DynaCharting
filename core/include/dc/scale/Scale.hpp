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
#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// ---------------------------------------------------------------------------
// ScaleType — discriminates scale kinds. Only Linear is implemented here; the
// other values are reserved so the enum (and any switch over it) is stable as
// later tickets land their scales.
// ---------------------------------------------------------------------------
enum class ScaleType : std::uint8_t {
  Linear,  // ENC-596
  Time,    // ENC-597
  Band,    // ENC-598
  Point,   // ENC-598
  // --- reserved for later tickets (NOT implemented here) ---
  Log,     // (pow/sqrt family — separate)
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

  // Same, over an i64 array (e.g. a Timestamp epoch-ms column). Values widen to
  // f64 LOSSLESSLY (f64 has 52 mantissa bits, covering the full epoch-ms range —
  // the whole point of keeping time off the f32 path). ENC-597.
  void reduceFrom(const std::int64_t* data, std::size_t count);

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
// ScaleTick — one axis/legend tick: a value in the scale's DOMAIN space plus a
// formatted label for chrome. (ENC-599.) For a time scale the value is epoch-ms
// (CPU f64 — never the GPU-side f32 offset) and the label is a UTC date/time;
// for an ordinal (band/point) scale the value is the category position and the
// label is the category string.
// ---------------------------------------------------------------------------
struct ScaleTick {
  double value{0.0};
  std::string label;
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

  // Emit "nice" ticks for axis/legend chrome from the scale's CURRENT (live)
  // domain (ENC-599). `targetCount` is a hint for the number of intervals; the
  // realized count is rounded to nice boundaries. Returns an empty vector for an
  // empty/degenerate domain. Default: no ticks (overridden by concrete scales).
  virtual std::vector<ScaleTick> ticks(int targetCount = 5) const {
    (void)targetCount;
    return {};
  }

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

  // Emit nice tick values + labels over the current domain via the shared
  // NiceTicks engine (ENC-599). Labels are the formatted numeric values.
  std::vector<ScaleTick> ticks(int targetCount = 5) const override;

  // Round the domain outward to "nice" 1/2/2.5/5·10ᵏ bounds over `targetTicks`
  // intervals, via the shared NiceTicks engine (ENC-599 — was a self-contained
  // stub in ENC-596). No-op on an empty or degenerate domain. Returns *this.
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

// ===========================================================================
// ENC-597 — TimeScale (the f64/f32 epoch-ms correctness trap)
// ===========================================================================
//
// THE TRAP (RESEARCH §4.1/§4.2 + §7): epoch-ms today is ~1.7e12, which overflows
// the f32 mantissa (~16.7M / 2^24). Casting an epoch-ms to f32 silently quantizes
// time to ~minute/hour buckets — a real, named correctness trap. WebGPU has no
// f64, so a time axis CANNOT push absolute epoch-ms to the GPU.
//
// THE MITIGATION: keep the i64 epoch-ms base on the CPU. All domain math runs in
// f64 epoch-ms. Before any GPU use, normalize a timestamp to a RELATIVE f32
// offset against a CPU-held base epoch (the domain min): `(t - baseEpochMs)` is
// small (a multi-day span is ~1e8 ms, well under the 16.7M *relative* budget once
// expressed in a sensible unit — and exactly representable for the spans we care
// about). map()/invert() operate purely in f64; normalizedOffsetF32() is the only
// f32 surface and is what a GPU transform would consume.
//
// AUTO-DOMAIN: reuses RunningDomain over a Timestamp (i64 epoch-ms) column,
// widening each i64 to f64 (lossless for epoch-ms — f64 has 52 mantissa bits,
// covering the full epoch-ms range). The base epoch is re-pinned to the running
// domain min on every updateDomain().
class TimeScale : public Scale {
 public:
  TimeScale() = default;
  TimeScale(double minEpochMs, double maxEpochMs, const Range& r) {
    setDomain(minEpochMs, maxEpochMs);
    range_ = r;
    baseEpochMs_ = minEpochMs;
  }

  ScaleType type() const override { return ScaleType::Time; }

  // map/invert run entirely in f64 epoch-ms — no f32 anywhere on this path.
  double map(double epochMs) const override;
  double invert(double rangeValue) const override;

  // The CPU-held base epoch (epoch-ms) the relative f32 offset is measured from.
  // Defaults to the domain min; re-pinned on updateDomain().
  double baseEpochMs() const { return baseEpochMs_; }
  void setBaseEpochMs(double base) { baseEpochMs_ = base; }

  // The ONLY f32 surface: a timestamp normalized to a relative f32 offset (ms)
  // against the CPU base epoch. This is what a GPU transform consumes; it is
  // exactly representable for the spans a single view shows (no f32 epoch-ms cast
  // ever happens). Out-of-band callers must pass an epoch-ms (NOT a pre-offset).
  float normalizedOffsetF32(double epochMs) const {
    return static_cast<float>(epochMs - baseEpochMs_);
  }

  // Nice time ticks over the live epoch-ms domain (ENC-599). Values are epoch-ms
  // (f64); labels are UTC date/time strings. Backed by NiceTimeTicks.
  std::vector<ScaleTick> ticks(int targetCount = 6) const override;

  // ----- streaming auto-domain over a Timestamp column ----------------------

  // Bind an i64 Timestamp column as the auto-domain source. Resets prior state.
  void bindColumn(Id tableId, std::string columnName);
  bool hasBoundColumn() const { return bound_; }

  // Fold the bound Timestamp column's NEW tail rows (O(Δ)), adopt the running
  // [min,max] (epoch-ms, f64) as the domain, and re-pin the base epoch to the
  // domain min. Returns false if no column is bound, the column is missing/not a
  // Timestamp column, or no rows have arrived. Range is left untouched.
  bool updateDomain(const TableStore& tables, const BufferByteSource& src);

  const RunningDomain& runningDomain() const { return running_; }

 private:
  double baseEpochMs_{0.0};
  RunningDomain running_;
  Id boundTable_{kInvalidId};
  std::string boundColumn_;
  bool bound_{false};
};

// ===========================================================================
// ENC-598 — Ordinal scales: BandScale + PointScale
// ===========================================================================
//
// An ORDINAL scale maps a discrete CATEGORY (by its dictionary code, RESEARCH
// §4.1 `cat` dtype) to a position in the range. The category set GROWS over the
// stream: new categories arrive as new dictionary codes and extend the domain in
// arrival order. Both scales share an OrdinalDomain (the ordered code set, mirror
// of the TableStore Cat dictionary) and differ only in placement math.
//
//   BandScale (RESEARCH §4.2 "category→slot+pad"): each category owns a BAND of
//   width `bandwidth()`; bars/heatmap-columns fill it. Inner/outer padding carve
//   gaps. map(code) returns the band's START edge; center(code) its midpoint.
//
//   PointScale (RESEARCH §4.2 "category→position"): each category is a POINT
//   (line-per-category, ticks); points sit at band centers of an equivalent band
//   layout with bandwidth → 0 (the standard point==band-with-zero-width identity).
//
// Auto-domain reuses the TableStore Cat dictionary: updateDomain() folds the NEW
// dictionary codes (O(Δ) over the growing dict) into the ordered domain.

// OrdinalDomain — the ordered set of category codes a band/point scale maps from.
// Codes are appended in arrival order; ordinalOf(code) gives the dense 0-based
// position (or -1 if absent). Mirrors the Cat dictionary's code space.
struct OrdinalDomain {
  std::vector<std::uint32_t> codes;  // arrival-ordered category codes

  std::size_t size() const { return codes.size(); }
  bool empty() const { return codes.empty(); }
  void clear() { codes.clear(); }

  // Append `code` if not already present. Returns true if newly added.
  bool add(std::uint32_t code);

  // Dense 0-based ordinal of `code`, or -1 if not in the domain.
  std::ptrdiff_t ordinalOf(std::uint32_t code) const;
};

// OrdinalScale — shared base for band/point. Owns an OrdinalDomain + padding and
// the streaming bind-to-Cat-dictionary auto-domain. The numeric Scale::map()
// contract takes a category CODE (cast through double) and returns the position;
// ordinal-specific helpers (bandwidth/center/step) are exposed directly.
class OrdinalScale : public Scale {
 public:
  const OrdinalDomain& ordinalDomain() const { return ordinal_; }

  // Padding in [0,1] as a FRACTION of a step. paddingInner carves the gap BETWEEN
  // bands; paddingOuter the gap before the first / after the last. (D3 semantics.)
  void setPadding(double p) { paddingInner_ = paddingOuter_ = p; }
  void setPaddingInner(double p) { paddingInner_ = p; }
  void setPaddingOuter(double p) { paddingOuter_ = p; }
  double paddingInner() const { return paddingInner_; }
  double paddingOuter() const { return paddingOuter_; }

  // Seed the domain directly from category codes (in order). Replaces the domain.
  void setCategories(const std::vector<std::uint32_t>& codes);

  // The distance between successive band starts (point positions). 0 for empty.
  double step() const;

  // Width of one band. For a point scale this is 0 (points have no width).
  virtual double bandwidth() const = 0;

  // Center position of the band/point for category `code` (or the parked range
  // midpoint if the code is not in the domain).
  double center(std::uint32_t code) const;

  // Scale::map — takes a category code (cast through double) → its START position
  // (band start for band; the point position for point, since bandwidth is 0).
  double map(double code) const override;

  // Scale::invert — range value → the category code whose band/cell contains it
  // (cast through double). Returns -1.0 (as double) if outside or empty.
  double invert(double rangeValue) const override;

  // Category labels as ticks (ENC-599): one tick per category at its center,
  // labeled from the Cat dictionary. `targetCount` is ignored (every category is
  // a tick). dict may be null (then labels are the numeric codes).
  std::vector<ScaleTick> ticksWithDict(const CatDictionary* dict) const;
  std::vector<ScaleTick> ticks(int targetCount = 0) const override {
    (void)targetCount;
    return ticksWithDict(nullptr);
  }

  // ----- streaming auto-domain over a Cat column's dictionary ----------------

  // Bind a Cat column; its dictionary's codes become the (growing) domain.
  void bindColumn(Id tableId, std::string columnName);
  bool hasBoundColumn() const { return bound_; }

  // Fold the bound Cat column's dictionary NEW codes (O(Δ)) into the ordinal
  // domain, in code order (dense codes are assigned in arrival order, so this
  // preserves arrival order). Returns false if no column is bound or it is not a
  // Cat column. A 0-code (empty) dictionary is a valid no-op that returns true.
  bool updateDomain(const TableStore& tables);

 protected:
  // Position of the START of band/point at dense ordinal `i` (0-based).
  double startAt(std::size_t i) const;
  // The usable [r0,r1] extent and its sign, with outer padding already removed.
  void layout(double& start, double& span, double& stepOut, double& bandOut) const;

  // Effective inner padding used by layout(). A BAND uses paddingInner_ (gap
  // between bands); a POINT uses 1.0 — the identity point==band-with-zero-width,
  // which makes step = extent/(n-1) for paddingOuter 0 (points at r0..r1). This
  // is the only thing that distinguishes the two scales' layout math.
  virtual double effectiveInnerPadding() const { return paddingInner_; }

  OrdinalDomain ordinal_;
  double paddingInner_{0.0};
  double paddingOuter_{0.0};
  Id boundTable_{kInvalidId};
  std::string boundColumn_;
  bool bound_{false};
  std::uint32_t consumedCats_{0};  // dict high-water for O(Δ) growth
};

// BandScale — category → a band [start, start+bandwidth] with inner/outer pad.
class BandScale : public OrdinalScale {
 public:
  ScaleType type() const override { return ScaleType::Band; }
  double bandwidth() const override;
};

// PointScale — category → a single position (bandwidth 0); points sit at the band
// centers of the equivalent zero-width-band layout.
class PointScale : public OrdinalScale {
 public:
  ScaleType type() const override { return ScaleType::Point; }
  double bandwidth() const override { return 0.0; }

 protected:
  double effectiveInnerPadding() const override { return 1.0; }
};

}  // namespace dc
