# Trial 030: Butterfly Chart (Population Pyramid)

**Date:** 2026-03-12
**Goal:** Population pyramid with 9 age groups — male bars extending left, female bars extending right from a central axis. Tests instancedRect@1 with bidirectional horizontal bars (negative X for males, positive X for females), symmetric layout around X=0, lineAA@1 center axis and grid lines, and 4-layer depth ordering.
**Outcome:** All 9 male bars, 9 female bars, center axis, and 9 grid lines are exact. The characteristic butterfly shape is clearly visible. Zero defects.

---

## What Was Built

A 900×650 viewport with a single pane (clipX [−0.889, 0.933], clipY [−0.877, 0.846], background #0f172a):

**9 age groups (rows 1–9, bottom to top):**

| Age Group | Row | Male (left) | Female (right) |
|-----------|-----|-------------|----------------|
| 0–9       | 1   | 32M         | 30M            |
| 10–19     | 2   | 34M         | 32M            |
| 20–29     | 3   | 38M         | 36M            |
| 30–39     | 4   | 42M         | 40M            |
| 40–49     | 5   | 40M         | 39M            |
| 50–59     | 6   | 35M         | 36M            |
| 60–69     | 7   | 28M         | 31M            |
| 70–79     | 8   | 18M         | 22M            |
| 80+       | 9   | 8M          | 12M            |

**1 DrawItem for male bars (instancedRect@1, rect4, 9 instances):** Blue #3b82f6, alpha 0.85. Bars extend from X=−male_value to X=0.

**1 DrawItem for female bars (instancedRect@1, rect4, 9 instances):** Pink #ec4899, alpha 0.85. Bars extend from X=0 to X=female_value.

**1 DrawItem for center axis (lineAA@1, rect4, 1 instance):** White, alpha 0.3, lineWidth 1. Vertical line at X=0 from Y=0.5 to Y=9.5.

**1 DrawItem for grid lines (lineAA@1, rect4, 9 instances):** White, alpha 0.06, lineWidth 1. Horizontal lines at Y=1..9 spanning full X range [−45, 45].

Bar height: ±0.35 data units = 39.2px. Inter-row gap: 0.3 data units = 16.8px.

Data space: X=[−45, 45], Y=[0, 10]. Transform: sx=0.020247, sy=0.17231, tx=0.022222, ty=−0.8769. X=0 maps to clipX=0.022 (pane center).

Layers: grid (10) → male bars (11) → female bars (12) → center axis (13).

Text overlay: title, subtitle, 9 age labels (centered on axis), 6 axis scale labels (symmetric 15M/30M/45M), 2 legend labels = 19 labels.

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

- **All 9 male bars are exact.** Each bar spans [−male_value, row−0.35, 0, row+0.35]. Verified all 9 — zero errors. The bars correctly extend leftward from X=0.

- **All 9 female bars are exact.** Each bar spans [0, row−0.35, female_value, row+0.35]. Verified all 9 — zero errors. The bars correctly extend rightward from X=0.

- **The butterfly shape is clearly visible.** Widest at rows 4–5 (30–39: 42M+40M, 40–49: 40M+39M), narrowing to row 9 (80+: 8M+12M) at top and row 1 (0–9: 32M+30M) at bottom. The asymmetry where female bars are slightly longer than male in older age groups (60+) is visible — a realistic demographic pattern.

- **Center axis at X=0 correctly divides male and female.** The vertical line at (0, 0.5, 0, 9.5) spans the full data range. X=0 maps to clipX=0.022, which is exactly the center of the pane (pane center = (−0.889 + 0.933)/2 = 0.022).

- **Grid lines span the full data width.** All 9 horizontal lines run from X=−45 to X=45 at each row center. They provide subtle visual reference behind the bars.

- **Transform is exact.** X=−45→clipX=−0.889, X=0→clipX=0.022, X=45→clipX=0.933. Y=0→clipY=−0.877, Y=10→clipY=0.846. The slight X offset (tx=0.022) places X=0 at the pane center, accommodating the asymmetric left margin for labels.

- **Bar proportions are correct.** Bar height 39.2px with 16.8px gaps gives clear separation. The 0.7:0.3 ratio (bar:gap) makes the bars prominent without crowding.

- **4-layer depth ordering is correct.** Grid (10, behind) → male bars (11) → female bars (12) → center axis (13, on top). The center axis renders on top of both bar sets, creating a clean dividing line.

- **Age group labels are centered on the axis.** All 9 labels at clipX=0.022 (the center axis position), with clipY values matching row × sy + ty. These will overlay the center axis line in the live viewer.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All vertex counts match.** Male: 36/4=9 ✓. Female: 36/4=9 ✓. Axis: 4/4=1 ✓. Grid: 36/4=9 ✓.

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Butterfly charts use negative X for one side.** Male bars span [−value, 0] while female bars span [0, +value]. The data range must be symmetric (X=[−45, 45]) to give equal visual weight to both sides.

2. **The center axis divides the chart visually.** A thin vertical line at X=0 on the top layer provides a clean reference line separating the two groups. Drawing it above the bars ensures it's always visible.

3. **Asymmetric pane offset centers X=0.** With a left margin for labels, the pane clipX range is asymmetric ([−0.889, 0.933]), but the transform's tx=0.022 places X=0 at the pane center. This is the correct approach — the data's center of symmetry should align with the visual center.

4. **Population pyramids naturally show demographic patterns.** The wider middle rows (working age), narrowing at both ends, and the gender crossover in older age groups (female > male) are immediately readable from the bar lengths alone.

5. **Only 4 DrawItems for 18 bars + 10 lines.** Grouping all male bars into one DrawItem and all female bars into another is efficient — instancedRect@1 renders all 9 instances with one draw call each. Similarly, grid lines share one DrawItem.
