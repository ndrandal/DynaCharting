# Trial 023: Dumbbell Chart

**Date:** 2026-03-12
**Goal:** Ten-department horizontal dumbbell chart comparing employee satisfaction before (2019) and after (2024) a remote work policy — thin connector lines with gray "before" circles and color-coded "after" circles (green improved, red declined). Tests lineAA@1 connectors + triAA@1 endpoints with mixed direction, extreme aspect correction (3.68:1), and 4-layer depth ordering.
**Outcome:** All 10 connector endpoints, 20 circle centers, aspect correction, and text positions are exact. The before/after pattern with directional color coding is immediately readable. Zero defects.

---

## What Was Built

A 900×650 viewport with a single pane (680×550px, 180px left/40px right/60px top/40px bottom margins):

**10 departments (bottom to top):**

| Row | Department | 2019 (Before) | 2024 (After) | Change | After Color |
|-----|-----------|---------------|--------------|--------|-------------|
| 1 | Executive | 80 | 76 | −4 | Red |
| 2 | R&D | 69 | 88 | +19 | Green |
| 3 | Support | 58 | 85 | +27 | Green |
| 4 | Legal | 74 | 72 | −2 | Red |
| 5 | Operations | 62 | 78 | +16 | Green |
| 6 | Finance | 70 | 68 | −2 | Red |
| 7 | HR | 65 | 80 | +15 | Green |
| 8 | Sales | 75 | 71 | −4 | Red |
| 9 | Marketing | 68 | 82 | +14 | Green |
| 10 | Engineering | 72 | 89 | +17 | Green |

**10 connector lines (1 lineAA@1 DrawItem, rect4, 10 instances):**
Each connects [min_score, row, max_score, row]. White, alpha 0.2, lineWidth 1.5.

**10 "before" circles (1 triAA@1 DrawItem, pos2_alpha, 1440 vertices):**
Gray (#6b7280), alpha 0.8. All at 2019 score positions.

**6 "after improved" circles (1 triAA@1 DrawItem, pos2_alpha, 864 vertices):**
Green (#34d399), alpha 1.0. Engineering, Marketing, HR, Operations, Support, R&D.

**4 "after declined" circles (1 triAA@1 DrawItem, pos2_alpha, 576 vertices):**
Red (#f87171), alpha 1.0. Sales, Finance, Legal, Executive.

Circle radii: X=0.5147, Y=0.14 data units. Pixel radius: 7.000px in both axes. Aspect correction: 3.6765 (px_per_dy/px_per_dx = 50.0/13.6).

16 angular segments per circle, 144 vertices each (48 core + 96 fringe). Fringe: 2.5px.

**6 vertical grid lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
At X=50, 60, 70, 80, 90, 100. White, alpha 0.06, lineWidth 1.

Data space: X=[50, 100], Y=[0, 11]. Transform: sx=0.030222, sy=0.153846, tx=−2.111111, ty=−0.876923.

Layers: grid (10) → connectors (11) → before circles (12) → after circles (13).

Text overlay: title, subtitle, 10 department names, 10 after-score values (color-coded), 6 X-axis labels, 3 legend entries.

Total: 1 pane, 4 layers, 1 transform, 5 buffers, 5 geometries, 5 drawItems = 21 IDs.

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

- **All 10 connector lines match the data.** Each connects min(2019, 2024) to max(2019, 2024) at the correct row. Verified all 10: Executive [76,1,80,1], R&D [69,2,88,2], ..., Engineering [72,10,89,10].

- **All 10 "before" circle centers are exact.** Executive at (80,1), R&D at (69,2), Support at (58,3), Legal at (74,4), Operations at (62,5), Finance at (70,6), HR at (65,7), Sales at (75,8), Marketing at (68,9), Engineering at (72,10). All match 2019 scores.

- **All 10 "after" circle centers are exact.** Improved: R&D (88,2), Support (85,3), Operations (78,5), HR (80,7), Marketing (82,9), Engineering (89,10). Declined: Executive (76,1), Legal (72,4), Finance (68,6), Sales (71,8). All match 2024 scores.

- **Directional color coding is correct.** 6 improved departments get green after-circles, 4 declined get red. No misclassifications.

- **Circles are perfectly circular at 7px.** X_data_radius (0.5147) × px_per_dx (13.6) = Y_data_radius (0.14) × px_per_dy (50.0) = 7.000px. Aspect correction factor 3.6765 correctly applied — X radius is 3.68× larger than Y radius in data units.

- **Fringe is exactly 2.5px.** X fringe: 0.18382 × 13.6 = 2.500px. Verified from buffer data.

- **Transform is exact.** X=50→clipX=−0.600, X=100→clipX=0.911. Y=0→clipY=−0.877, Y=11→clipY=0.815. All verified.

- **4-layer depth ordering creates correct visual hierarchy.** Grid (10, behind) → connectors (11) → before circles (12) → after circles (13, on top). After circles render on top of before circles, emphasizing the current state.

- **Connector line positions are visually correct.** Short connectors for small changes (Legal ±2, Finance ±2), long connectors for large changes (Support +27, R&D +19). The visual encoding immediately communicates magnitude.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All vertex counts match.** Grid: 24/4=6 ✓. Connectors: 40/4=10 ✓. Before: 4320/3=1440=10×144 ✓. Improved: 2592/3=864=6×144 ✓. Declined: 1728/3=576=4×144 ✓.

- **Text label positions align with transform.** All 10 department name clipY values match row × sy + ty to ≤0.001 precision. All 6 X-axis label clipX values match X × sx + tx. Score labels are offset ~0.05 clip units right of the after-circle position for readability.

- **All 21 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Dumbbell charts need 3 circle groups.** One DrawItem for all "before" circles (uniform gray), then split "after" circles by direction for color coding. This is more efficient than one DrawItem per department (would need 10).

2. **Connector lines use min/max, not before/after.** Since lineAA@1 rect4 defines [x0, y0, x1, y1] and the engine draws from x0 to x1, using min_score→max_score ensures the line always spans the correct range regardless of direction.

3. **4-layer ordering adds clarity.** Grid → connectors → before → after creates a natural visual depth where the "current state" (after circles) is most prominent. Before circles peek out behind the after circles on the opposite side of the connector.

4. **Extreme aspect correction (3.68:1) is handled cleanly.** With px_per_dy=50 and px_per_dx=13.6, the X data radius must be 3.68× the Y data radius. The tessellation produces perfectly circular 7px dots despite this ratio.

5. **Mixed direction in dumbbell charts creates immediate visual narrative.** Green dots right of gray = improvement, red dots left of gray = decline. No axis labels needed to understand the pattern — the color and position encode both direction and magnitude.
