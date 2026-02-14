#include "dc/recipe/DrawingRecipe.hpp"
#include <cstdio>
#include <string>

namespace dc {

DrawingRecipe::DrawingRecipe(Id idBase, const DrawingRecipeConfig& config)
  : Recipe(idBase), config_(config) {}

RecipeBuildResult DrawingRecipe::build() const {
  RecipeBuildResult result;
  auto idStr = [](Id id) { return std::to_string(id); };

  result.createCommands.push_back(
    R"({"cmd":"createBuffer","id":)" + idStr(bufferId()) + R"(,"byteLength":0})");
  result.createCommands.push_back(
    R"({"cmd":"createGeometry","id":)" + idStr(geometryId()) +
    R"(,"vertexBufferId":)" + idStr(bufferId()) +
    R"(,"format":"rect4","vertexCount":1})");
  result.createCommands.push_back(
    R"({"cmd":"createDrawItem","id":)" + idStr(drawItemId()) +
    R"(,"layerId":)" + idStr(config_.layerId) +
    R"(,"name":")" + config_.name + R"("})");
  result.createCommands.push_back(
    R"({"cmd":"bindDrawItem","drawItemId":)" + idStr(drawItemId()) +
    R"(,"pipeline":"lineAA@1","geometryId":)" + idStr(geometryId()) + "}");

  // Set default style
  {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
      R"({"cmd":"setDrawItemStyle","drawItemId":%s,"r":%.9g,"g":%.9g,"b":%.9g,"a":%.9g,"lineWidth":%.9g})",
      idStr(drawItemId()).c_str(),
      static_cast<double>(config_.defaultColor[0]),
      static_cast<double>(config_.defaultColor[1]),
      static_cast<double>(config_.defaultColor[2]),
      static_cast<double>(config_.defaultColor[3]),
      static_cast<double>(config_.defaultLineWidth));
    result.createCommands.push_back(buf);
  }

  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(drawItemId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(geometryId()) + "}");
  result.disposeCommands.push_back(R"({"cmd":"delete","id":)" + idStr(bufferId()) + "}");

  return result;
}

DrawingRecipe::DrawingData DrawingRecipe::computeDrawings(
    const DrawingStore& store,
    double dataXMin, double dataXMax,
    double dataYMin, double dataYMax) const {
  DrawingData data;

  for (const auto& d : store.drawings()) {
    switch (d.type) {
      case DrawingType::Trendline:
        // One line segment from (x0,y0) to (x1,y1)
        data.lineSegments.push_back(static_cast<float>(d.x0));
        data.lineSegments.push_back(static_cast<float>(d.y0));
        data.lineSegments.push_back(static_cast<float>(d.x1));
        data.lineSegments.push_back(static_cast<float>(d.y1));
        data.segmentCount++;
        break;

      case DrawingType::HorizontalLevel:
        // Horizontal line across full X range
        data.lineSegments.push_back(static_cast<float>(dataXMin));
        data.lineSegments.push_back(static_cast<float>(d.y0));
        data.lineSegments.push_back(static_cast<float>(dataXMax));
        data.lineSegments.push_back(static_cast<float>(d.y0));
        data.segmentCount++;
        break;

      case DrawingType::VerticalLine:
        // D21.3: Vertical line from (x0, dataYMin) to (x0, dataYMax)
        data.lineSegments.push_back(static_cast<float>(d.x0));
        data.lineSegments.push_back(static_cast<float>(dataYMin));
        data.lineSegments.push_back(static_cast<float>(d.x0));
        data.lineSegments.push_back(static_cast<float>(dataYMax));
        data.segmentCount++;
        break;

      case DrawingType::Rectangle: {
        // D21.3: 4 line segments forming the border (top, bottom, left, right)
        auto fx0 = static_cast<float>(d.x0);
        auto fy0 = static_cast<float>(d.y0);
        auto fx1 = static_cast<float>(d.x1);
        auto fy1 = static_cast<float>(d.y1);

        // Top: (x0,y0) -> (x1,y0)
        data.lineSegments.push_back(fx0); data.lineSegments.push_back(fy0);
        data.lineSegments.push_back(fx1); data.lineSegments.push_back(fy0);
        data.segmentCount++;

        // Bottom: (x0,y1) -> (x1,y1)
        data.lineSegments.push_back(fx0); data.lineSegments.push_back(fy1);
        data.lineSegments.push_back(fx1); data.lineSegments.push_back(fy1);
        data.segmentCount++;

        // Left: (x0,y0) -> (x0,y1)
        data.lineSegments.push_back(fx0); data.lineSegments.push_back(fy0);
        data.lineSegments.push_back(fx0); data.lineSegments.push_back(fy1);
        data.segmentCount++;

        // Right: (x1,y0) -> (x1,y1)
        data.lineSegments.push_back(fx1); data.lineSegments.push_back(fy0);
        data.lineSegments.push_back(fx1); data.lineSegments.push_back(fy1);
        data.segmentCount++;
        break;
      }

      case DrawingType::FibRetracement: {
        // D21.3: Horizontal lines at standard Fibonacci levels between y0 and y1.
        // Lines extend from x0 to x1 (scoped to drawing range).
        static constexpr double fibLevels[] = {0.0, 0.236, 0.382, 0.5, 0.618, 1.0};
        auto fx0 = static_cast<float>(d.x0);
        auto fx1 = static_cast<float>(d.x1);
        double yRange = d.y1 - d.y0;

        for (double level : fibLevels) {
          float y = static_cast<float>(d.y0 + yRange * level);
          data.lineSegments.push_back(fx0); data.lineSegments.push_back(y);
          data.lineSegments.push_back(fx1); data.lineSegments.push_back(y);
          data.segmentCount++;
        }
        break;
      }
    }
  }

  return data;
}

} // namespace dc
