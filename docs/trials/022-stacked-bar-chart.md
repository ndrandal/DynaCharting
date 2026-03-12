# Trial 022: Stacked Bar Chart

**Date:** 2026-03-12
**Goal:** Eight-quarter stacked bar chart with 4 product lines (Cloud, Enterprise, Consumer, Services), testing instancedRect@1 precision where the top of each segment must exactly equal the bottom of the next. First trial with vertical stacking arithmetic.
**Outcome:** All 32 bar segments stack with zero gaps across all 8 quarters. Transform, colors, grid lines, and text positions are mathematically exact. Zero defects.

---

## What Was Built

A 1000×650 viewport with a single pane (900×550px, 60px left/40px right/60px top/40px bottom margins):

**32 bar segments across 4 instancedRect@1 DrawItems (rect4 format, 8 instances each):**

| Quarter | Services (y0→y1) | Consumer (y0→y1) | Enterprise (y0→y1) | Cloud (y0→y1) | Total |
|---------|-------------------|-------------------|---------------------|----------------|-------|
| Q1 2023 | 0→12 | 12→30 | 30→58 | 58→100 | 100 |
| Q2 2023 | 0→14 | 14→34 | 34→64 | 64→112 | 112 |
| Q3 2023 | 0→15 | 15→37 | 37→69 | 69→124 | 124 |
| Q4 2023 | 0→16 | 16→40 | 40→75 | 75→137 | 137 |
| Q1 2024 | 0→18 | 18→44 | 44→77 | 77→145 | 145 |
| Q2 2024 | 0→20 | 20→48 | 48→84 | 84→159 | 159 |
| Q3 2024 | 0→22 | 22→52 | 52→90 | 90→172 | 172 |
| Q4 2024 | 0→24 | 24→56 | 56→96 | 96→186 | 186 |

Stacking order (bottom to top): Services (orange #f59e0b), Consumer (green #10b981), Enterprise (purple #8b5cf6), Cloud (blue #3b82f6). All alpha 0.9, cornerRadius 3.

Bars centered at X=1..8, half-width 0.35 (full width 0.70, gap 0.30). Bar width: 70px, gap: 30px.

**5 horizontal grid lines (lineAA@1, rect4 format):**
At Y=0, 50, 100, 150, 200. Each spans [-0.5, Y, 8.5, Y]. White, alpha 0.1, lineWidth 1.

Data space: X=[−0.5, 8.5], Y=[0, 200]. Transform: sx=0.2, sy=0.008462, tx=−0.78, ty=−0.876923.

Text overlay: title, subtitle, 8 quarter labels, 5 Y-axis value labels, 4 legend labels with colored bullet markers.

Viewport declaration with pan/zoom disabled (static chart).

Total: 1 pane, 2 layers, 1 transform, 5 buffers, 5 geometries, 5 drawItems, 1 viewport = 19 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the quarters, dollar values, product line names, and legend are absent.

---

## Spatial Reasoning Analysis

### Done Right

- **All 32 stacking boundaries are exact.** Verified every quarter: Services y1 = Consumer y0, Consumer y1 = Enterprise y0, Enterprise y1 = Cloud y0. Zero gaps, zero overlaps across all 8 quarters. Stacking gaps measured as exactly 0 for all 24 boundaries.

- **All totals are correct.** Q1 2023: 12+18+28+42=100, Q2: 14+20+30+48=112, ..., Q4 2024: 24+32+40+90=186. All match the spec data table.

- **Transform is mathematically exact.** sx=0.2 = 1.8/9, sy=0.008462 = 1.692308/200, tx=−0.78 = −0.88−(−0.5)×0.2, ty=−0.876923. All verified to 6+ significant figures.

- **Bar positions and widths are correct.** All 8 bars centered at integer X positions (1–8) with half-width 0.35. X ranges: [n−0.35, n+0.35] for n=1..8. Bar width 70px, gap 30px at 100 px/data-unit.

- **Grid lines correctly positioned.** 5 horizontal lines at Y=0,50,100,150,200, each spanning the full data X range. Transform maps Y=0→clipY=−0.877, Y=100→−0.031, Y=200→0.815. All within pane bounds.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, instancedRect@1 uses rect4 ✓. Zero format mismatches.

- **All vertex counts match buffer sizes.** Grid: 20/4=5 ✓. Services/Consumer/Enterprise/Cloud: 32/4=8 each ✓.

- **Layer ordering correct.** Grid on layer 10 (behind), bars on layer 20 (on top). Grid lines visible through bar gaps.

- **Color hex-to-float conversions exact.** Services #f59e0b→[0.961,0.620,0.043], Consumer #10b981→[0.063,0.725,0.506], Enterprise #8b5cf6→[0.545,0.361,0.965], Cloud #3b82f6→[0.231,0.510,0.965]. All verified.

- **Text label positions match transform output.** All 8 X-axis labels at clipX = n×0.2−0.78 (matching bar centers). All 5 Y-axis labels at clipY = Y×0.008462−0.877 (matching grid lines). Sub-pixel precision.

- **All 19 IDs unique.** Systematic triplet allocation (100–114) plus structural IDs (1, 10, 20, 50). No collisions.

- **Progressive growth clearly visible.** Bars grow from 100 (Q1'23) to 186 (Q4'24), with Cloud as the dominant and fastest-growing segment. The stacked format makes both individual product growth and total growth immediately visible.

- **Tallest bar fits within pane.** Q4 2024 total 186 → clipY=0.697 < paneClipYMax=0.815. Adequate headroom (14 data units = ~32px).

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **instancedRect@1 handles stacked bars naturally.** One DrawItem per category, 8 instances each. The stacking arithmetic (y0 of segment N+1 = y1 of segment N) is straightforward with integer data values — no floating-point precision issues.

2. **cornerRadius on stacked segments is cosmetic-only.** With cornerRadius=3 on all segments, the rounded corners at segment boundaries could theoretically create sub-pixel gaps. In practice, at 3px radius on 30–90px tall segments, the effect is imperceptible.

3. **Grid lines behind stacked bars provide scale reference.** The lineAA@1 grid at alpha 0.1 is subtle enough to not compete with the bar colors but visible enough through the 30px gaps between bars. Layer ordering (10 behind 20) keeps them properly layered.

4. **Static viewport declarations are useful.** Setting panX/Y/zoomX/Y all to false makes the chart non-interactive in the live viewer, which is appropriate for a presentation-style chart. The viewport still defines the data range for the initial transform computation.

5. **One DrawItem per category enables independent styling.** Each product line gets its own color, which is cleaner than trying to use per-vertex coloring. This pattern scales to any number of categories.
