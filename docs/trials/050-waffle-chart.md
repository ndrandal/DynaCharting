# Trial 050: Waffle Chart

**Date:** 2026-03-12
**Goal:** Waffle chart (square pie chart) with 100 colored cells in a 10×10 grid representing 5 budget categories. Tests grid-based percentage visualization, left-to-right bottom-to-top fill ordering, category grouping into 5 instancedRect@1 DrawItems, and rounded corners on a square 700×700 viewport.
**Outcome:** All 100 cells at correct grid positions. All 5 category counts correct (35+22+18+15+10=100). Fill order correct. No duplicate or missing cells. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**100 cells across 5 categories (5 instancedRect@1 DrawItems, rect4, cornerRadius 3px):**

| Category | Cells | Cell Range | Color | Alpha |
|----------|-------|-----------|-------|-------|
| Housing | 35 | 0–34 | #3b82f6 (blue) | 0.9 |
| Food | 22 | 35–56 | #10b981 (emerald) | 0.9 |
| Transport | 18 | 57–74 | #f59e0b (amber) | 0.9 |
| Entertainment | 15 | 75–89 | #ec4899 (pink) | 0.9 |
| Savings | 10 | 90–99 | #8b5cf6 (violet) | 0.9 |

Fill order: left-to-right, bottom-to-top. Cell i → column i%10, row i÷10. Each cell spans [col+0.05, row+0.05] to [col+0.95, row+0.95] (0.9×0.9 with 0.1 gaps).

**Border (1 lineAA@1 DrawItem, rect4, 4 instances):**
Rectangle [0,0]→[10,10]. White, alpha 0.15, lineWidth 1.

Data space: X=Y=[0, 10]. Transform 50: sx=sy=0.19, tx=ty=−0.95.

Layers: Border (10) → Cells (11).

Total: 22 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 100 cells at correct grid positions.** Every cell's [xMin, yMin, xMax, yMax] verified against expected [col+0.05, row+0.05, col+0.95, row+0.95]. 100/100 correct.

- **All 5 category counts correct.** Housing: 35, Food: 22, Transport: 18, Entertainment: 15, Savings: 10. Sum = 100 = 100%.

- **No duplicate cells.** All 100 cell positions are unique across all 5 category buffers.

- **Fill order is correct (left-to-right, bottom-to-top).** Housing fills the bottom 3 full rows and 5 cells of row 3 (cells 0–34). Food continues from cell 35 (col 5, row 3) through cell 56. Category boundaries fall at the expected grid positions.

- **Category boundary transitions visible.** In the PNG, the transition from blue→emerald happens mid-row (at column 5, row 3), and other transitions also create partial-row boundaries. This is the characteristic waffle chart appearance.

- **Square viewport ensures square cells.** 700×700 with symmetric transform (sx=sy=0.19) means all cells are perfectly square.

- **Transform math is exact.** sx=sy=1.9/10=0.19 maps [0,10] to clip[−0.95, 0.95] in both axes.

- **Corner radius adds visual polish.** 3px cornerRadius creates subtle rounded squares.

- **Cell gaps create clear grid pattern.** 0.1 data-unit gaps (0.9 cell in 1.0 spacing) produce visible dark lines between cells.

- **5 distinct colors with sufficient contrast.** Blue, emerald, amber, pink, violet are easily distinguishable in the grid.

- **Border frames the grid.** 4-segment rectangle at [0,0]→[10,10] provides a subtle boundary.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 6/6 geometries verified.

- **All 22 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Waffle charts map percentages to grid cells.** Each cell = 1%. Categories fill consecutive cells in a fixed order (left-to-right, bottom-to-top). The partial-row boundaries between categories create the visual interest.

2. **Square viewports eliminate aspect concerns.** With 700×700 and sx=sy, all cells are naturally square without any correction.

3. **Grouping cells by category requires only 5 DrawItems for 100 cells.** Each category's cells share the same color, so they go into one instancedRect@1 buffer. This is far more efficient than 100 individual DrawItems.

4. **Cell index arithmetic is simple.** Cell i → col = i%10, row = i÷10. This linear fill order maps naturally to the grid.

5. **Waffle charts communicate proportions more intuitively than pie charts for many audiences.** The discrete squares make it easy to count or estimate percentages (each cell = 1%), while the spatial layout shows relative sizes clearly.
