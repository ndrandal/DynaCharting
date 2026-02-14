#include "dc/drawing/DrawingInteraction.hpp"

namespace dc {

void DrawingInteraction::beginTrendline() {
  mode_ = DrawingMode::PlacingTrendlineFirst;
}

void DrawingInteraction::beginHorizontalLevel() {
  mode_ = DrawingMode::PlacingHorizontalLevel;
}

void DrawingInteraction::beginVerticalLine() {
  mode_ = DrawingMode::PlacingVerticalLine;
}

void DrawingInteraction::beginRectangle() {
  mode_ = DrawingMode::PlacingRectangleFirst;
}

void DrawingInteraction::beginFibRetracement() {
  mode_ = DrawingMode::PlacingFibFirst;
}

void DrawingInteraction::cancel() {
  mode_ = DrawingMode::Idle;
}

std::uint32_t DrawingInteraction::onClick(double dataX, double dataY,
                                           DrawingStore& store) {
  previewX_ = dataX;
  previewY_ = dataY;

  switch (mode_) {
    case DrawingMode::PlacingTrendlineFirst:
      firstX_ = dataX;
      firstY_ = dataY;
      mode_ = DrawingMode::PlacingTrendlineSecond;
      return 0;

    case DrawingMode::PlacingTrendlineSecond: {
      auto id = store.addTrendline(firstX_, firstY_, dataX, dataY);
      mode_ = DrawingMode::Idle;
      return id;
    }

    case DrawingMode::PlacingHorizontalLevel: {
      auto id = store.addHorizontalLevel(dataY);
      mode_ = DrawingMode::Idle;
      return id;
    }

    case DrawingMode::PlacingVerticalLine: {
      auto id = store.addVerticalLine(dataX);
      mode_ = DrawingMode::Idle;
      return id;
    }

    case DrawingMode::PlacingRectangleFirst:
      firstX_ = dataX;
      firstY_ = dataY;
      mode_ = DrawingMode::PlacingRectangleSecond;
      return 0;

    case DrawingMode::PlacingRectangleSecond: {
      auto id = store.addRectangle(firstX_, firstY_, dataX, dataY);
      mode_ = DrawingMode::Idle;
      return id;
    }

    case DrawingMode::PlacingFibFirst:
      firstX_ = dataX;
      firstY_ = dataY;
      mode_ = DrawingMode::PlacingFibSecond;
      return 0;

    case DrawingMode::PlacingFibSecond: {
      auto id = store.addFibRetracement(firstX_, firstY_, dataX, dataY);
      mode_ = DrawingMode::Idle;
      return id;
    }

    case DrawingMode::Idle:
    default:
      return 0;
  }
}

} // namespace dc
