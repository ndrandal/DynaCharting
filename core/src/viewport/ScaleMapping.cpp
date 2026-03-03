#include "dc/viewport/ScaleMapping.hpp"
#include <cmath>

namespace dc {

// Helper: linear interpolation t in [0,1] -> [a, b]
static double lerp(double a, double b, double t) {
  return a + t * (b - a);
}

// Helper: inverse lerp value in [a, b] -> t in [0,1]
static double invLerp(double a, double b, double value) {
  if (b == a) return 0.0;
  return (value - a) / (b - a);
}

double ScaleMapping::toScreen(double dataValue,
                              double screenMin, double screenMax,
                              double dataMin, double dataMax) const {
  switch (mode_) {
    case ScaleMode::Linear: {
      double t = invLerp(dataMin, dataMax, dataValue);
      return lerp(screenMin, screenMax, t);
    }

    case ScaleMode::Logarithmic: {
      // Guard against non-positive values in log domain.
      // If dataMin or dataMax are <= 0, fall back to linear.
      if (dataMin <= 0.0 || dataMax <= 0.0) {
        double t = invLerp(dataMin, dataMax, dataValue);
        return lerp(screenMin, screenMax, t);
      }
      // Clamp dataValue to positive
      double safeVal = dataValue > 0.0 ? dataValue : dataMin;
      double logMin = std::log(dataMin);
      double logMax = std::log(dataMax);
      if (logMax == logMin) return screenMin;
      double t = (std::log(safeVal) - logMin) / (logMax - logMin);
      return lerp(screenMin, screenMax, t);
    }

    case ScaleMode::Percentage: {
      // Map dataValue as percentage change from referencePrice_.
      // referencePrice_ -> 0%, dataMin/dataMax define visible range.
      if (referencePrice_ == 0.0) {
        // Degenerate: fall back to linear
        double t = invLerp(dataMin, dataMax, dataValue);
        return lerp(screenMin, screenMax, t);
      }
      double pctValue = (dataValue - referencePrice_) / referencePrice_ * 100.0;
      double pctMin = (dataMin - referencePrice_) / referencePrice_ * 100.0;
      double pctMax = (dataMax - referencePrice_) / referencePrice_ * 100.0;
      double t = invLerp(pctMin, pctMax, pctValue);
      return lerp(screenMin, screenMax, t);
    }

    case ScaleMode::Indexed: {
      // Normalize so referencePrice_ = 100. Then map linearly.
      if (referencePrice_ == 0.0) {
        double t = invLerp(dataMin, dataMax, dataValue);
        return lerp(screenMin, screenMax, t);
      }
      double idxValue = dataValue / referencePrice_ * 100.0;
      double idxMin = dataMin / referencePrice_ * 100.0;
      double idxMax = dataMax / referencePrice_ * 100.0;
      double t = invLerp(idxMin, idxMax, idxValue);
      return lerp(screenMin, screenMax, t);
    }
  }

  // Unreachable, but satisfy compiler
  return screenMin;
}

double ScaleMapping::toData(double screenValue,
                            double screenMin, double screenMax,
                            double dataMin, double dataMax) const {
  switch (mode_) {
    case ScaleMode::Linear: {
      double t = invLerp(screenMin, screenMax, screenValue);
      return lerp(dataMin, dataMax, t);
    }

    case ScaleMode::Logarithmic: {
      if (dataMin <= 0.0 || dataMax <= 0.0) {
        double t = invLerp(screenMin, screenMax, screenValue);
        return lerp(dataMin, dataMax, t);
      }
      double logMin = std::log(dataMin);
      double logMax = std::log(dataMax);
      if (logMax == logMin) return dataMin;
      double t = invLerp(screenMin, screenMax, screenValue);
      double logVal = lerp(logMin, logMax, t);
      return std::exp(logVal);
    }

    case ScaleMode::Percentage: {
      if (referencePrice_ == 0.0) {
        double t = invLerp(screenMin, screenMax, screenValue);
        return lerp(dataMin, dataMax, t);
      }
      double pctMin = (dataMin - referencePrice_) / referencePrice_ * 100.0;
      double pctMax = (dataMax - referencePrice_) / referencePrice_ * 100.0;
      double t = invLerp(screenMin, screenMax, screenValue);
      double pctVal = lerp(pctMin, pctMax, t);
      // pctVal = (dataValue - ref) / ref * 100  =>  dataValue = ref * (1 + pctVal/100)
      return referencePrice_ * (1.0 + pctVal / 100.0);
    }

    case ScaleMode::Indexed: {
      if (referencePrice_ == 0.0) {
        double t = invLerp(screenMin, screenMax, screenValue);
        return lerp(dataMin, dataMax, t);
      }
      double idxMin = dataMin / referencePrice_ * 100.0;
      double idxMax = dataMax / referencePrice_ * 100.0;
      double t = invLerp(screenMin, screenMax, screenValue);
      double idxVal = lerp(idxMin, idxMax, t);
      // idxVal = dataValue / ref * 100  =>  dataValue = idxVal * ref / 100
      return idxVal * referencePrice_ / 100.0;
    }
  }

  return dataMin;
}

} // namespace dc
