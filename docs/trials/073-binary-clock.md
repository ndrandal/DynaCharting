# Trial 073: Binary Clock

**Date:** 2026-03-12
**Goal:** Binary clock displaying 14:37:52 in BCD format as a 6×4 grid of circles (24 total), color-coded by time unit (hours blue, minutes emerald, seconds amber). Tests BCD digit encoding, triAA@1 circle tessellation at scale (24 circles, 3,456 vertices), and binary data visualization on a 600×400 viewport.
**Outcome:** All 10 ON bits and 14 OFF bits match BCD encoding of 14:37:52 exactly. Colors correct for all 3 time units. 24 circles total. 16 unique IDs. Zero defects.

---

## What Was Built

A 600×400 viewport with a single pane (background #0f172a):

**24 circles in a 6×4 grid (4 triAA@1 DrawItems, pos2_alpha):**

| DrawItem | Role | Count | Color | Alpha | Layer |
|----------|------|-------|-------|-------|-------|
| 102 | Hour ON | 2 | #3b82f6 (blue) | 0.9 | 11 |
| 105 | Minute ON | 5 | #10b981 (emerald) | 0.9 | 11 |
| 108 | Second ON | 3 | #f59e0b (amber) | 0.9 | 11 |
| 111 | OFF | 14 | #334155 (gray) | 0.3 | 10 |

Each circle: 144 vertices (48 triangles), inner radius 0.45 (alpha 1.0), outer fringe 0.55 (alpha 0.0).

**BCD encoding of 14:37:52:**

| Bit | H tens (1) | H units (4) | M tens (3) | M units (7) | S tens (5) | S units (2) |
|-----|-----------|------------|-----------|------------|-----------|------------|
| 8 | OFF | OFF | OFF | OFF | OFF | OFF |
| 4 | OFF | **ON** | OFF | **ON** | **ON** | OFF |
| 2 | OFF | OFF | **ON** | **ON** | OFF | **ON** |
| 1 | **ON** | OFF | **ON** | **ON** | **ON** | OFF |

10 ON + 14 OFF = 24 total circles.

Grid spacing: 1.5 data units between centers. Data space: [0, 9] × [0, 6].

Total: 16 unique IDs (1 pane, 2 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems).

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

- **BCD encoding matches 14:37:52 exactly.** All 10 ON positions verified: H tens bit 1, H units bit 4, M tens bits 1+2, M units bits 1+2+4, S tens bits 1+4, S units bit 2. Every digit decodes to the correct value.

- **All 14 OFF positions correct.** The complement of the 10 ON positions fills all remaining grid slots.

- **Color assignment by time unit is correct.** Hour columns (0–1) use blue, minute columns (2–3) use emerald, second columns (4–5) use amber. All 10 ON circles verified.

- **ON/OFF visual distinction is clear.** ON circles at alpha 0.9 on layer 11 are bright and saturated. OFF circles at alpha 0.3 on layer 10 are dim and recessive. The layering ensures ON circles render on top.

- **Circle tessellation with AA fringe.** 16 angular segments × 3 triangles (inner fan + fringe ring) = 48 triangles per circle. Inner alpha 1.0, outer alpha 0.0 creates smooth anti-aliased edges.

- **Grid spacing is uniform.** All circle centers at 1.5-unit intervals, confirmed by position extraction.

- **Total vertex count is correct.** 24 circles × 144 vertices = 3,456 total vertices across 4 DrawItems (288 + 720 + 432 + 2016 = 3456).

- **All buffer sizes match vertex counts.** 4/4 geometries verified (pos2_alpha: 3 fpv).

- **All 16 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **BCD encoding: each decimal digit maps to 4 binary bits independently.** The digit value is the sum of active bit weights (8, 4, 2, 1). This is different from pure binary encoding of the full number.

2. **Grouping circles by ON/OFF × color category minimizes DrawItems.** 4 DrawItems for 24 circles (3 ON colors + 1 OFF color) is more efficient than 24 individual DrawItems.

3. **triAA@1 with center-fan + fringe ring scales well.** 144 vertices per circle × 24 circles = 3,456 vertices — manageable for a grid of circles.

4. **Binary clocks have a natural 6×4 grid structure.** 6 BCD digits (2 each for H:M:S) × 4 bits per digit. Row 0 (bit 8) is rarely used — only for digits 8 and 9.

5. **Color-coding by time unit (H/M/S) provides immediate visual parsing.** The viewer can instantly distinguish which digits belong to which time component without reading labels.
