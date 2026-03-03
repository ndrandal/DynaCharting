#pragma once

namespace dc {

// D63: Non-linear scale mapping modes.
// Linear is the default (standard affine).
// Logarithmic maps data using natural log for price-chart log scale.
// Percentage maps data as % change from a reference price.
// Indexed normalizes so that referencePrice = 100.
enum class ScaleMode {
  Linear,
  Logarithmic,
  Percentage,
  Indexed
};

class ScaleMapping {
public:
  void setMode(ScaleMode m) { mode_ = m; }
  ScaleMode mode() const { return mode_; }

  void setReferencePrice(double ref) { referencePrice_ = ref; }
  double referencePrice() const { return referencePrice_; }

  // Map a data-space value to screen-space.
  // screenMin/screenMax define the output range (e.g. pixel coordinates or clip coordinates).
  // dataMin/dataMax define the visible data range.
  double toScreen(double dataValue,
                  double screenMin, double screenMax,
                  double dataMin, double dataMax) const;

  // Inverse: map a screen-space value back to data-space.
  double toData(double screenValue,
                double screenMin, double screenMax,
                double dataMin, double dataMax) const;

private:
  ScaleMode mode_{ScaleMode::Linear};
  double referencePrice_{1.0};
};

} // namespace dc
