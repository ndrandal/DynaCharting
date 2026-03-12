# Trial 068: Periodic Table

**Date:** 2026-03-12
**Goal:** First 36 elements (H through Kr) of the periodic table in standard positions, color-coded by 7 element categories (hydrogen, alkali, alkaline earth, transition metals, other metals, nonmetals/metalloids, noble gases). Tests instancedRect@1 grid layout with gaps, 7-category color grouping, standard periodic table positioning with empty gaps in periods 1–3, and text overlay for element symbols on a 1200×700 viewport.
**Outcome:** All 36 elements at correct positions with correct category colors. Category counts match exactly (1+3+3+10+3+12+4 = 36). Cell dimensions 0.90×0.90 with 0.10 gap. 37 text labels. 24 unique IDs. Zero defects.

---

## What Was Built

A 1200×700 viewport with a single pane (background #0f172a):

**36 element cells in 7 color groups (7 instancedRect@1 DrawItems, rect4):**

| DrawItem | Category | Color | Count | Elements |
|----------|----------|-------|-------|----------|
| 102 | Hydrogen | #f59e0b (amber) | 1 | H |
| 105 | Alkali metals | #ef4444 (red) | 3 | Li, Na, K |
| 108 | Alkaline earth | #f97316 (orange) | 3 | Be, Mg, Ca |
| 111 | Transition metals | #3b82f6 (blue) | 10 | Sc–Zn |
| 114 | Other metals | #06b6d4 (cyan) | 3 | Al, Ga, Ge |
| 117 | Nonmetals/metalloids | #10b981 (emerald) | 12 | B,C,N,O,F,Si,P,S,Cl,As,Se,Br |
| 120 | Noble gases | #a855f7 (purple) | 4 | He, Ne, Ar, Kr |

All cells: 0.90×0.90 data units within 1.0×1.0 grid slots (0.10 gap). Alpha 0.85.

**Standard periodic table layout:**
- Period 1 (row 0): H at col 0, He at col 17 (16-column gap)
- Period 2 (row 1): Li–Be at cols 0–1, B–Ne at cols 12–17 (10-column gap)
- Period 3 (row 2): Na–Mg at cols 0–1, Al–Ar at cols 12–17 (10-column gap)
- Period 4 (row 3): K–Kr fills all 18 columns (first transition metal row)

Y-axis inverted: row 0 (Period 1) at data y=3.05, row 3 (Period 4) at y=0.05. Period 1 appears at top of viewport.

Data space: X=[0, 18], Y=[0, 4]. Transform 50: sx=0.105556, sy=0.475, tx=−0.95, ty=−0.95.

**37 text labels:** All 36 element symbols + title "Periodic Table" at top center.

Total: 24 unique IDs (1 pane, 1 layer, 1 transform, 7 buffers, 7 geometries, 7 drawItems).

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. 37 labels (36 symbols + title) are defined but not rendered in the PNG.

---

## Spatial Reasoning Analysis

### Done Right

- **All 36 elements at correct standard periodic table positions.** Every element verified against the canonical (col, row) assignment. No displaced or missing cells.

- **All 7 category colors match expected assignments.** Hydrogen (amber), alkali metals (red), alkaline earth (orange), transition metals (blue), other metals (cyan), nonmetals/metalloids (emerald), noble gases (purple). Every element checked against its expected category.

- **Category instance counts are exact.** 1+3+3+10+3+12+4 = 36. Each DrawItem has precisely the right number of instances for its category.

- **Standard periodic table gaps rendered correctly.** The empty region (cols 2–11 in periods 1–3) creates the characteristic "step" shape. Period 4 fills all 18 columns, showing the first complete row with transition metals.

- **Cell dimensions are uniform.** All 36 cells measure exactly 0.90×0.90 data units, with 0.10 gap between adjacent cells.

- **Y-axis inversion places Period 1 at top.** Row 0 mapped to y=3.05 and row 3 to y=0.05 ensures the standard top-to-bottom period ordering.

- **Wide viewport suits the periodic table aspect ratio.** 1200×700 (1.71:1) accommodates the 18-column × 4-row layout well. Each cell renders at approximately 57×155 pixels — tall rectangles due to the 18:4 data aspect ratio on a wide viewport.

- **Transform maps full data range to clip space.** X=[0,18] → clip [−0.95, 0.95], Y=[0,4] → clip [−0.95, 0.95] with 5% margin.

- **All buffer sizes match vertex counts.** 7/7 geometries verified (rect4: 4 fpv).

- **All 24 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Periodic table layout requires non-uniform column filling.** Periods 1–3 have gaps (cols 2–11 empty) while Period 4 fills all 18 columns. The standard layout encodes chemistry in its spatial structure — the gap separates s-block (cols 0–1) from p-block (cols 12–17).

2. **7 instancedRect@1 DrawItems for 7 categories is efficient.** Grouping elements by category into separate DrawItems (each with its own color) avoids per-element color management. One DrawItem per color.

3. **Y-axis inversion via data positioning.** Instead of using negative sy, the agent placed row 0 at y=3 and row 3 at y=0. Both approaches work; this one keeps sy positive and avoids transform sign confusion.

4. **Cell gap via undersized rectangles.** 0.90×0.90 cells in 1.0×1.0 slots create consistent 0.10 gaps. The 0.05 offset positions each cell centered in its slot.

5. **36 elements is the natural boundary for a 4-period table.** Period 4 (K through Kr) is the first row that fills all 18 columns, making it a natural stopping point for a compact periodic table.
