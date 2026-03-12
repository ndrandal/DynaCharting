# Trial 049: Error Bar Chart

**Date:** 2026-03-12
**Goal:** 10 data points with asymmetric error bars combining vertical error lines, horizontal caps, a connecting line, and aspect-corrected value dots. Tests multi-component point alignment, grouped line rendering (10 bars + 20 caps + 9 connecting segments + 10 dots in 5 DrawItems), and asymmetric error range representation.
**Outcome:** All 10 error bars, 20 caps, 9 connecting segments, and 10 value dots match spec exactly. Dots perfectly circular at 8px. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**10 vertical error bars (1 lineAA@1 DrawItem, rect4, 10 instances):**
White, alpha 0.5, lineWidth 1.5. Each from (X, ErrorLow) to (X, ErrorHigh).

**20 cap lines (1 lineAA@1 DrawItem, rect4, 20 instances):**
White, alpha 0.5, lineWidth 1.5. Horizontal caps at ±0.2 data units from center X, at both ErrorLow and ErrorHigh for each point.

**Connecting line (1 lineAA@1 DrawItem, rect4, 9 instances):**
Blue (#3b82f6), alpha 0.4, lineWidth 1.5. Connects consecutive value positions.

**10 value dots (1 triAA@1 DrawItem, pos2_alpha, 480 vertices = 10 circles × 48 verts):**
Blue (#3b82f6), alpha 1.0. Aspect-corrected 8px circles (rx≈0.093, ry≈1.263 data units). 16 segments per circle.

| Point | X | Value | Error Low | Error High |
|-------|---|-------|-----------|------------|
| 1 | 1 | 42 | 35 | 48 |
| 2 | 2 | 55 | 49 | 63 |
| 3 | 3 | 38 | 30 | 44 |
| 4 | 4 | 67 | 58 | 72 |
| 5 | 5 | 51 | 44 | 60 |
| 6 | 6 | 73 | 65 | 80 |
| 7 | 7 | 45 | 38 | 55 |
| 8 | 8 | 62 | 53 | 68 |
| 9 | 9 | 58 | 50 | 66 |
| 10 | 10 | 70 | 62 | 78 |

**4 grid lines (1 lineAA@1 DrawItem, rect4, 4 instances):**
At Y=20, 40, 60, 80. Spanning X=[0, 11]. White, alpha 0.06, lineWidth 1.

Data space: X=[0, 11], Y=[0, 90]. Transform 50: sx=0.172727, sy=0.021111, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Error bars/caps (11) → Connecting line (12) → Value dots (13).

Total: 21 unique IDs.

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

- **All 10 error bars at correct positions.** Each vertical segment connects (X, ErrorLow) to (X, ErrorHigh). All 10/10 verified with exact coordinate matches.

- **All 20 caps at correct positions.** Bottom caps at ErrorLow Y, top caps at ErrorHigh Y, each spanning X±0.2 data units. All 20/20 verified.

- **All 9 connecting line segments correct.** Each segment connects (X_i, Value_i) to (X_{i+1}, Value_{i+1}). All 9/9 verified.

- **All 10 value dots at correct positions.** Each dot centered at (X, Value). All 10/10 verified.

- **Dots perfectly circular at 8px.** Despite 13.6:1 aspect ratio (px_per_dx=86.36, px_per_dy=6.33), aspect correction produces exact 8.00px radius with ≤0.0001px spread for all 10 dots.

- **Asymmetric error ranges correctly represented.** Each point has different upper and lower error extents. For example, P7 has ErrorLow=38 (7 below value 45) vs ErrorHigh=55 (10 above value 45). The bars visually show this asymmetry.

- **Transform math is exact.** sx=1.9/11=0.172727 and sy=1.9/90=0.021111 correctly map the data space to clip[−0.95, 0.95].

- **Layer ordering is correct.** Grid (10, back) → error bars/caps (11) → connecting line (12) → dots (13, front). Dots draw on top of both the connecting line and error bars.

- **Efficient grouping.** 5 DrawItems for a 5-component chart with 10 data points: grid (1), error bars (1), caps (1), connecting line (1), dots (1). All same-styled elements grouped.

- **Grid lines at correct intervals.** Y=20, 40, 60, 80 spanning full X range.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All buffer sizes match vertex counts.** 5/5 geometries verified.

- **All 21 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Error bar charts combine 4 component types per data point.** Vertical bars, horizontal caps, value dots, and connecting lines. All can be grouped by type into single DrawItems since each type shares the same style properties.

2. **Asymmetric errors need separate ErrorLow/ErrorHigh values.** Symmetric errors (±e) are simpler but less realistic. Asymmetric errors (different upper/lower bounds) are common in experimental data and require explicit low/high coordinates.

3. **Cap width should be smaller than data point spacing.** At ±0.2 data units with 1.0 spacing, caps are clearly visible without overlapping adjacent points.

4. **Connecting line at reduced alpha preserves error bar readability.** Alpha 0.4 for the connecting line vs alpha 0.5 for error bars and alpha 1.0 for dots creates a visual hierarchy: dots (primary) > error bars (context) > connecting line (trend).

5. **All 10 dots in a single triAA@1 DrawItem is efficient.** 480 vertices (10 circles × 48 each) in one DrawItem. Since all dots share the same color and alpha, no per-point DrawItems needed.
