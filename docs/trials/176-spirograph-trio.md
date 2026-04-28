# Trial 176: Spirograph Trio

**Date:** 2026-03-22
**Goal:** 3 overlaid hypotrochoid curves on a 700x700 viewport. Parameters: (R=10,r=6,d=8), (R=10,r=3,d=5), (R=10,r=7,d=4). 200 segments each. Different colors.
**Outcome:** 3 DrawItems: red, green, blue hypotrochoids. Each curve closed. Classic Spirograph patterns. Zero defects.

---

## What Was Built

A 700x700 viewport with 3 hypotrochoid curves:
- Red: R=10, r=6, d=8 — epitrochoid-like with 4 cusps
- Green: R=10, r=3, d=5 — 7/3 pattern
- Blue: R=10, r=7, d=4 — 3-lobed curve
- x = (R-r)*cos(t) + d*cos((R-r)t/r)
- y = (R-r)*sin(t) - d*sin((R-r)t/r)

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
- Hypotrochoid parametric equations correctly implemented
- Period computed from lcm(R, r) for clean closure
- Three distinct parameter sets produce visually different patterns
