# Trial 031: Marimekko Chart (Mosaic)

**Date:** 2026-03-12
**Goal:** Variable-width stacked bar chart showing IT market share across 4 regions (column width ∝ market size) with 3 product segments per region (height ∝ segment share). The 12 rectangles must tile the chart area with zero gaps. Tests instancedRect@1 with non-uniform widths and stacked heights, and precise spatial tiling.
**Outcome:** All 12 rectangles are exact. Stacking boundaries are perfectly continuous in all 4 columns. Column boundaries align at X=36, 64, 88. The mosaic tiles the full [0,100]×[0,100] data area with zero gaps. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (clipX [−0.88, 0.96], clipY [−0.867, 0.833], background #111827):

**4 regions (variable-width columns):**

| Region | Market ($B) | Width (%) | X Range |
|--------|------------|-----------|---------|
| North America | 180 | 36 | 0–36 |
| Europe | 140 | 28 | 36–64 |
| Asia Pacific | 120 | 24 | 64–88 |
| Rest of World | 60 | 12 | 88–100 |

**3 segments per region (stacked bottom-to-top: Hardware → Software → Services):**

| Region | Hardware | Software | Services |
|--------|----------|----------|----------|
| NA | Y: 0–35 | Y: 35–75 | Y: 75–100 |
| EU | Y: 0–30 | Y: 30–65 | Y: 65–100 |
| AP | Y: 0–45 | Y: 45–75 | Y: 75–100 |
| RoW | Y: 0–25 | Y: 25–55 | Y: 55–100 |

**3 DrawItems for segments (instancedRect@1, rect4, 4 instances each):**
- Hardware (layer 10): #3b82f6 (blue), alpha 0.85
- Software (layer 11): #8b5cf6 (violet), alpha 0.85
- Services (layer 12): #f59e0b (amber), alpha 0.85

**1 DrawItem for 3 column separators (lineAA@1, rect4, 3 instances, layer 13):**
White, alpha 0.25, lineWidth 1. At X=36, 64, 88.

Data space: X=[0, 100], Y=[0, 100]. Transform: sx=0.0184, sy=0.017, tx=−0.88, ty=−0.867.

Text overlay: title, 4 region labels, 4 market size labels, 6 legend elements = 15 labels.

Total: 1 pane, 4 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems = 18 IDs.

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

- **All 12 rectangles are exact.** Every rect4 matches the expected (xLeft, yBottom, xRight, yTop) from the spec data. Hardware (4/4), Software (4/4), Services (4/4) — zero errors.

- **Stacking boundaries are perfectly continuous.** In all 4 columns: Hardware top = Software bottom, Software top = Services bottom, Services top = 100. Verified all 12 boundary pairs — zero gaps.

- **Column boundaries align perfectly.** North America ends at X=36 where Europe begins. Europe ends at X=64 where Asia Pacific begins. Asia Pacific ends at X=88 where Rest of World begins. The 3 column separator lines are at exactly these X positions.

- **The mosaic tiles the full data area.** Bottom-left corner (0,0) to top-right corner (100,100) is completely covered by the 12 rectangles. No gaps, no overlaps.

- **Column widths are proportional to market size.** NA: 36% (180/500), EU: 28% (140/500), AP: 24% (120/500), RoW: 12% (60/500). Sum = 100% ✓. The visual width differences are immediately apparent — NA is 3× wider than RoW.

- **Segment heights encode within-region shares.** Asia Pacific has the tallest Hardware (45%), Rest of World has the tallest Services (45%), North America has the tallest Software (40%). These patterns are clearly visible in the rendered mosaic.

- **Transform is correct.** X=0→clipX=−0.88, X=100→clipX=0.96. Y=0→clipY=−0.867, Y=100→clipY=0.833.

- **Column separators provide clear visual boundaries.** The 3 white lines (alpha 0.25) at X=36, 64, 88 cleanly delineate the regions without obscuring the underlying rectangles.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match.** Segments: 16/4=4 ✓ (×3). Separators: 12/4=3 ✓.

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Marimekko charts encode two dimensions in area.** Column width encodes one variable (market size), row height within each column encodes another (segment share). The visual area of each rectangle is proportional to the product — the absolute market value for that segment in that region.

2. **Variable-width columns use cumulative sums for X boundaries.** The X positions are: 0, 36, 64, 88, 100 (cumulative: 0, 180/500×100, (180+140)/500×100, ...). This is analogous to stacked bar Y boundaries but on the X axis.

3. **One DrawItem per segment type is efficient.** Since all Hardware rects share the same color, they go in one DrawItem (4 instances). Only 3 DrawItems needed for 12 rectangles.

4. **Perfect tiling requires exact boundary arithmetic.** Every rectangle's edges must match its neighbors. The key invariant: for each column, the segment percentages must sum to exactly 100 (35+40+25=100, 30+35+35=100, etc.).

5. **Column separator lines on the top layer add clarity.** Without separators, adjacent columns of the same color (e.g., Services amber in AP and RoW) would visually merge. The thin white lines prevent this.
