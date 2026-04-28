# Trial 276: Flight Instruments

**Date:** 2026-03-22
**Goal:** 2-pane flight instrument panel. Left: artificial horizon (sky/ground split, horizon line, pitch ladder, aircraft symbol). Right: altimeter (circular gauge, tick marks, needle at 5500ft).
**Outcome:** 10 DrawItems across 2 panes (6 layers). Horizon shows level flight; altimeter reads ~5500ft. 38 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x400. Two panes side-by-side.

**Pane 1 (Artificial Horizon) — 5 DrawItems, 3 layers:**

| DrawItem | Layer | Element | Pipeline | Detail |
|----------|-------|---------|----------|--------|
| 102 | 10 | Sky | triSolid@1 | 2 tris, blue |
| 105 | 10 | Ground | triSolid@1 | 2 tris, brown |
| 108 | 11 | Horizon line | lineAA@1 | 1 seg, white, lw=2 |
| 111 | 11 | Pitch ladder | lineAA@1 | 4 segs, white |
| 114 | 12 | Aircraft symbol | lineAA@1 | 3 segs, yellow, lw=2.5 |

**Pane 2 (Altimeter) — 5 DrawItems, 3 layers:**

| DrawItem | Layer | Element | Pipeline | Detail |
|----------|-------|---------|----------|--------|
| 117 | 20 | Dial circle | lineAA@1 | 48 segs |
| 120 | 20 | Major ticks | lineAA@1 | 12 segs |
| 123 | 20 | Minor ticks | lineAA@1 | 48 segs |
| 126 | 21 | Needle | lineAA@1 | 1 seg, red |
| 129 | 22 | Center dot | triSolid@1 | 12 tris |

Total: 38 unique IDs (2 panes, 6 layers, 10 buffers, 10 geometries, 10 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Sky/ground split at y=0 creates artificial horizon.** Blue above, brown below — immediately recognizable as an attitude indicator.
- **Pitch ladder marks at +-10 and +-20 degrees.** Shorter marks for 10-degree, longer for 20-degree intervals.
- **Yellow aircraft symbol overlays the horizon.** Wing lines and center dot provide the fixed reference.
- **Altimeter needle at 165 degrees.** Maps to ~5500ft on a 12-position circular scale (each 30deg = 1000ft).
- **Major/minor tick differentiation.** Major ticks (longer, brighter) every 30deg, minor ticks every 6deg.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Two-pane side-by-side layout.** clipXMin/clipXMax split the viewport into left and right instruments.
2. **Circular gauge construction.** circle_outline + radial ticks + needle from center = complete dial.
