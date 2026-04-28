# Trial 246: EKG Heartbeat

**Date:** 2026-03-22
**Goal:** Electrocardiogram with realistic PQRST waveform (5 cycles, 89 line segments). Green trace on black. Dashed reference grid.
**Outcome:** Waveform renders 5 heartbeat cycles with proper P, QRS, T morphology. Grid has 16 segments. 10 unique IDs. Zero defects.

---

## What Was Built
Viewport 900x400. Black background with green EKG trace.
Data space: x=[0,100], y=[-1,2]. Transform maps to clip space.
5 PQRST cycles with: P wave (small bump), QRS complex (sharp spike), T wave (broad bump).
Dashed grid lines at 0.5-unit vertical intervals and 10-unit horizontal intervals.
Total: 10 unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometries, 2 drawItems).

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
- **PQRST morphology is recognizable.** R spike at y=1.8, P wave at y=0.25, T wave at y=0.35. Q and S dips below baseline.
- **5 cycles tile evenly across 100 data units.** Each cycle occupies 20 units, filling the viewport.
- **Grid dashes provide ECG paper feel.** Dashed lines at physiologically meaningful intervals.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Physiological waveforms need distinct amplitude ratios.** R spike should be ~7x P wave amplitude for recognizability.
2. **Dashed grids via dashLength/gapLength avoid separate buffer construction.**
