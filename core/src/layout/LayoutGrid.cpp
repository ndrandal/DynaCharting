#include "dc/layout/LayoutGrid.hpp"

#include <algorithm>
#include <cmath>

namespace dc {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void LayoutGrid::setLayout(int rows, int cols) {
  if (rows < 1) rows = 1;
  if (cols < 1) cols = 1;
  rows_ = rows;
  cols_ = cols;

  // Equal distribution
  rowHeights_.assign(static_cast<std::size_t>(rows), 1.0 / rows);
  colWidths_.assign(static_cast<std::size_t>(cols), 1.0 / cols);

  rebuildCells();
  rebuildDividers();
}

int LayoutGrid::rows() const { return rows_; }
int LayoutGrid::cols() const { return cols_; }

void LayoutGrid::setSingle()    { setLayout(1, 1); }
void LayoutGrid::setDual()      { setLayout(1, 2); }
void LayoutGrid::setTriple()    { setLayout(1, 3); }
void LayoutGrid::setQuad()      { setLayout(2, 2); }
void LayoutGrid::setLayout2x3() { setLayout(2, 3); }
void LayoutGrid::setLayout3x3() { setLayout(3, 3); }

const GridCell* LayoutGrid::getCell(int row, int col) const {
  for (auto& c : cells_) {
    if (c.row == row && c.col == col) return &c;
  }
  return nullptr;
}

const GridCell* LayoutGrid::getCellById(std::uint32_t id) const {
  for (auto& c : cells_) {
    if (c.id == id) return &c;
  }
  return nullptr;
}

const std::vector<GridCell>& LayoutGrid::cells() const { return cells_; }

void LayoutGrid::setCellSpan(int row, int col, int rowSpan, int colSpan) {
  if (rowSpan < 1) rowSpan = 1;
  if (colSpan < 1) colSpan = 1;

  // Find the target cell
  GridCell* target = nullptr;
  for (auto& c : cells_) {
    if (c.row == row && c.col == col) {
      target = &c;
      break;
    }
  }
  if (!target) return;

  // Clamp spans to grid bounds (minimum 1)
  if (row + rowSpan > rows_) rowSpan = std::max(1, rows_ - row);
  if (col + colSpan > cols_) colSpan = std::max(1, cols_ - col);

  target->rowSpan = rowSpan;
  target->colSpan = colSpan;

  // Remove cells that are now covered by the span
  cells_.erase(
    std::remove_if(cells_.begin(), cells_.end(), [&](const GridCell& c) {
      if (&c == target) return false;
      return c.row >= row && c.row < row + rowSpan &&
             c.col >= col && c.col < col + colSpan;
    }),
    cells_.end());
}

void LayoutGrid::setChartId(int row, int col, std::uint32_t chartId) {
  for (auto& c : cells_) {
    if (c.row == row && c.col == col) {
      c.chartId = chartId;
      return;
    }
  }
}

int LayoutGrid::hitTestDivider(double normalizedX, double normalizedY,
                               double tolerance) const {
  for (std::size_t i = 0; i < dividers_.size(); ++i) {
    auto& d = dividers_[i];
    if (d.horizontal) {
      // Horizontal divider: compare Y
      if (std::fabs(normalizedY - d.position) <= tolerance) {
        return static_cast<int>(i);
      }
    } else {
      // Vertical divider: compare X
      if (std::fabs(normalizedX - d.position) <= tolerance) {
        return static_cast<int>(i);
      }
    }
  }
  return -1;
}

void LayoutGrid::beginDividerDrag(int dividerIndex) {
  if (dividerIndex < 0 ||
      dividerIndex >= static_cast<int>(dividers_.size())) return;
  dividers_[static_cast<std::size_t>(dividerIndex)].dragging = true;
}

void LayoutGrid::updateDividerDrag(int dividerIndex, double normalizedPos) {
  if (dividerIndex < 0 ||
      dividerIndex >= static_cast<int>(dividers_.size())) return;

  auto& d = dividers_[static_cast<std::size_t>(dividerIndex)];

  // Minimum fraction to prevent collapsing a row/col
  constexpr double kMinFrac = 0.05;

  if (d.horizontal) {
    // Horizontal divider between row d.index-1 and d.index
    // d.index is 1-based boundary (boundary between row index-1 and row index)
    int above = d.index - 1;  // row above the divider
    int below = d.index;      // row below the divider

    if (above < 0 || below >= rows_) return;

    // Compute cumulative position of the boundary before above row
    double topEdge = 0.0;
    for (int r = 0; r < above; ++r)
      topEdge += rowHeights_[static_cast<std::size_t>(r)];

    // And after below row
    double bottomEdge = topEdge +
      rowHeights_[static_cast<std::size_t>(above)] +
      rowHeights_[static_cast<std::size_t>(below)];

    // Clamp normalizedPos within [topEdge + kMinFrac, bottomEdge - kMinFrac]
    double clamped = std::max(topEdge + kMinFrac,
                              std::min(normalizedPos, bottomEdge - kMinFrac));

    rowHeights_[static_cast<std::size_t>(above)] = clamped - topEdge;
    rowHeights_[static_cast<std::size_t>(below)] = bottomEdge - clamped;

    d.position = clamped;
  } else {
    // Vertical divider between col d.index-1 and d.index
    int left  = d.index - 1;
    int right = d.index;

    if (left < 0 || right >= cols_) return;

    double leftEdge = 0.0;
    for (int c = 0; c < left; ++c)
      leftEdge += colWidths_[static_cast<std::size_t>(c)];

    double rightEdge = leftEdge +
      colWidths_[static_cast<std::size_t>(left)] +
      colWidths_[static_cast<std::size_t>(right)];

    double clamped = std::max(leftEdge + kMinFrac,
                              std::min(normalizedPos, rightEdge - kMinFrac));

    colWidths_[static_cast<std::size_t>(left)] = clamped - leftEdge;
    colWidths_[static_cast<std::size_t>(right)] = rightEdge - clamped;

    d.position = clamped;
  }
}

void LayoutGrid::endDividerDrag(int dividerIndex) {
  if (dividerIndex < 0 ||
      dividerIndex >= static_cast<int>(dividers_.size())) return;
  dividers_[static_cast<std::size_t>(dividerIndex)].dragging = false;
}

void LayoutGrid::setRowHeights(const std::vector<double>& heights) {
  if (static_cast<int>(heights.size()) != rows_) return;
  rowHeights_ = heights;
  rebuildDividers();
}

void LayoutGrid::setColWidths(const std::vector<double>& widths) {
  if (static_cast<int>(widths.size()) != cols_) return;
  colWidths_ = widths;
  rebuildDividers();
}

void LayoutGrid::setGap(double gap) { gap_ = gap; }
double LayoutGrid::gap() const { return gap_; }

void LayoutGrid::recompute(double totalWidth, double totalHeight) {
  // Gap in normalized coordinates
  double gapX = (totalWidth  > 0) ? (gap_ / totalWidth)  : 0.0;
  double gapY = (totalHeight > 0) ? (gap_ / totalHeight) : 0.0;

  // Total gap space consumed
  double totalGapX = gapX * std::max(0, cols_ - 1);
  double totalGapY = gapY * std::max(0, rows_ - 1);

  // Available space after gaps
  double availW = 1.0 - totalGapX;
  double availH = 1.0 - totalGapY;
  if (availW < 0) availW = 0;
  if (availH < 0) availH = 0;

  // Precompute cumulative row positions (top edge of each row)
  std::vector<double> rowTop(static_cast<std::size_t>(rows_));
  {
    double y = 0.0;
    for (int r = 0; r < rows_; ++r) {
      rowTop[static_cast<std::size_t>(r)] = y;
      y += rowHeights_[static_cast<std::size_t>(r)] * availH;
      if (r < rows_ - 1) y += gapY;
    }
  }

  // Precompute cumulative column positions (left edge of each col)
  std::vector<double> colLeft(static_cast<std::size_t>(cols_));
  {
    double x = 0.0;
    for (int c = 0; c < cols_; ++c) {
      colLeft[static_cast<std::size_t>(c)] = x;
      x += colWidths_[static_cast<std::size_t>(c)] * availW;
      if (c < cols_ - 1) x += gapX;
    }
  }

  // Compute cell regions
  for (auto& cell : cells_) {
    int r = cell.row;
    int c = cell.col;
    int rs = cell.rowSpan;
    int cs = cell.colSpan;

    cell.x = colLeft[static_cast<std::size_t>(c)];
    cell.y = rowTop[static_cast<std::size_t>(r)];

    // Width = sum of col widths for the span + gaps between spanned cols
    double w = 0.0;
    for (int ci = c; ci < c + cs && ci < cols_; ++ci) {
      w += colWidths_[static_cast<std::size_t>(ci)] * availW;
      if (ci > c) w += gapX;  // add gap between spanned columns
    }
    cell.width = w;

    // Height = sum of row heights for the span + gaps between spanned rows
    double h = 0.0;
    for (int ri = r; ri < r + rs && ri < rows_; ++ri) {
      h += rowHeights_[static_cast<std::size_t>(ri)] * availH;
      if (ri > r) h += gapY;  // add gap between spanned rows
    }
    cell.height = h;
  }
}

const std::vector<GridDivider>& LayoutGrid::dividers() const {
  return dividers_;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void LayoutGrid::rebuildCells() {
  cells_.clear();
  nextCellId_ = 1;

  for (int r = 0; r < rows_; ++r) {
    for (int c = 0; c < cols_; ++c) {
      GridCell cell;
      cell.id = nextCellId_++;
      cell.row = r;
      cell.col = c;
      cell.rowSpan = 1;
      cell.colSpan = 1;
      cells_.push_back(cell);
    }
  }
}

void LayoutGrid::rebuildDividers() {
  dividers_.clear();

  // Horizontal dividers (between rows)
  {
    double cumY = 0.0;
    for (int r = 0; r < rows_ - 1; ++r) {
      cumY += rowHeights_[static_cast<std::size_t>(r)];
      GridDivider d;
      d.horizontal = true;
      d.index = r + 1;       // boundary index (1-based)
      d.position = cumY;
      d.dragging = false;
      dividers_.push_back(d);
    }
  }

  // Vertical dividers (between columns)
  {
    double cumX = 0.0;
    for (int c = 0; c < cols_ - 1; ++c) {
      cumX += colWidths_[static_cast<std::size_t>(c)];
      GridDivider d;
      d.horizontal = false;
      d.index = c + 1;
      d.position = cumX;
      d.dragging = false;
      dividers_.push_back(d);
    }
  }
}

} // namespace dc
