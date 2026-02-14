#pragma once
#include <string>

namespace dc {

// Viewport state for serialization.
struct ViewportState {
  double xMin{0}, xMax{100}, yMin{0}, yMax{100};
};

// Serializable chart configuration.
struct ChartState {
  std::string version{"1.0"};
  ViewportState viewport;
  std::string drawingsJSON;  // DrawingStore::toJSON() output
  std::string themeName;     // "Dark" or "Light" or custom name

  // Optional metadata
  std::string symbol;        // e.g. "BTCUSD"
  std::string timeframe;     // e.g. "1H"
};

// Serialize ChartState to a JSON string.
std::string serializeChartState(const ChartState& state);

// Deserialize a JSON string into ChartState. Returns false on error.
bool deserializeChartState(const std::string& json, ChartState& out);

} // namespace dc
