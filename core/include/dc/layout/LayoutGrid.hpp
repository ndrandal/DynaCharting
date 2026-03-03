#pragma once
#include <cstdint>
#include <vector>

namespace dc {

struct GridCell {
  std::uint32_t id{0};
  int row{0}, col{0};
  int rowSpan{1}, colSpan{1};

  // Computed region in normalized coordinates (0-1)
  double x{0}, y{0}, width{0}, height{0};

  // Optional: associated session/chart ID
  std::uint32_t chartId{0};
};

struct GridDivider {
  bool horizontal;   // true = horizontal divider (between rows)
  int index;         // which row/col boundary
  double position;   // normalized 0-1
  bool dragging{false};
};

class LayoutGrid {
public:
  // Set grid dimensions
  void setLayout(int rows, int cols);
  int rows() const;
  int cols() const;

  // Preset layouts
  void setSingle();       // 1x1
  void setDual();         // 1x2
  void setTriple();       // 1x3
  void setQuad();         // 2x2
  void setLayout2x3();    // 2x3
  void setLayout3x3();    // 3x3

  // Cell access
  const GridCell* getCell(int row, int col) const;
  const GridCell* getCellById(std::uint32_t id) const;
  const std::vector<GridCell>& cells() const;

  // Cell spanning (merge cells)
  void setCellSpan(int row, int col, int rowSpan, int colSpan);

  // Assign chart to cell
  void setChartId(int row, int col, std::uint32_t chartId);

  // Divider dragging (resize)
  // Returns divider index if hit, -1 if miss
  int hitTestDivider(double normalizedX, double normalizedY, double tolerance = 0.01) const;
  void beginDividerDrag(int dividerIndex);
  void updateDividerDrag(int dividerIndex, double normalizedPos);
  void endDividerDrag(int dividerIndex);

  // Custom row/column sizes (normalized, must sum to 1.0)
  void setRowHeights(const std::vector<double>& heights);
  void setColWidths(const std::vector<double>& widths);

  // Gap between cells (pixels)
  void setGap(double gap);
  double gap() const;

  // Recompute cell regions after layout change
  void recompute(double totalWidth, double totalHeight);

  const std::vector<GridDivider>& dividers() const;

private:
  int rows_{1}, cols_{1};
  std::vector<double> rowHeights_;
  std::vector<double> colWidths_;
  std::vector<GridCell> cells_;
  std::vector<GridDivider> dividers_;
  double gap_{2.0};
  std::uint32_t nextCellId_{1};

  void rebuildCells();
  void rebuildDividers();
};

} // namespace dc
