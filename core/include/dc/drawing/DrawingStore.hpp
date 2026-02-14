#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace dc {

// D16.1: Drawing model — user-created annotations in data space.
enum class DrawingType : std::uint8_t {
  Trendline = 1,       // two-point line
  HorizontalLevel = 2, // single price level
  VerticalLine = 3,    // D21.1: single x-coordinate vertical line
  Rectangle = 4,       // D21.1: rectangle zone (x0,y0 -> x1,y1)
  FibRetracement = 5   // D21.1: fibonacci retracement (y-range from x0,y0 -> x1,y1)
};

struct Drawing {
  std::uint32_t id{0};
  DrawingType type{DrawingType::Trendline};

  // Data-space coordinates
  double x0{0}, y0{0};   // first point (or price for HorizontalLevel)
  double x1{0}, y1{0};   // second point (unused for HorizontalLevel)

  float color[4] = {1.0f, 1.0f, 0.0f, 1.0f}; // yellow default
  float lineWidth{2.0f};
};

class DrawingStore {
public:
  std::uint32_t addTrendline(double x0, double y0, double x1, double y1);
  std::uint32_t addHorizontalLevel(double price);
  std::uint32_t addVerticalLine(double x);          // D21.1
  std::uint32_t addRectangle(double x0, double y0, double x1, double y1); // D21.1
  std::uint32_t addFibRetracement(double x0, double y0, double x1, double y1); // D21.1

  void setColor(std::uint32_t id, float r, float g, float b, float a);
  void setLineWidth(std::uint32_t id, float width);
  void remove(std::uint32_t id);
  void clear();

  const Drawing* get(std::uint32_t id) const;
  const std::vector<Drawing>& drawings() const { return drawings_; }
  std::size_t count() const { return drawings_.size(); }

  // D19.2: Serialization
  std::string toJSON() const;
  bool loadJSON(const std::string& json);

private:
  std::vector<Drawing> drawings_;
  std::uint32_t nextId_{1};
};

} // namespace dc
