# Trial 177: Epitrochoid

**Date:** 2026-03-22
**Goal:** Epitrochoid curve (gear outside circle) on a 700x700 viewport. R=5, r=3, d=5. 200 segments. Cyan on dark background.
**Outcome:** 1 DrawItem with 200 line segments. Classic epitrochoid with 3+5/3 cusps. Cyan color. Zero defects.

---

## What Was Built

A 700x700 viewport with epitrochoid:
- x = (R+r)*cos(t) - d*cos((R+r)t/r)
- y = (R+r)*sin(t) - d*sin((R+r)t/r)
- R=5, r=3, d=5
- 200 segments, period = 2*pi*r/gcd(R,r) = 18.8496
- lineAA@1, lineWidth=2.5, cyan

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
- Epitrochoid (not hypotrochoid): R+r in formula (gear rolls outside)
- Period correctly computed for clean curve closure
- d > r produces loops (d=5 > r=3)
