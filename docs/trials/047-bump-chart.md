# Trial 047: Bump Chart

**Date:** 2026-03-12
**Goal:** Bump chart showing 6 entities changing rank across 8 time periods. Tests Y-axis inversion (negative sy for rank-1-at-top), line crossings, per-entity coloring, aspect-corrected rank dots, and grid overlay. 42 line segments, 48 rank dots, 14 grid lines.
**Outcome:** All 42 line segments match ranking data exactly. All 48 dot positions correct. Y-axis inversion working (rank 1 at top, rank 6 at bottom). Dots perfectly circular at 6px. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**6 entity lines (6 lineAA@1 DrawItems, rect4, 7 instances each):**
lineWidth 3.0, alpha 0.8.

| Entity | Color | Ranking T1→T8 |
|--------|-------|---------------|
| Alpha | #3b82f6 (blue) | 1,1,2,3,2,1,1,2 |
| Beta | #10b981 (emerald) | 2,3,1,1,1,2,3,1 |
| Gamma | #f59e0b (amber) | 3,2,3,2,4,4,2,3 |
| Delta | #ec4899 (pink) | 4,4,4,5,3,3,4,4 |
| Epsilon | #8b5cf6 (violet) | 5,5,6,4,5,6,5,5 |
| Zeta | #06b6d4 (cyan) | 6,6,5,6,6,5,6,6 |

Each line has 7 segments connecting consecutive (time, rank) positions.

**48 rank dots (6 triAA@1 DrawItems, pos2_alpha, 384 vertices each = 8 circles × 48 verts):**
Same colors as lines, alpha 1.0. Aspect-corrected 6px circles (rx=0.0568 data, ry=0.0737 data). 16 segments per circle, center-fan tessellation.

**8 time column lines (1 lineAA@1 DrawItem, rect4, 8 instances):**
Vertical lines at X=1..8, from Y=0.5 to Y=6.5. White, alpha 0.04, lineWidth 1.

**6 rank lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
Horizontal lines at Y=1..6, from X=0.5 to X=8.5. White, alpha 0.04, lineWidth 1.

Data space: X=[0, 9], Y=[0, 7]. Transform 50: sx=0.211111, sy=−0.271429, tx=−0.95, ty=0.95.

**Y-axis inversion:** Negative sy maps rank 1 to clipY=0.679 (pixel Y=96, near top) and rank 6 to clipY=−0.679 (pixel Y=504, near bottom).

Layers: Grid (10) → Lines (11) → Dots (12).

Total: 47 unique IDs.

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

- **All 42 line segments match ranking data exactly.** Each of the 6 entities has 7 segments connecting (T_i, rank_i) to (T_{i+1}, rank_{i+1}). All 42 segments verified against the spec. 42/42 correct.

- **All 48 dot positions correct.** Each entity has 8 dots at (time, rank) positions. All centers verified: 48/48 match the ranking table.

- **Y-axis inversion works correctly.** Negative sy=−0.271429 maps rank 1 to clipY=0.679 (near top) and rank 6 to clipY=−0.679 (near bottom). The PNG confirms rank 1 entities (Alpha, Beta) appear at the top and rank 6 entities (Zeta) appear at the bottom.

- **Line crossings are visible.** Where entities swap ranks (e.g., Alpha↔Beta at T3, Delta↔Epsilon at T4-T5), the colored lines cross each other. The crossing pattern is the defining visual feature of bump charts.

- **Dots are perfectly circular at 6px.** Despite different X and Y scales (px_per_dx=105.56, px_per_dy=81.43), aspect correction produces exact 6.00px radius with 0.0000px spread across all 16 rim vertices.

- **Transform math is exact.** sx=1.9/9, sy=−1.9/7, tx=−0.95, ty=0.95. The negative sy with positive ty correctly inverts the Y axis for rank display.

- **Layer ordering is correct.** Grid (10, back) → Lines (11) → Dots (12, front). Dots draw over line intersections, providing clear position markers. Grid provides subtle reference without obscuring data.

- **Grid lines at correct positions.** 8 vertical time columns at X=1..8, 6 horizontal rank lines at Y=1..6. Time columns extend from Y=0.5 to Y=6.5 (half-rank padding), rank lines from X=0.5 to X=8.5 (half-period padding).

- **6 distinct colors with sufficient contrast.** Blue, emerald, amber, pink, violet, cyan are easily distinguishable against the dark background and from each other, even at line crossings.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All buffer sizes match vertex counts.** All 14 geometries verified. 14/14 correct.

- **All 47 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Y-axis inversion uses negative sy.** For rank charts where rank 1 = top, set sy negative and ty positive. This maps low Y values (rank 1) to high clip-space Y (top of screen). No need to invert the data — the transform handles it.

2. **Bump charts show rank changes through line crossings.** The visual power comes from lines crossing each other when entities swap ranks. Thick lines (lineWidth 3) make crossings clearly visible.

3. **Dots at rank positions provide precise reading.** Without dots, it would be hard to tell exact rank at each time period, especially where multiple lines are close together. The dots serve as both visual anchors and data markers.

4. **Grid lines at integer positions aid rank reading.** Horizontal lines at each rank position (1..6) and vertical lines at each time period (1..8) create a matrix that makes it easy to read exact ranks.

5. **Per-entity DrawItems are required for both lines and dots.** Each entity has a unique color, so both lines and dots need per-entity DrawItems. Grid lines (all white) can be grouped into 2 DrawItems (vertical + horizontal).
