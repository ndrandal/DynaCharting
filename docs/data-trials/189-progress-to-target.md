# Data Trial 189: Progress to Target
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Progress bar visualization — 8 departments with background track and colored fill. Classic UI pattern using instancedRect@1.
**Goal:** Department revenue as percentage of target (target = actual * 1.2).
**Outcome:** 8 progress bars with background tracks. All at ~83.3% (since target = 120% of actual). 10 unique IDs. Zero defects.

---
## What Was Built

Viewport 900x500. Two layers: dark background tracks (full width) + blue fill bars (partial).
Rounded corners (4px) for polished UI appearance.
Percentage labels on the right.

---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---
## Data Insights
- All departments show ~83.3% progress (1/1.2 = 83.3%) — expected given the synthetic target formula.
- In a real scenario, targets would vary, creating more visual differentiation.

---
## Lessons
1. Progress bars are a clean pattern: two overlapping instancedRect@1 DrawItems (background + fill).
2. cornerRadius on both layers creates a polished pill-shaped appearance.
