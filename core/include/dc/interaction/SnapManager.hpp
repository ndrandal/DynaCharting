#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dc {

enum class SnapMode : std::uint8_t {
  None  = 0,
  Magnet = 1,   // snap to nearest data point
  Grid   = 2,   // snap to grid intervals
  Both   = 3    // snap to nearest of data or grid
};

struct SnapTarget {
  double x{0}, y{0};
  std::uint32_t sourceId{0};   // which data source this came from (buffer ID, drawing ID, etc.)
  std::uint8_t kind{0};        // 0=OHLC, 1=drawing endpoint, 2=grid intersection
};

struct SnapResult {
  double x{0}, y{0};        // snapped position
  bool snapped{false};        // true if position was modified
  double distance{0};         // pixel distance to snap target
  std::uint32_t targetSourceId{0};
};

class SnapManager {
public:
  void setMode(SnapMode mode);
  SnapMode mode() const;

  // Set magnet radius in pixels
  void setMagnetRadius(double radius);
  double magnetRadius() const;

  // Set grid intervals (data-space units)
  void setGridInterval(double intervalX, double intervalY);
  double gridIntervalX() const;
  double gridIntervalY() const;

  // Register OHLC snap targets for a range of candles.
  // Each candle has x, open, high, low, close (5 doubles per candle).
  // stride = number of doubles between consecutive candles (default 5).
  void setOHLCTargets(const double* data, std::size_t candleCount,
                      std::size_t stride = 5);

  // Register custom snap targets (drawing endpoints, etc.)
  void addTarget(SnapTarget target);
  void clearTargets();
  void clearOHLCTargets();

  // Snap a data-space coordinate.
  // pixelPerDataX/Y needed to convert magnet radius from pixels to data units.
  SnapResult snap(double dataX, double dataY,
                  double pixelPerDataX, double pixelPerDataY) const;

private:
  SnapResult snapToTargets(double dataX, double dataY,
                           double pixelPerDataX, double pixelPerDataY) const;
  SnapResult snapToGrid(double dataX, double dataY,
                        double pixelPerDataX, double pixelPerDataY) const;

  SnapMode mode_{SnapMode::None};
  double magnetRadius_{10.0};  // pixels
  double gridIntervalX_{0}, gridIntervalY_{0};
  std::vector<SnapTarget> ohlcTargets_;
  std::vector<SnapTarget> customTargets_;
};

} // namespace dc
