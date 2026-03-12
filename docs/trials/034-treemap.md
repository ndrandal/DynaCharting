# Trial 034: Treemap

**Date:** 2026-03-12
**Goal:** Category-grouped treemap showing disk usage — 12 items across 3 categories (Media, System, Apps), laid out as 3 vertical columns with items stacked proportionally within each. Tests instancedRect@1 with 12 unique-color tiles that must perfectly tile a rectangle (total area = 10,000 sq units, zero gaps, zero overlaps), and area-proportional layout.
**Outcome:** All 12 tiles are exact. Areas are proportional to GB values. Column widths sum to 100. Item heights within each column sum to 100. All stacking boundaries continuous. Total tiled area exactly 10,000. Zero defects.

---

## What Was Built

A 1000×700 viewport with a single pane (background #0f172a):

**3 category columns (proportional to total category size):**

| Category | Total GB | Width (%) | X Range |
|----------|----------|-----------|---------|
| Media | 470 | 56.287 | 0–56.287 |
| System | 200 | 23.952 | 56.287–80.240 |
| Apps | 165 | 19.760 | 80.240–100 |

**12 tiles (instancedRect@1, rect4, 1 instance each):**

| Item | GB | Category | Area | Color |
|------|-----|----------|------|-------|
| Videos | 180 | Media | 2155.7 | #3b82f6 (blue) |
| Photos | 120 | Media | 1437.1 | #60a5fa (light blue) |
| Documents | 75 | Media | 898.2 | #93c5fd (pale blue) |
| Music | 55 | Media | 658.7 | #bfdbfe (very pale blue) |
| Downloads | 40 | Media | 479.0 | #dbeafe (near-white blue) |
| System | 95 | System | 1137.7 | #f97316 (orange) |
| Cache | 45 | System | 538.9 | #fb923c (light orange) |
| Backups | 35 | System | 419.2 | #fdba74 (pale orange) |
| Logs | 25 | System | 299.4 | #fed7aa (very pale orange) |
| Games | 85 | Apps | 1018.0 | #10b981 (emerald) |
| Apps | 65 | Apps | 778.4 | #34d399 (light emerald) |
| Temp | 15 | Apps | 179.6 | #6ee7b7 (pale emerald) |

**2 column separator lines (1 lineAA@1 DrawItem, rect4, 2 instances):** White, alpha 0.3, lineWidth 1. At X=56.287 and X=80.240.

Data space: X=[0, 100], Y=[0, 100]. Transform: sx=sy=0.019, tx=ty=−0.95.

Text overlay: title + 12 item labels + 3 category headers + 12 size labels = 28 labels.

Total: 1 pane, 13 layers, 1 transform, 13 buffers, 13 geometries, 13 drawItems = 54 IDs.

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

- **All 12 tile areas are exactly proportional to their GB values.** Each tile's area = (GB / 835) × 10,000. Verified all 12 to <0.01% error.

- **Total tiled area is exactly 10,000.** The 12 tiles perfectly fill the [0,100]×[0,100] data rectangle with zero gaps and zero overlaps.

- **Column widths are proportional to category totals.** Media: 470/835 = 56.287%, System: 200/835 = 23.952%, Apps: 165/835 = 19.760%. Sum = 100.000%.

- **Item heights within each column sum to exactly 100.** Media: 38.298+25.532+15.957+11.702+8.511 = 100.000. System: 47.500+22.500+17.500+12.500 = 100.000. Apps: 51.515+39.394+9.091 = 100.000.

- **All stacking boundaries are continuous.** Verified 9 inter-item boundaries (4 in Media, 3 in System, 2 in Apps) — top of each tile exactly equals bottom of the next.

- **Color coding creates clear category grouping.** Media tiles use blue gradient (dark→light as size decreases). System uses orange gradient. Apps uses emerald gradient. The color intensity encoding makes relative sizes within each category immediately readable.

- **Separator lines delineate column boundaries.** Thin white lines at the 2 column boundaries provide visual separation even between tiles of different category colors.

- **Transform is correct.** sx=sy=0.019 maps [0,100] to a 1.9-unit clip range centered at (−0.95, −0.95) to (0.95, 0.95).

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All 54 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Category-grouped column layout is a tractable treemap algorithm.** Rather than a full squarified treemap (complex, error-prone), grouping by category into proportional columns with stacked items produces a readable layout with simple arithmetic.

2. **Area proportionality requires both width and height to be proportional.** Column width ∝ category total, item height within column ∝ item share of category. The product (area) equals item share of grand total — the mathematical property that defines a treemap.

3. **One DrawItem per tile enables unique colors.** With 12 distinct colors (3 gradients of 4-5 shades), each tile needs its own DrawItem. This uses more IDs (54 total) but is the only way to achieve per-tile coloring with instancedRect@1.

4. **Color gradients within categories encode hierarchy.** Darker = larger creates a secondary visual encoding that reinforces the area-based primary encoding. Users can identify both the category (by hue) and relative size (by saturation/lightness).

5. **Perfect tiling requires exact arithmetic.** All boundary values must use full floating-point precision. Rounding would introduce sub-pixel gaps visible as thin background-colored lines between tiles.
