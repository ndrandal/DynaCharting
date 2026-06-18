// ENC-633 (D3) — BrushGesture: a drag becomes an interval/rect selection signal.
//
// The input primitive for brushing: begin/update/end track a drag rectangle in
// DATA space and write it into a SignalStore as either an IntervalSelection (a 1-D
// brush over a field, the common cross-filter case) or a BrushRect (2-D). Because
// it writes through SignalStore::set/define, every drag step marks the signal
// dirty, so a bound selectionFilter (ENC-626) re-renders LIVE as the user brushes
// — and, through a ViewSession (ENC-638), brushing one view filters the others.
//
// Distinct from BoxSelection (dc/selection), which hit-tests DrawItems into
// SelectionKeys; this produces a value/region SIGNAL the transform DAG consumes.
// Pure `dc` (no GPU): the host feeds data-space coordinates (the pan/zoom inverse
// of pointer pixels).
#pragma once

#include "dc/ids/Id.hpp"
#include "dc/interaction/SignalStore.hpp"

namespace dc {

class BrushGesture {
 public:
  // What the drag resolves to:
  //   XInterval — IntervalSelection over [minX,maxX] (a vertical brush band).
  //   YInterval — IntervalSelection over [minY,maxY].
  //   Rect      — a 2-D BrushRect.
  enum class Mode { XInterval, YInterval, Rect };

  BrushGesture() = default;
  BrushGesture(SignalStore* signals, Id signalId)
      : signals_(signals), signalId_(signalId) {}

  void setSignals(SignalStore* signals) { signals_ = signals; }
  void setSignal(Id signalId) { signalId_ = signalId; }
  void setMode(Mode mode) { mode_ = mode; }
  Mode mode() const { return mode_; }
  // The field id attached to a produced IntervalSelection (so two views linking on
  // the same field cross-filter; a non-invalid id also marks the interval active).
  void setField(Id fieldId) { field_ = fieldId; }
  // Write the signal on every update() (live brushing, default), or only on end().
  void setLiveUpdate(bool live) { live_ = live; }

  // Start a drag at a data-space point.
  void begin(double dataX, double dataY);
  // Extend the drag; writes the signal if live updates are on. Returns true if the
  // signal was mutated.
  bool update(double dataX, double dataY);
  // Finish the drag; writes the final signal. Returns true if the signal was
  // mutated.
  bool end(double dataX, double dataY);
  // Abort: clear the brush signal (selection-empty) and end the drag.
  void cancel();

  bool isActive() const { return active_; }
  double minX() const { return x0_ < x1_ ? x0_ : x1_; }
  double maxX() const { return x0_ > x1_ ? x0_ : x1_; }
  double minY() const { return y0_ < y1_ ? y0_ : y1_; }
  double maxY() const { return y0_ > y1_ ? y0_ : y1_; }

 private:
  // Write the current rect into the signal per mode (define if absent, else set).
  bool writeSignal();

  SignalStore* signals_{nullptr};
  Id signalId_{kInvalidId};
  Id field_{kInvalidId};
  Mode mode_{Mode::XInterval};
  bool live_{true};
  bool active_{false};
  double x0_{0}, y0_{0}, x1_{0}, y1_{0};
};

}  // namespace dc
