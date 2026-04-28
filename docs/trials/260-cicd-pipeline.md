# Trial 260: CI/CD Pipeline

**Date:** 2026-03-22
**Goal:** 5-stage CI/CD pipeline (Build, Test, Lint, Deploy, Monitor) with status colors (green/yellow/gray), arrows, and progress bar.
**Outcome:** 5 stages (2 green, 1 yellow, 2 gray), 4 arrows, progress bar at 45%. 22 unique IDs. Zero defects.

---

## What Was Built
Viewport 900x350. Dark background. 5 pipeline stage boxes in horizontal row.
Build + Test = green (passed). Lint = yellow (warning). Deploy + Monitor = gray (pending).
Arrows between stages with triangle arrowheads. Progress bar below shows 45% completion.
Total: 22 unique IDs (1 pane, 3 layers, 6 buffers, 6 geometries, 6 drawItems).

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
- **5 stages evenly spaced horizontally.** Centers at x=-0.7, -0.35, 0, 0.35, 0.7.
- **Status colors instantly readable.** Green=done, yellow=warning, gray=pending.
- **Progress bar fill proportional to pipeline completion.** 2/5 passed + partial = 45%.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Pipeline diagrams are linear node+arrow chains.** Horizontal layout most natural for L-to-R reading.
2. **Status grouping by color: separate drawItems per status.**
