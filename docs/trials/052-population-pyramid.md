# Trial 052: Population Pyramid

**Date:** 2026-03-12
**Goal:** Back-to-back horizontal bar chart showing 12 age groups by gender. Male bars extend left (negative X) from center, female bars extend right. Tests negative-coordinate bars (instancedRect@1 with negative xMin), centered transform (tx=0), and symmetric pyramid layout on a 900×700 viewport.
**Outcome:** All 12 male bars and 12 female bars at correct positions and lengths. Center axis at X=0. Pyramid shape clearly visible. Zero defects.

---

## What Was Built

A 900×700 viewport with a single pane (background #0f172a):

**12 male bars (1 instancedRect@1 DrawItem, rect4, 12 instances):**
Blue (#3b82f6), alpha 0.8. Each bar spans [−maleValue, y+0.1] to [0, y+0.9].

**12 female bars (1 instancedRect@1 DrawItem, rect4, 12 instances):**
Pink (#ec4899), alpha 0.8. Each bar spans [0, y+0.1] to [femaleValue, y+0.9].

| Age Group (y) | Male (%) | Female (%) |
|---------------|----------|------------|
| 0–4 (0) | 3.2 | 3.0 |
| 5–14 (1) | 6.5 | 6.1 |
| 15–24 (2) | 7.8 | 7.4 |
| 25–34 (3) | 8.5 | 8.2 |
| 35–44 (4) | 7.2 | 7.0 |
| 45–54 (5) | 6.8 | 6.9 |
| 55–64 (6) | 5.5 | 5.8 |
| 65–74 (7) | 3.8 | 4.2 |
| 75–84 (8) | 2.1 | 2.8 |
| 85–94 (9) | 0.8 | 1.2 |
| 95–104 (10) | 0.2 | 0.4 |
| 105+ (11) | 0.02 | 0.05 |

**Center axis (1 lineAA@1 DrawItem, rect4, 1 instance):**
From (0, 0) to (0, 12). White, alpha 0.3, lineWidth 1.5.

**Grid (2 lineAA@1 DrawItems):**
- 12 horizontal lines at Y=0..11 (white, alpha 0.04)
- 4 vertical lines at X=−5, −2.5, 2.5, 5 (white, alpha 0.04)

Data space: X=[−10, 10], Y=[0, 12]. Transform 50: sx=0.095, sy=0.158333, tx=0, ty=−0.95.

Layers: Grid (10) → Bars (11) → Center axis (12).

Total: 20 unique IDs.

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

- **All 12 male bars at correct positions.** Each bar has xMin=−maleValue, xMax=0, yMin=i+0.1, yMax=i+0.9. All 12/12 verified.

- **All 12 female bars at correct positions.** Each bar has xMin=0, xMax=femaleValue, yMin=i+0.1, yMax=i+0.9. All 12/12 verified.

- **Negative X coordinates work correctly.** Male bars use negative xMin values (e.g., −8.5 for the 25–34 group). The instancedRect@1 pipeline handles negative data coordinates through the transform, producing bars extending left from center.

- **Centered transform (tx=0) correctly positions X=0 at the viewport center.** With sx=0.095 and tx=0: X=−10 → clip=−0.95, X=0 → clip=0, X=10 → clip=0.95. The center axis is at the exact pixel center of the viewport.

- **Pyramid shape is clearly visible.** The widest bars (25–34 age group) are at row 3, with bars narrowing both upward (older) and downward (younger). The characteristic demographic pyramid shape is immediately recognizable.

- **Gender differences are visible.** Female bars are slightly longer in older age groups (65+), reflecting higher female life expectancy. Male bars are slightly longer in younger working-age groups (15–34).

- **Bar height and gaps are correct.** Each bar is 0.8 data units tall (y+0.1 to y+0.9) with 0.2 gaps between adjacent age groups.

- **Center axis provides clear separation.** The white line at X=0 from Y=0 to Y=12 cleanly divides male (left) and female (right) sides.

- **Transform math is exact.** sx=1.9/20=0.095 maps X=[−10,10] to clip[−0.95,0.95]. sy=1.9/12≈0.158333 maps Y=[0,12] to clip[−0.95,0.95].

- **Vertical grid at symmetric positions.** X=−5, −2.5, 2.5, 5 creates symmetric reference lines on both sides of the axis.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 5/5 geometries verified.

- **All 20 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Population pyramids use negative coordinates for one gender.** Male bars extend left (negative X) from center X=0, female bars extend right (positive X). The centered transform (tx=0) places the axis at the viewport center.

2. **instancedRect@1 handles negative coordinates natively.** No special handling needed — just use negative xMin values and the transform maps them to the correct clip-space positions.

3. **Centered transforms (tx=0) are useful for symmetric charts.** When the data is symmetric around X=0, setting tx=0 places the center at the viewport midpoint. The sx scales the full range [−max, +max] to clip[−0.95, 0.95].

4. **Bar height should be less than row spacing.** 0.8 bar height in 1.0 spacing creates 0.2 gaps that visually separate age groups without wasting space.

5. **Gender differences in older age groups are a natural test of bar accuracy.** The crossover where female percentages exceed male percentages (starting at age 45–54 in this data) creates an asymmetric pyramid shape that verifies both sides are independently correct.
