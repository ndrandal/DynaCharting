# Trial 010: Weekly Activity Heatmap

**Date:** 2026-03-12
**Goal:** 7×24 activity heatmap (168 cells) using `triGradient@1` with per-vertex color, 5-stop color ramp interpolation, and summary bars. First trial to use `triGradient@1` at high data density (1008 vertices, 6048 floats in a single DrawItem).
**Outcome:** Mathematically flawless. All 10 spot-checked cells match expected colors to 4 decimal places. The weekday work-hour pattern is immediately visible. Zero major defects.

---

## What Was Built

A 1100×750 viewport with two panes:

**Pane 1 — Heatmap (1078×548px, 73.1%):**
- **168 colored cells** (7 rows × 24 columns) as a single `triGradient@1` DrawItem with `pos2_color4` format. Each cell = 2 triangles = 6 vertices, all vertices sharing the cell's interpolated color. Total: 1008 vertices × 6 floats = 6048 floats in one buffer.
- **Data space:** X = [0, 24] (hours), Y = [0, 7] (days: 0=Monday bottom, 6=Sunday top). Cell dimensions: 0.92 × 0.92 with 0.08-unit gap between cells (~3.5px).
- **5-stop color ramp:** deep blue-purple (0%) → cool blue (25%) → teal-green (50%) → warm yellow (75%) → hot red-orange (100%). Linear interpolation between adjacent stops.
- **Activity pattern:** Deterministic formula producing realistic weekly data — high during weekday work hours (9-17), moderate commute (7-9, 17-19), low overnight and weekends. Friday peaks (93%), Sunday troughs (5-8%).
- Viewport: [-0.5, 24.5] × [-0.5, 7.5], static.

**Pane 2 — Legend + Summary (1078×170px, 22.7%):**
- **Gradient legend bar** (`triGradient@1`, pos2_color4, 24 vertices = 4 quads): horizontal strip showing the full 5-stop color ramp from x=2 to x=98.
- **3 summary bars** (`instancedRect@1`, rect4, cornerRadius 3.0):
  - Weekday average: teal (0.15, 0.65, 0.45), bar extends to x≈46.6
  - Saturday average: yellow-green (0.6, 0.7, 0.2), bar to x≈37.4
  - Sunday average: cool blue (0.15, 0.35, 0.65), bar to x≈22.6
- Viewport: [0, 110] × [0, 5], static.

Gap: 16px (2.1%). Margins: ~8px. Total: 8+548+16+170+8 = 750 ✓

Total resources: 2 panes, 3 layers, 2 transforms, 5 buffers, 5 geometries, 5 drawItems, 2 viewports = 22 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation. Without labels, the 24 hour columns and 7 day rows are unidentified. The gradient legend has no "Low"/"High" endpoints. The summary bars have no dimension names or percentages.

2. **Summary bar lengths are offset by 2 units.** All bars start at x=2 rather than x=0, but the x2 coordinate equals the score value (e.g., weekday bar goes from x=2 to x=46.64, visual length = 44.64 vs actual average ~46.6%). The 2-unit offset creates a ~2% understatement of the bar's visual length relative to the score it represents. Starting at x=0 or adjusting x2 to (2 + score) would fix the proportionality.

3. **Legend gradient occupies only 87% of the bar pane width.** The legend spans x=[2, 98] in a viewport of [0, 110], leaving the rightmost 11% empty (reserved for the "High" text label that's invisible in PNG). In PNG mode this appears as unused space to the right of the gradient.

---

## Spatial Reasoning Analysis

### Done Right

- **Color ramp interpolation is mathematically exact.** Verified 10 cells across all activity levels and days:

  | Cell | Value | Expected RGB | Actual RGB | Match |
  |------|-------|-------------|------------|-------|
  | Mon H0 | 8 | (0.1296, 0.1664, 0.4460) | (0.1296, 0.1664, 0.4460) | ✓ |
  | Mon H10 | 77 | (0.8540, 0.7100, 0.1460) | (0.8540, 0.7100, 0.1460) | ✓ |
  | Wed H14 | 76 | (0.8520, 0.7300, 0.1480) | (0.8520, 0.7300, 0.1480) | ✓ |
  | Fri H12 | 93 | (0.8860, 0.3900, 0.1140) | (0.8860, 0.3900, 0.1140) | ✓ |
  | Fri H22 | 23 | (0.1476, 0.3284, 0.6260) | (0.1476, 0.3284, 0.6260) | ✓ |
  | Sat H10 | 41 | (0.1500, 0.5420, 0.5220) | (0.1500, 0.5420, 0.5220) | ✓ |
  | Sat H20 | 61 | (0.4580, 0.6940, 0.3180) | (0.4580, 0.6940, 0.3180) | ✓ |
  | Sun H3 | 8 | (0.1296, 0.1664, 0.4460) | (0.1296, 0.1664, 0.4460) | ✓ |
  | Sun H15 | 41 | (0.1500, 0.5420, 0.5220) | (0.1500, 0.5420, 0.5220) | ✓ |
  | Thu H9 | 84 | (0.8680, 0.5700, 0.1320) | (0.8680, 0.5700, 0.1320) | ✓ |

  10/10 exact matches to 4 decimal places. The 5-stop ramp interpolation works flawlessly.

- **Grid layout is correct.** 24 unique X positions (0-23) and 7 unique Y positions (0-6) confirmed. Cell dimensions: 0.92×0.92 data units with 0.08-unit gaps. In pixels: each cell is ~39.7×71.8px with ~3.5×5.9px gaps. The grid is dense but has clearly visible separation lines.

- **Cell tessellation is correct.** Each cell follows BL→BR→TR, BL→TR→TL triangle winding. Verified on cell 0: (0,0)→(0.92,0)→(0.92,0.92), (0,0)→(0.92,0.92)→(0,0.92). All 6 vertices within each cell carry identical RGBA values.

- **Activity pattern is visually compelling.** The PNG immediately reveals: dark blue overnight (hours 0-6), transition through commute hours (7-9), warm orange/yellow work zone (9-17), cool-down through evening, with Friday showing the hottest values and Sunday the coolest. The weekday-weekend contrast is striking.

- **Layout is pixel-perfect.** Pane 1: 548px, gap: 16px, pane 2: 170px. Verified against clip-space values. Total = 750px ✓.

- **Resource efficiency is excellent.** Only 22 IDs total — the most minimal trial yet. A single triGradient@1 DrawItem renders all 168 heatmap cells. This demonstrates that per-vertex coloring in pos2_color4 format scales well to high data density.

- **All 22 IDs unique.** Century-range pattern: buffers 100-104, geometries 200-204, drawItems 300-304. No collisions.

### Done Wrong

- Nothing structurally wrong. The bar offset (x=2 start) is a minor proportionality issue, not a geometric error.

---

## Lessons for Future Trials

1. **`triGradient@1` with `pos2_color4` is ideal for heatmaps.** A single DrawItem can render hundreds of colored cells at different colors. The key: each cell's 6 vertices share the same RGBA, giving uniform color per cell. The GPU handles the large vertex buffer efficiently.

2. **5-stop color ramps need careful interpolation.** For value v in [0,1]: find the two bracketing stops, compute local t, linear interpolate each channel independently. The formula is simple but must be applied consistently to all 168 cells. This trial proves one-shot generation of 6048 correctly interpolated floats is achievable.

3. **Deterministic data patterns create convincing heatmaps.** The formula-based approach (base value per time-of-day bracket + day offset) produces realistic-looking weekly patterns without randomness. The visual immediately communicates "weekday work schedule" to any viewer.

4. **Minimal ID allocation is possible.** This trial uses only 22 IDs for a visualization with 168 data cells. The trick: pack all cells into one triGradient@1 DrawItem rather than creating per-cell drawItems. This is the most resource-efficient trial in the series.
